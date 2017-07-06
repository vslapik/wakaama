mockData = {
    "device_id1": {
        "descr": "Outside sensor #1",
        "sensors" : [{
            "id": "/0/0/1",
            "type": "temp",
            "value": "23,5"
        },{
            "id": "/0/1/1",
            "type": "humid",
            "value": "25"
        }]
    },
    "device_id2": {
        "descr": "Outside sensor #2",
        "sensors" : [{
            "id": "/1/0/1",
            "type": "bulb",
            "value": "ON"
        }]
    }
}

var errorMsgs = {
	"getDeviceListErr": "Error when loading device list!",
	"getSensorsListErr": "Error when loading sensors list!",
	"getSensorValueErr": "Error when loading sensor value!",
	"getSensorStatErr": "Error when loading sensor statistics!"
}

function hideOverlay() {
    $(".loading-overlay").hide();
}

function showErrorOnOverlay(errorMsg) {
    var overlay = $(".loading-overlay");
    var spinner = $(".spinner", overlay);
    var errorMsgElem = $(".loading-error-msg", overlay);

    spinner.hide();

    errorMsgElem.text(errorMsg);
    errorMsgElem.show();
}

function getDevicesListURL() {
	return "devices/list";
}

function getSensorsListURL(devName) {
    return "devices/" + devName + "/sensors/list";   
}

function getSensorValueURL(devName, sensName) {
    return "devices/" + devName + "/sensors/" + sensName + "/value";
}

function getSensorStatURL(devName, sensName) {
    return "devices/" + devName + "/sensors/" + sensName + "/stat";
}