const loginElement = document.querySelector('#login-form');
const contentElement = document.querySelector("#content-sign-in");
const userDetailsElement = document.querySelector('#user-details');
const authBarElement = document.querySelector("#authentication-bar");

// Elements for GPIO states and sensor readings
const stateElement1 = document.getElementById("state1");
const stateElement2 = document.getElementById("state2");
const stateElement3 = document.getElementById("state3");
const tempElement = document.getElementById("temp");
const humElement = document.getElementById("hum");
const presElement = document.getElementById("pres");

// Button Elements
const btn1On = document.getElementById('btn1On');
const btn1Off = document.getElementById('btn1Off');
const btn2On = document.getElementById('btn2On');
const btn2Off = document.getElementById('btn2Off');
const btn3On = document.getElementById('btn3On');
const btn3Off = document.getElementById('btn3Off');

// Database path for GPIO states
var dbPathOutput1 = 'button_state/button1';
var dbPathOutput2 = 'button_state/button2';
var dbPathOutput3 = 'button_state/button3';
// var dbPathTemp = 'sensor_data/temperature';
// var dbPathHum = 'sensor_data/humidity';
// var dbPathPres = 'Light_data/light_intensity';

// Database references
var dbRefOutput1 = firebase.database().ref().child(dbPathOutput1);
var dbRefOutput2 = firebase.database().ref().child(dbPathOutput2);
var dbRefOutput3 = firebase.database().ref().child(dbPathOutput3);

// Firebase Realtime Database paths for sensors
// var dbPathTemp = firebase.database().ref().child(dbPathTemp);
// var dbPathHum  = firebase.database().ref().child(dbPathHum);
// var dbPathPres = firebase.database().ref().child(dbPathPres);
var dbRefTemp = firebase.database().ref('sensor_data/temperature');
var dbRefHum  = firebase.database().ref('sensor_data/humidity');
var dbRefPres = firebase.database().ref('Light_data/light_intensity');



// MANAGE LOGIN/LOGOUT UI
const setupUI = (user) => {
  if (user) {
    //toggle UI elements
    loginElement.style.display = 'none';
    contentElement.style.display = 'block';
    authBarElement.style.display ='block';
    userDetailsElement.style.display ='block';
    userDetailsElement.innerHTML = user.email;
    // get user UID to get data from database
    var uid = user.uid;
    console.log(uid);

    //Update states depending on the database value
    dbRefOutput1.on('value', snap => {
        if(snap.val()==1) {
            stateElement1.innerText="ON";
        }
        else{
            stateElement1.innerText="OFF";
        }
    });
    dbRefOutput2.on('value', snap => {
        if(snap.val()==1) {
            stateElement2.innerText="ON";
        }
        else{
            stateElement2.innerText="OFF";
        }
    });
    dbRefOutput3.on('value', snap => {
        if(snap.val()==1) {
            stateElement3.innerText="ON";
        }
        else{
            stateElement3.innerText="OFF";
        }
    });

    // Update database upon button click
    btn1On.onclick = () => dbRefOutput1.set(1);
    btn1Off.onclick = () => dbRefOutput1.set(0);
    btn2On.onclick = () => dbRefOutput2.set(1);
    btn2Off.onclick = () => dbRefOutput2.set(0);
    btn3On.onclick = () => dbRefOutput3.set(1);
    btn3Off.onclick = () => dbRefOutput3.set(0);

    // Update sensor values from Firebase
    dbRefTemp.on('value', snap => {
        const tempData = snap.val();
        console.log("Temperature data:", tempData); 
        if (tempData !== null) {
          // tempElement.innerText = `${tempData} °C`;
          tempElement.innerText = `${parseFloat(tempData).toFixed(2)} °C`;
        } else {
          tempElement.innerText = 'N/A';
          // console.error("Temperature data is null or undefined.");
        }
      });
  
      dbRefHum.on('value', snap => {
        const humData = snap.val();
        console.log("Humidity data:", humData); // Debugging
        if (humData !== null) {
          // humElement.innerText = `${humData} %`;
          humElement.innerText = `${parseFloat(humData).toFixed(2)} %`;
        } else {
          humElement.innerText = 'N/A';
          // console.error("Humidity data is null or undefined.");
        }
      });
  
      dbRefPres.on('value', snap => {
        const presData = snap.val();
        console.log("Pressure data:", presData); // Debugging
        if (presData !== null) {
          // presElement.innerText = `${presData} hPa`;
          presElement.innerText = `${parseFloat(presData).toFixed(2)} lux`;
        } else {
          presElement.innerText = 'N/A';
          // console.error("Pressure data is null or undefined.");
        }
      });
  // if user is logged out
  } else {
    // toggle UI elements
    loginElement.style.display = 'block';
    authBarElement.style.display = 'none';
    userDetailsElement.style.display = 'none';
    contentElement.style.display = 'none';
  }
};

// Firebase authentication state change listener
firebase.auth().onAuthStateChanged(setupUI);

