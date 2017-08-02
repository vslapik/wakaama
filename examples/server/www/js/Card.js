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
    var toInsertAfter;

    if (nextSiblings.length > 0 && nextSiblings.length < siblToRowEnd) {
        toInsertAfter = nextSiblings.last()
    } else {
        toInsertAfter = (siblToRowEnd == 0 || nextSiblings.length == 0) ? cardWrapper : cardWrapper.nextAll().eq(siblToRowEnd - 1);
    }
   
    toInsertAfter.after(
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
                        timestamp: timestamp*1000,
                        /*timestamp: new Date(timestamp*1000),*/
                        value: Number(el[timestamp])
                    }
                })

                that.sensStat = sensStat;

                that.addChart(detailsCard)
            });
    } else {
        detailsCard.parent().hide();
        cardWrapper.removeClass("i-card-details-open");
        cardWrapper.addClass("i-card-details-hide");
    }
}

function addHighchartToCard(cardElement, chartData) {
    this.canvasWrapper = $('<div/>', { class: 'chart-wrapper'});
    this.chartElem = $('<div/>', { class: 'param-chart', id: 'chart_container'});
    this.timeScaleBtns = $(' \
        <div class="btn-group" role="group" aria-label="..."> \
            <button type="button" class="btn btn-secondary time-scale-btn" value="day">Day</button> \
            <button type="button" class="btn btn-secondary time-scale-btn" value="week">Week</button> \
            <button type="button" class="btn btn-secondary time-scale-btn" value="month">Month</button> \
        </div>');


    this.chartElem.appendTo(this.canvasWrapper);
    this.canvasWrapper.appendTo(cardElement);

    Highcharts.chart('chart_container', {
        chart: {
            zoomType: 'x'
        },
        title: {
            text: getDeviceTypeStr(this.configObj.type) + ' sensor statistics'
        },
        subtitle: {
            text: document.ontouchstart === undefined ?
                    'Click and drag in the plot area to zoom in' : 'Pinch the chart to zoom in'
        },
        xAxis: {
            type: 'datetime'
        },
        yAxis: {
            title: {
                text: 'Sensor value'
            }
        },
        legend: {
            enabled: false
        },
        plotOptions: {
            area: {
                fillColor: {
                    linearGradient: {
                        x1: 0,
                        y1: 0,
                        x2: 0,
                        y2: 1
                    },
                    stops: [
                        [0, Highcharts.getOptions().colors[0]],
                        [1, Highcharts.Color(Highcharts.getOptions().colors[0]).setOpacity(0).get('rgba')]
                    ]
                },
                marker: {
                    radius: 2
                },
                lineWidth: 1,
                states: {
                    hover: {
                        lineWidth: 1
                    }
                },
                threshold: null
            }
        },

        series: [{
            type: 'area',
            name: 'Sensor measurements',
            data: this.sensStat.map((el) => ([el.timestamp, el.value]))
        }]
    });
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

    if (type == 'temp') {
        logoIcon = $('<i/>', { class: 'fa fa-thermometer-three-quarters i-card-logo-icon'});
    } else if (type == 'type') {
        logoIcon = $('<i/>', { class: 'fa fa-question i-card-logo-icon'});
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

function getNewCardContent(value, units) {
    contentWrap = $('<div/>', { class: 'i-card-content'});
    valueWrap = $('<div/>', { class: 'sensor-value'});
    contentValue = $('<h2/>', { text: value});
    contentUnits = $('<h3/>');

    contentUnits.text(units);

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
        this.cardElem.on('click', processOpenCardEvent.bind(this));
    }

    headingElem = getNewCardHeading(this.sensorID);
    logoItem = getNewCardLogo(this.configObj.type);
    contentItem = getNewCardContent(this.sensorValue, this.configObj.units);

    headingElem.appendTo(this.cardElem);
    logoItem.appendTo(this.cardElem);
    contentItem.appendTo(this.cardElem);

    this.cardElem.appendTo(this.cardWrap);

    this.cardWrap.appendTo(cardsContainer);
}

function Card(devDescr, sensData, Rules) {
    this.devName = devDescr
    this.sensorID = sensData.id;
    this.sensorValue = sensData.value;

    this.addToPane = addCardToPane;
    this.addChart = addHighchartToCard/*addChartToCard*/;

    this.processOpenCardEvent = processOpenCardEvent;

    if (Rules[this.sensorID.split('.')[1]] != undefined) {
        this.configObj = Rules[this.sensorID.split('.')[1]];
    }
  
    return this;
}
