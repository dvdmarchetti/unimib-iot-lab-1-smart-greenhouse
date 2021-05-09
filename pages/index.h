const char PAGE_ROOT[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Document</title>
  <link href="https://unpkg.com/tailwindcss@^2/dist/tailwind.min.css" rel="stylesheet">
  <link rel="stylesheet" href="https://unicons.iconscout.com/release/v3.0.6/css/line.css">
  <style>.card:hover .button {bottom: -32px}</style>
</head>
<body class="bg-gray-100" style="min-width: 320px;">
  <div id="app">
    <div class="container mx-auto px-4 md:px-2 py-4">
      <div class="flex items-baseline justify-between">
        <h3 class="text-3xl">ESP8266 Greenhouse</h3>

        <span class="text-xs tracking-wide font-semibold">
          <span v-if="status.error != null" class="text-red-500">{{ status.error }}</span>
          <span v-else-if="status.last_update != null">{{ status.last_update.toUTCString() }}</span>
          <span v-else>Loading...</span>
        </span>
      </div>
    </div>

    <div class="container px-4 md:px-0 mx-auto grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4">
      <div class="mb-2 md:px-1 lg:pr-1">
        <div class="flex flex-row bg-white shadow-sm items-center rounded-xl p-4 h-full">
          <div class="flex items-center justify-center flex-shrink-0 h-12 w-12 rounded-lg bg-red-100 text-red-500">
            <i class="uil uil-temperature-half text-2xl"></i>
          </div>
          <div class="flex flex-col flex-grow ml-4 justify-center">
            <div class="text-xs text-gray-600 font-semibold uppercase tracking-wide">Temperature</div>
            <div class="text-lg font-semibold text-red-600">
              <span v-if="!temperature.updating">{{ temperature.value.toFixed(2) }}°C</span>
              <span v-else class="text-sm animate-pulse">Updating</span>
            </div>
          </div>
          <div class="flex flex-col">
            <div class="cursor-pointer text-gray-500 hover:text-red-500 transition" @click="doIncreaseTemperature">
              <i class="w-6 uil uil-plus-circle text-2xl"></i>
            </div>

            <div class="cursor-pointer text-gray-500 hover:text-red-500 transition" @click="doDecreaseTemperature">
              <i class="w-6 uil uil-minus-circle text-2xl"></i>
            </div>
          </div>
        </div>
      </div>

      <div class="mb-2 md:px-1 lg:px-1">
        <div class="flex flex-row items-center bg-white shadow-sm rounded-xl p-4 h-full">
          <div class="flex items-center justify-center flex-shrink-0 h-12 w-12 rounded-lg bg-blue-100 text-blue-500">
            <i class="uil uil-water text-2xl"></i>
          </div>
          <div class="flex flex-col flex-grow ml-4 justify-center">
            <div class="text-xs text-gray-600 font-semibold uppercase tracking-wide">Humidity</div>
            <div class="text-lg font-semibold text-blue-600">{{ humidity.toFixed(0) }}%</div>
          </div>
        </div>
      </div>

      <div class="mb-2 md:px-1 lg:px-1">
        <div class="flex flex-row items-center bg-white shadow-sm rounded-xl p-4 h-full">
          <div class="flex items-center justify-center flex-shrink-0 h-12 w-12 rounded-lg bg-yellow-100 text-yellow-500">
            <i class="uil uil-sun text-2xl"></i>
          </div>
          <div class="flex flex-col flex-grow ml-4 justify-center">
            <div class="text-xs text-gray-600 font-semibold uppercase tracking-wide">Light Amount</div>
            <div class="text-lg font-semibold text-yellow-600">{{ (lightness/1024*100).toFixed(0) }}%
            </div>
          </div>
        </div>
      </div>

      <div class="mb-2 md:px-1 lg:pr-2 lg:pl-1">
        <div class="flex flex-row items-center bg-white shadow-sm rounded-xl p-4 h-full">
          <div class="flex items-center justify-center flex-shrink-0 h-12 w-12 rounded-lg bg-green-100 text-green-500">
            <i class="uil uil-raindrops-alt text-2xl"></i>
          </div>
          <div class="flex flex-col flex-grow ml-4 justify-center">
            <div class="text-xs text-gray-600 font-semibold uppercase tracking-wide">Terrain Moisture</div>
            <div class="text-lg font-semibold text-green-600">
              <span v-if="!moisture.updating">{{ (moisture.value/1024*100).toFixed(0) }}%</span>
              <span v-else class="text-sm animate-pulse">Updating</span>
            </div>
          </div>
          <div class="flex flex-col">
            <div class="cursor-pointer text-gray-600 hover:text-green-500 transition" @click="doWaterTerrain">
              <i class="w-6 uil uil-plus-circle text-2xl"></i>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div><!-- /#app -->

  <div class="container mx-auto text-xs text-gray-500 tracking-wide text-center py-2">
    <p class="sm:inline-block">Davide Marchetti &ndash; Laboratorio IoT@Unimib &ndash; A.A.2020/2021</p>
    <span class="hidden sm:inline-block"> &ndash; </span>
    <p class="sm:inline-block">Unicons by <a href="https://iconscout.com/" class="text-purple-500">Iconscout</a></p>
  </div>

  <script src="https://cdn.jsdelivr.net/npm/vue@2.6.12"></script>
  <script>
    const app = new Vue({
      el: '#app',
      data: {
        status: {
          error: null,
          last_update: null,
        },
        temperature: {
          value: 0.0,
          updating: false,
        },
        humidity: 0,
        lightness: 0,
        moisture: {
          value: 0,
          updating: false,
        },
      },

      mounted () {
        this.fetchData()
        setInterval(this.fetchData, 5000);
      },

      methods: {
        async fetchData() {
          try {
            let response = await fetch('/api/status');

            let data = await response.json();
            this.status.last_update = new Date()
            this.temperature.value = data['temperature']
            this.humidity = data['humidity']
            this.lightness = data['lightness']
            this.moisture.value = data['moisture']
          } catch {
            this.error = "Update failed"
          }
        },

        async doWaterTerrain() {
          if (this.moisture.updating) {
            return;
          }

          this.moisture.updating = true
          let response = await fetch('/api/moisture', { method: 'POST' });
          this.moisture.updating = false
        },

        async doIncreaseTemperature() {
          if (this.temperature.updating) {
            return;
          }

          this.temperature.updating = true
          let response = await fetch('/api/temperature/increase', { method: 'POST' });
          this.temperature.updating = false
        },

        async doDecreaseTemperature() {
          if (this.temperature.updating) {
            return;
          }

          this.temperature.updating = true
          let response = await fetch('/api/temperature/decrease', { method: 'POST' });
          this.temperature.updating = false
        }
      }
    })
  </script>
</body>
</html>
)=====";
