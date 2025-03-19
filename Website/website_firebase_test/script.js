// Firebase configuration (replace with your own Firebase project config)
// const firebaseConfig = {
// 	apiKey: "AIzaSyAXR4tVGcGuzKEfXQ5RT8dBG7M53VAss7k",
// 	authDomain: "fir-getstart-9f542.firebaseapp.com",
// 	databaseURL: "https://fir-getstart-9f542-default-rtdb.firebaseio.com",
// 	projectId: "fir-getstart-9f542",
// 	storageBucket: "fir-getstart-9f542.firebasestorage.app",
// 	messagingSenderId: "631927582694",
// 	appId: "1:631927582694:web:791443aab4b6270da7f027",
// 	measurementId: "G-NENJ5J3Y6W"
//   };
  // Firebase Configuration
  const firebaseConfig = {
	apiKey: "AIzaSyBsCZsM6FFKctzF01vCKqtIZQTFEsposmg",
	authDomain: "web-firebase-488b6.firebaseapp.com",
	databaseURL: "https://web-firebase-488b6-default-rtdb.firebaseio.com",
	projectId: "web-firebase-488b6",
	storageBucket: "web-firebase-488b6.firebasestorage.app",
	messagingSenderId: "420821034815",
	appId: "1:420821034815:web:b999a100498436e91d558a",
	measurementId: "G-YPJZ00ZS7K"
  };
  
  // Initialize Firebase
  firebase.initializeApp(firebaseConfig);
  
  const database = firebase.database();
  
  // Elements
const mainScreen = document.getElementById("main-screen");
const detailScreen = document.getElementById("room-detail-screen");
const backBtn = document.getElementById("back-btn");
const roomName = document.getElementById("room-name");
const roomTemperature = document.getElementById("room-temperature");
const roomHumidity = document.getElementById("room-humidity");
const chartCanvas = document.getElementById("electricity-chart");

// Room Data (Mock Data)
const roomData = {
  "Living Room": { temperature: 22, humidity: 40 },
  "Bedroom": { temperature: 20, humidity: 35 },
  "Kitchen": { temperature: 24, humidity: 50 },
};

// Chart instance
let electricityChart;

// Navigation
document.querySelectorAll(".room-card").forEach((card) => {
  card.addEventListener("click", () => {
    const room = card.dataset.room;
    roomName.textContent = room;
    roomTemperature.textContent = `${roomData[room].temperature} °C`;
    roomHumidity.textContent = `${roomData[room].humidity} %`;

    mainScreen.style.display = "none";
    detailScreen.style.display = "block";

    // Initialize chart with real-time data
    initChart();
  });
});

backBtn.addEventListener("click", () => {
  detailScreen.style.display = "none";
  mainScreen.style.display = "block";

  // Destroy the chart to reset it for other rooms
  if (electricityChart) {
    electricityChart.destroy();
  }
});

// Toggle Buttons
document.querySelectorAll(".toggle-btn").forEach((btn) => {
  btn.addEventListener("click", () => {
    btn.classList.toggle("active");
  });
});

// Initialize Chart
function initChart() {
  const ctx = chartCanvas.getContext("2d");
  const labels = Array.from({ length: 10 }, (_, i) => `${i}s`);
  const data = Array.from({ length: 10 }, () => Math.floor(Math.random() * 100));

  electricityChart = new Chart(ctx, {
    type: "line",
    data: {
      labels: labels,
      datasets: [
        {
          label: "Electricity Usage",
          data: data,
          backgroundColor: "rgba(33, 236, 243, 0.2)",
          borderColor: "#21ecf3",
          borderWidth: 2,
          fill: true,
        },
      ],
    },
    options: {
      scales: {
        x: {
          title: { display: true, text: "Time (s)" },
        },
        y: {
          title: { display: true, text: "Usage (kWh)" },
        },
      },
    },
  });

  // Update chart data in real-time
  setInterval(() => {
    const newData = Math.floor(Math.random() * 100);
    electricityChart.data.datasets[0].data.push(newData);
    electricityChart.data.datasets[0].data.shift();
    electricityChart.update();
  }, 1000);
}

  