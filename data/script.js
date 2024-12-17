var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener("load", onload);

function onload(event) {
  initWebSocket();
  initButton();
}

function getReadings() {
  websocket.send("getReadings");
}

function initWebSocket() {
  console.log("Trying to open a websocket connection...");
  websocket = new WebSocket(gateway);
  websocket.onopen = onOpen;
  websocket.onclose = onClose;
  websocket.onmessage = onMessage;
}

function onOpen(event) {
  console.log("Connection closed");
  getReadings();
}

function onClose(event) {
  console.log("Connection closed");
  setTimeout(initWebSocket, 2000);
}

function onMessage(event) {
  console.log(event.data);
  var myObj = JSON.parse(event.data);
  var keys = Object.keys(myObj);

  for (var i = 0; i < keys.length; i++) {
    var key = keys[i];
    if (key === "water_pump") {
      var checkbox = document.getElementById("water_pump_checkbox");
      if (myObj[key] === "1") {
        checkbox.checked = true;
      } else {
        checkbox.checked = false;
      }
    } else if (key === "soil_moisture") {
      if (myObj[key] === "0") {
        document.getElementById(key).innerHTML = "Wet";
      } else {
        document.getElementById(key).innerHTML = "Dry";
      }
    } else {
      document.getElementById(key).innerHTML = myObj[key];
    }
  }
}

function initButton() {
  document
    .getElementById("water_pump_checkbox")
    .addEventListener("change", toggle);
}

function toggle() {
  console.log("Send toggle");
  websocket.send("toggle");
}
