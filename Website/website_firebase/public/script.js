// Firebase configuration (replace with your own Firebase project config)
const firebaseConfig = {
	apiKey: "AIzaSyAXR4tVGcGuzKEfXQ5RT8dBG7M53VAss7k",
	authDomain: "fir-getstart-9f542.firebaseapp.com",
	databaseURL: "https://fir-getstart-9f542-default-rtdb.firebaseio.com",
	projectId: "fir-getstart-9f542",
	storageBucket: "fir-getstart-9f542.firebasestorage.app",
	messagingSenderId: "631927582694",
	appId: "1:631927582694:web:791443aab4b6270da7f027",
	measurementId: "G-NENJ5J3Y6W"
  };
  
  // Initialize Firebase
  firebase.initializeApp(firebaseConfig);
  
  $(document).ready(function () {
	var database = firebase.database();
	var Led1Status;
  
	// Fetch data from Firebase in real-time
	database.ref().on("value", function (snap) {
	  // Handle LED status
	  Led1Status = snap.val().Led1Status;
	  if (Led1Status == 1) {
		document.getElementById("unact").style.display = "none";
		document.getElementById("act").style.display = "block";
	  } else {
		document.getElementById("unact").style.display = "block";
		document.getElementById("act").style.display = "none";
	  }
  
	  // Display temperature and humidity from SENSOR node
	  var sensorData = snap.val().SENSOR || {};
	  var temperature = sensorData.temperature || "--";
	  var humidity = sensorData.humidity || "--";
	  document.getElementById("temperature").innerText = temperature + " °C";
	  document.getElementById("humidity").innerText = humidity + " %";
	});
  
	// Toggle LED status on button click
	$(".toggle-btn").click(function () {
	  var firebaseRef = database.ref().child("Led1Status");
  
	  if (Led1Status == 1) {
		firebaseRef.set(0);
		Led1Status = 0;
	  } else {
		firebaseRef.set(1);
		Led1Status = 1;
	  }
	});
  });