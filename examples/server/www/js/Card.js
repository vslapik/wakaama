function processOpenCardEvent(e) {
	var that = this;

    var target = e.target;
    var cardWrapper = $(target).closest('.i-card-wrapper');
    var detailsCard = $(".i-card.i-details-card").parent();

    detailsCard.remove();

    var contaiderWidth = $(target).closest(".device-panel").width();
    var cardWidth = cardWrapper.width();
    var cardsInContainer = Math.floor(contaiderWidth / cardWidth);
    var siblToRowEnd = cardsInContainer - (cardWrapper.index() % cardsInContainer) - 1;

    var nextSiblings = cardWrapper.nextAll();
    var toInserAfter;

    if (nextSiblings.length < siblToRowEnd) {
        toInserAfter = nextSiblings.last()
    } else {
        toInserAfter = (siblToRowEnd == 0) ? cardWrapper : cardWrapper.nextAll().eq(siblToRowEnd - 1);
    }
   
    toInserAfter.after(
        "<div class=\"col-12 col-md-12 col-sm-12\" style=\"display: none;\"> \
            <div class=\"i-card i-details-card p-3\"></div> \
        </div>");

    detailsCard = $(".i-card.i-details-card");


    $(".i-card-details-open").not(cardWrapper).addClass("i-card-details-hide");
    $(".i-card-details-open").not(cardWrapper).removeClass("i-card-details-open");
   
    if (cardWrapper.hasClass("i-card-details-hide")) {
        detailsCard.parent().show();
        cardWrapper.removeClass("i-card-details-hide");
        cardWrapper.addClass("i-card-details-open");

        if (detailsCard.is(':visible')) {
            detailsCard.empty();
        }


        $.getJSON(getSensorStatURL(that.devName, that.sensorID))
        	.then(function(res) {
        		var sensStat = res.data.map(function(el) {
        			var timestamp = Object.keys(el)[0];

        			return {
        				timestamp: timestamp,
        				value: el[timestamp]
        			}
        		})

        		addChartToDetailsCard(detailsCard, {
		            deviceName: that.devName,
		            measuredValue: that.sensorType,
		            values: sensStat
		        });
        	});
    } else {
        detailsCard.parent().hide();
        cardWrapper.removeClass("i-card-details-open");
        cardWrapper.addClass("i-card-details-hide");
    }
}

function getNewCardHeading(text) {
    var headingWrap = $('<div/>', { class: 'i-card-heading'});
    var headingMask = $('<div/>', { class: 'i-card-heading-image-mask pt-2'});
    var headingText = $('<h1/>', {
        class: 'h4 i-card-heading-text',
        text: text
    });

    headingText.appendTo(headingMask);
    headingMask.appendTo(headingWrap);

    return headingWrap;
}

function getNewCardLogo(type) {
    var logoWrap = $('<div/>', { class: 'i-card-logo'});
    var logoContentWrap = $('<div/>', { class: 'i-card-logo-content-wrap'});
    var logoBg = $('<div/>', { class: 'i-card-logo-background'});
    var logoIcon;

    if (type == 'temp' || type == 'type') {
        logoIcon = $('<i/>', { class: 'fa fa-thermometer-three-quarters i-card-logo-icon'});
    } else if (type == 'humid') {
        logoIcon = $('<i/>', { class: 'fa fa-tint i-card-logo-icon'});
    } else if (type == 'bulb') {
        logoWrap.addClass('flip-container');
        logoIcon = $('\
            <div class="flip-item"> \
                <figure class="front i-card-side"> \
                    <div class="i-card-logo-background"></div> \
                    <i class="fa fa-lightbulb-o i-card-logo-icon"></i> \
                </figure> \
                <figure class="back i-card-side"> \
                    <div class="i-card-logo-background"></div> \
                    <i class="fa fa-lightbulb-o i-card-logo-icon"></i> \
                </figure> \
            </div>'
        );

        logoIcon.on('click', function (e) {
            var target = e.target;
            $(target).closest('.flip-item').toggleClass('flipped');
            var valueTextElem = $(target).closest('.i-card').find(".sensor-value h2");
            if (valueTextElem.text() == "ON") {
                valueTextElem.text("OFF");
            } else {
                valueTextElem.text("ON");
            }
        });
    }


    logoBg.appendTo(logoContentWrap);
    logoIcon.appendTo(logoContentWrap);
    logoContentWrap.appendTo(logoWrap);

    return logoWrap;
}

