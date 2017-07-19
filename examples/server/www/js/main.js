$(function () {
    Cards = [];
    devicesData = {}
 
    var Rules = {
        "3303": {
            valueID: ".5700",
            units: "celsium",
            type: "temp"
        },
        "3304": {
            valueID: ".5700",
            units: "percents",
            type: "humid"
        },
        "1024": {
            valueID: ".1",
            units: "test units",
            type: "type"
        }
    }

    function getSensorsListForDevices(data) {
        if (data.data.length == 0) {
            showErrorOnOverlay(errorMsgs["deviceListEmpty"]);
            
            return $.Deferred().reject(errorMsgs["deviceListEmpty"]);
        }

        var devProms = data.data.map(function(devName) {
            return $.getJSON(getSensorsListURL(devName))
                .then(function(data) {
                    return {
                        devName: devName,
                        data: data.data
                    }
                });
        });
 
        return $.when.apply($, devProms);    
    }

    function getValuesForSensorsList() {
        var result = Array.prototype.slice.call(arguments);
 
        var devicesProms = result.map(function(device) {
            var filteredSensList = {};

            device.data.forEach(function(sensName) {
                var devID = sensName.split('.')[1];

                if (Rules.hasOwnProperty(devID)) {
                    filteredSensList[sensName] = sensName + Rules[devID].valueID;    
                }    
               
            });

            var sensorsProms = Object.keys(filteredSensList).map(function(sensor) {
                return $.getJSON(getSensorValueURL(device.devName, filteredSensList[sensor]))
                    .then(function(data) {
                        return {
                            devName: device.devName,
                            sensor: filteredSensList[sensor],
                            value: data.data
                        }
                    });
            });

            return $.when.apply($, sensorsProms);    
        });

        return $.when.apply($, devicesProms);
    }

    function createDataModelAndCards(data) {
        var result = Array.prototype.slice.call(arguments);

        if (!Array.isArray(result[0])) {
            var arr = [[]];

            result.forEach((e) => arr[0].push(e));
            result = arr;
        }    

        result.forEach(function(devSensList) {
            var devName = devSensList[0].devName;

            devicesData[devName] = {};
            devicesData[devName].descr = devName;
            devicesData[devName].sensors = [];

            devSensList.forEach(function(sensor) {
                devicesData[devName].sensors.push({
                    id: sensor.sensor,
                    type: sensor.value.type,
                    value: sensor.value.value
                })
                
            })
        });
        
        for (device in devicesData) {
            devicesData[device].sensors.forEach(function(sensorData, index) {
                var newCard = new Card(devicesData[device].descr, sensorData, Rules);
                
                newCard.addToPane(devicesData[device].descr, sensorData)
                
                Cards.push(newCard)

            });
        }
    }
 
    $.getJSON(getDevicesListURL())
        .then(getSensorsListForDevices, function() {
            showErrorOnOverlay(errorMsgs["getDeviceListErr"]);
        })
        .then(getValuesForSensorsList, function() {
            showErrorOnOverlay(errorMsgs["getSensorsListErr"]);
        })
        .then(createDataModelAndCards, function() {
            showErrorOnOverlay(errorMsgs["getSensorValueErr"]);
        })
        .then(hideOverlay)
});