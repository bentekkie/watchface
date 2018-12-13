Pebble.addEventListener('ready', () => {
    getWeather();
    navigator.getBattery().then(battery => {
        function updateAllBatteryInfo() {
            updateLevelInfo();
            updateChargingStatus();
        }
        updateAllBatteryInfo();
        function updateLevelInfo() {
            Pebble.sendAppMessage({
                "PHONE_BATTERY_PERCENTAGE":battery.level*100,
            })
        }
        function updateChargingStatus(){
            Pebble.sendAppMessage({
                "PHONE_CHARGING_STATUS":battery.charging
            })
        }
        battery.addEventListener('levelchange', updateLevelInfo);
        battery.addEventListener('chargingchange',updateChargingStatus)
    })
})
var xhrRequest = function (url, type, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function () {
        callback(this.responseText);
    };
    xhr.open(type, url);
    xhr.send();
};

var OWMAPIKEY = "e7854b6d0065d1fda008ff6389b8dc42"
var validWeatherKeys = ['01d','01n',
'02d','02n',
'03d','03n',
'04d','04n',
'09d','09n',
'10d','10n',
'11d','11n',
'13d','13n',
'50d','50n']

function locationSuccess(pos) {
    let url = `http://api.openweathermap.org/data/2.5/weather?lat=${pos.coords.latitude}&lon=${pos.coords.longitude}&APPID=${OWMAPIKEY}`
    xhrRequest(url,'GET',resp => {
        let json = JSON.parse(resp)
        Pebble.sendAppMessage({
            "TEMPERATURE":Math.round(json.main.temp - 273.15),
            "WEATHER_ICON_KEY":validWeatherKeys.indexOf(json.weather[0].icon)
        })
    })
}

function locationError(err) {
    console.log('Error requesting location!');
}
  

function getWeather() {
    navigator.geolocation.getCurrentPosition(
        locationSuccess,
        locationError,
        {timeout: 15000, maximumAge: 60000, enableHighAccuracy:true}
      );
}


Pebble.addEventListener('appmessage',getWeather);