setInterval(function () {
  getMessage();
}, 5000);
function getMessage() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("mensagem").innerHTML = this.responseText;
      xhttp.open("GET", "mensagem?"+mensagem, true);
      xhttp.send();
    }
  };
}
function girar(horario) {
  var xhttp = new XMLHttpRequest();
  console.log("sentido horario ", horario);
  xhttp.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("mensagem").innerHTML = this.responseText;
    }
  };
  var msg = "&horario=" + horario;
  console.log("parametros?" + msg);
  xhttp.open("GET", "parametros?" + msg, true);
  xhttp.send();
}