function getNewCardContent(value, type) {
    contentWrap = $('<div/>', { class: 'i-card-content'});
    valueWrap = $('<div/>', { class: 'sensor-value'});
    contentValue = $('<h2/>', { text: value});
    contentUnits = $('<h3/>');

    if (type == 'temp' || type == 'type') {
        contentUnits.text('celsium');
    } else if (type == 'humid') {
        contentUnits.text('percents');
    } else if (type == 'bulb') {
        contentUnits.text('power');
    }

    contentValue.appendTo(valueWrap);
    contentUnits.appendTo(valueWrap);
    valueWrap.appendTo(contentWrap);

    return contentWrap;
}

function addCardToPane(deviceDescr, sensorData) {
	var that = this;
    cardsContainer = $('.device-panel .row');

    this.cardWrap = $('<div/>', { class: 'col-12 col-lg-3 col-md-4 col-sm-6 i-card-wrapper'});
    this.cardElem = $('<div/>', { class: 'i-card'});

    if (this.sensorType != 'bulb') {
        this.cardWrap.addClass('i-card-details-hide');
        this.cardElem.addClass('openedable');
        this.cardElem.on('click', function(e) {
        	that.processOpenCardEvent(e);
        });
    }

    headingElem = getNewCardHeading(this.sensorID);
    logoItem = getNewCardLogo(this.sensorType);
    contentItem = getNewCardContent(this.sensorValue, this.sensorType);

    headingElem.appendTo(this.cardElem);
    logoItem.appendTo(this.cardElem);
    contentItem.appendTo(this.cardElem);

    this.cardElem.appendTo(this.cardWrap);

    this.cardWrap.appendTo(cardsContainer);
}

function addChartToDetailsCard(cardElement, data) {
    var chartCanvas = $('<canvas/>', { class: 'param-chart'});
    chartCanvas.appendTo(cardElement);

    var myChart = new Chart(chartCanvas.get(), {
        type: 'line',
        data: {
            labels: data.values.map((el) => el.timestamp),
            datasets: [
                {
                    label: data.measuredValue,
                    fill: true,
                    lineTension: 0.1,
                    backgroundColor: "rgba(67,86,104,0.4)",
                    borderColor: "rgba(67,86,104,1)",
                    borderCapStyle: 'butt',
                    borderDash: [],
                    borderDashOffset: 0.0,
                    borderJoinStyle: 'miter',
                    pointBorderColor: "rgba(67,86,104,1)",
                    pointBackgroundColor: "#fff",
                    pointBorderWidth: 1,
                    pointHoverRadius: 5,
                    pointHoverBackgroundColor: "rgba(67,86,104,1)",
                    pointHoverBorderColor: "rgba(220,220,220,1)",
                    pointHoverBorderWidth: 2,
                    pointRadius: 5,
                    pointHitRadius: 10,
                    data: data.values.map((el) => el.value),
                    spanGaps: false,
                }
            ]
        },
        options: {
            scales: {
                yAxes: [{
                    ticks: {
                        beginAtZero:true
                    }
                }]
            },
            responsive: true,
            maintainAspectRatio: false
        }
    });
}

function Card(devDescr, sensData) {
    this.devName = devDescr
    this.sensorID = sensData.id;
    this.sensorValue = sensData.value;
    this.sensorType = sensData.type;

    this.addToPane = addCardToPane;
    this.addChart = addChartToDetailsCard;

    this.processOpenCardEvent = processOpenCardEvent;

    return this;
}