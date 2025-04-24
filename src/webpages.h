const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<style>
  .slider { -webkit-appearance: none; margin: 14px; width: 360px; height: 5px; background: #FFD65C;
    outline: none; -webkit-transition: .2s; transition: opacity .2s;}
  .slider::-webkit-slider-thumb {-webkit-appearance: none; appearance: none; width: 12px; height: 12px; background: #003249; cursor: pointer;}
  .slider::-moz-range-thumb { width: 35px; height: 35px; background: #003249; cursor: pointer; }
</style>
<body>
  <p>HUB75 Pixel Art Display 改 by tondol (original version by mzashh)</p>
  <p>Firmware: %FIRMWARE%</p>
  <p>Free Storage: <span id="freelittlefs">%FREELittleFS%</span> | Used Storage: <span id="usedlittlefs">%USEDLittleFS%</span> | Total Storage: <span id="totallittlefs">%TOTALLittleFS%</span></p>
  <p>
  <button onclick="listFilesButton()">List Files</button>
  <button onclick="showUploadButtonFancy()">Upload File</button>
  <button onclick="playText1Button()">Play Text 1</button>
  <button onclick="playText2Button()">Play Text 2</button>
  </p>
  <p>
  <button style="background-color: #999" onclick="logoutButton()">Logout</button>
  <button style="background-color: #999" onclick="rebootButton()">Reboot</button>
  </p>
  <p>Brightness: <span id="textSliderValue">%SLIDERVALUE%</span></p>
  <p><input type="range" onchange="updateSliderPWM(this)" id="pwmSlider" min="0" max="255" value="%SLIDERVALUE%" step="1" class="slider"></p>
  <p id="status"></p>
  <p id="detailsheader"></p>
  <p id="details"></p>
<script>
function updateSliderPWM(element) {
  var sliderValue = document.getElementById("pwmSlider").value;
  document.getElementById("textSliderValue").innerHTML = sliderValue;
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/slider?value="+sliderValue, true);
  xhr.send();
}
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
function rebootButton() {
  document.getElementById("details").innerHTML = "Invoking Reboot ...";
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/reboot", false);
  xhr.send();
  window.open("/reboot","_self");
}
function listFilesButton() {
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("detailsheader").innerHTML = "<h3>Files<h3>";
  document.getElementById("details").innerHTML = xmlhttp.responseText;
}
function downloadDeleteButton(filename, action) {
  var urltocall = "/file?name=" + filename + "&action=" + action;
  xmlhttp=new XMLHttpRequest();
  if (action == "delete") {
    xmlhttp.open("GET", urltocall, false);
    xmlhttp.send();
    document.getElementById("status").innerHTML = xmlhttp.responseText;
    xmlhttp.open("GET", "/listfiles", false);
    xmlhttp.send();
    document.getElementById("details").innerHTML = xmlhttp.responseText;
  }
  if (action == "download") {
    document.getElementById("status").innerHTML = "";
    window.open(urltocall,"_blank");
  }
   if (action == "play") {
    xmlhttp.open("GET", urltocall, false);
    xmlhttp.send();
  }
}
function showUploadButtonFancy() {
  document.getElementById("detailsheader").innerHTML = "<h3>Upload File<h3>"
  document.getElementById("status").innerHTML = "";
  var uploadform = "<form method = \"POST\" action = \"/\" enctype=\"multipart/form-data\"><input type=\"file\" name=\"data\"/><input type=\"submit\" name=\"upload\" value=\"Upload\" title = \"Upload File\"></form>"
  document.getElementById("details").innerHTML = uploadform;
  var uploadform =
  "<form id=\"upload_form\" enctype=\"multipart/form-data\" method=\"post\">" +
  "<input type=\"file\" name=\"file1\" id=\"file1\" onchange=\"uploadFile()\"><br>" +
  "<progress id=\"progressBar\" value=\"0\" max=\"100\" style=\"width:300px;\"></progress>" +
  "<h3 id=\"status\"></h3>" +
  "<p id=\"loaded_n_total\"></p>" +
  "</form>";
  document.getElementById("details").innerHTML = uploadform;
}
function playText1Button() {
  document.getElementById("detailsheader").innerHTML = "<h3>Play Text 1<h3>";
  var postform =
  "<form id=\"post_form\" method=\"post\" action=\"/playtext1\">" +
  "<p><input type=\"text\" name=\"text1\" id=\"text1\" placeholder=\"1行目のテキストを入力します\"></p>" +
  "<p><input type=\"text\" name=\"text2\" id=\"text2\" placeholder=\"2行目のテキストを入力します\"></p>" +
  "<p><input type=\"submit\" value=\"Play Text\"></p>" +
  "</form>";
  document.addEventListener('submit', (e) => {
    const form = e.target;
    fetch(form.action, {
        method: form.method,
        body: new FormData(form),
    });
    localStorage.setItem('text1', document.getElementById("text1").value);
    localStorage.setItem('text2', document.getElementById("text2").value);
    e.preventDefault();
  });
  document.getElementById("details").innerHTML = postform;
  document.getElementById("text1").value = localStorage.getItem('text1');
  document.getElementById("text2").value = localStorage.getItem('text2');
}
function playText2Button() {
  document.getElementById("detailsheader").innerHTML = "<h3>Play Text 2<h3>";
  var postform =
  "<form id=\"post_form\" method=\"post\" action=\"/playtext2\">" +
  "<p><textarea name=\"text1\" id=\"text1\" placeholder=\"テキストを入力します\"></textarea></p>" +
  "<p><input type=\"submit\" value=\"Play Text\"></p>" +
  "</form>";
  document.addEventListener('submit', (e) => {
    const form = e.target;
    fetch(form.action, {
        method: form.method,
        body: new FormData(form),
    });
    localStorage.setItem('text1', document.getElementById("text1").value);
    e.preventDefault();
  });
  document.getElementById("details").innerHTML = postform;
  document.getElementById("text1").value = localStorage.getItem('text1');
}
function _(el) {
  return document.getElementById(el);
}
function uploadFile() {
  var file = _("file1").files[0];
  var formdata = new FormData();
  formdata.append("file1", file);
  var ajax = new XMLHttpRequest();
  ajax.upload.addEventListener("progress", progressHandler, false);
  ajax.addEventListener("load", completeHandler, false); // doesnt appear to ever get called even upon success
  ajax.addEventListener("error", errorHandler, false);
  ajax.addEventListener("abort", abortHandler, false);
  ajax.open("POST", "/");
  ajax.send(formdata);
}
function progressHandler(event) {
  _("loaded_n_total").innerHTML = "Uploaded " + event.loaded + " bytes";
  var percent = (event.loaded / event.total) * 100;
  _("progressBar").value = Math.round(percent);
  _("status").innerHTML = Math.round(percent) + "% uploaded... please wait";
  if (percent >= 100) {
    _("status").innerHTML = "Please wait, writing file to filesystem";
  }
}
function completeHandler(event) {
  _("status").innerHTML = "Upload Complete";
  _("progressBar").value = 0;
  xmlhttp=new XMLHttpRequest();
  xmlhttp.open("GET", "/listfiles", false);
  xmlhttp.send();
  document.getElementById("status").innerHTML = "File Uploaded";
  document.getElementById("detailsheader").innerHTML = "<h3>Files<h3>";
  document.getElementById("details").innerHTML = xmlhttp.responseText;
}
function errorHandler(event) {
  _("status").innerHTML = "Upload Failed";
}
function abortHandler(event) {
  _("status").innerHTML = "inUpload Aborted";
}
</script>
</body>
</html>
)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p><a href="/">Log Back In</a></p>
</body>
</html>
)rawliteral";

// reboot.html base upon https://gist.github.com/Joel-James/62d98e8cb3a1b6b05102
const char reboot_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta charset="UTF-8">
</head>
<body>
<h3>
  Rebooting, returning to main page in <span id="countdown">30</span> seconds
</h3>
<script type="text/javascript">
  var seconds = 5;
  function countdown() {
    seconds = seconds - 1;
    if (seconds < 0) {
      window.location = "/";
    } else {
      document.getElementById("countdown").innerHTML = seconds;
      window.setTimeout("countdown()", 1000);
    }
  }
  countdown();
</script>
</body>
</html>
)rawliteral";
