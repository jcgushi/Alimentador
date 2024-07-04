setInterval(function () {
  getMessage();
}, 5000);
function getMessage(){
var xhttp=new XMLHttpRequest();
xhttp.onreadystatechange=function(){
if(this.readyState==4&&this.status==200){
let resposta=this.responseText.split("|");
if(resposta[0]=="1"){//---- dentro do intervalo ---------------
document.getElementById("horaAtual").innerHTML=resposta[1];
document.getElementById("horaAtual").style.color="#ffffff";
}else{
document.getElementById("horaAtual").innerHTML=resposta[1];
document.getElementById("horaAtual").style.color="#000000";};
if(resposta[2]=="1"){//---- ativado ---------------
document.getElementById("ativacao").style.background="red";
document.getElementById("ativacao").style.color="#ffffff";
document.getElementById("ativacao").innerHTML="ATIVADO";
document.getElementById("relogio").style.background="red";
document.getElementById("horaAtual").style.color="#ffffff";
}else{//---- desativado ---------------
document.getElementById("ativacao").style.background="#9dc9ff";
document.getElementById("ativacao").style.color="#000000";
document.getElementById("ativacao").innerHTML="DESATIVADO";
document.getElementById("relogio").style.background="#ecf0f3";
document.getElementById("horaAtual").style.color="#000000";};
document.getElementById("gaivota").style.transform = "rotate("+resposta[3]+"deg)"; 
document.getElementById("posicao").innerHTML=resposta[3]+"°";
document.getElementById("mensagem").innerHTML=resposta[4];};};
xhttp.open("GET","hora",true);
xhttp.send();};
function parametros() {
 var xhttp = new XMLHttpRequest();
 var me = Math.round(parseFloat(document.getElementById("inclinacao_maxima_esquerda").value));
 console.log("inc max esq ",me)
 var md = Math.round(parseFloat(document.getElementById("inclinacao_maxima_direita").value));
 console.log("inc max esq ",md)
 var ie = Math.round(parseFloat(document.getElementById("inclinacao_lateral_esquerda").value));
 console.log("inc esq ",ie)
 var ih = Math.round(parseFloat(document.getElementById("inclinacao_lateral_horizontal").value));
 console.log("inc hor lat ",ih)
 var id = Math.round(parseFloat(document.getElementById("inclinacao_lateral_direita").value));
 console.log("inc dir ",id)
 var ph = Math.round(parseFloat(document.getElementById("inclinacao_horizontal_perna").value));
 console.log("inc hor per ",ph)
 var pb = Math.round(parseFloat(document.getElementById("inclinacao_baixa_perna").value));
 console.log("inc per ",pb)
 var pm = Math.round(parseFloat(document.getElementById("inclinacao_maxima_perna").value));
 console.log("inc max per ",pm)
 var te = document.getElementById("permanencia_esquerda").value;
 console.log("per esq ",te)
 var th = document.getElementById("permanencia_horizontal").value;
 console.log("per hor ",th)
 var td = document.getElementById("permanencia_direita").value;
 console.log("per dir ",td)
 var to = document.getElementById("time_out").value;
 console.log("time out ",to)
 xhttp.onreadystatechange = function () {
 if (this.readyState == 4 && this.status == 200) {
  document.getElementById("mensagem").innerHTML = this.responseText;}};
 var msg="&me="+me+"&md="+md+"&ie="+ie+"&ih="+ih+"&id="+id+"&ph="+ph+"&pb="+pb+"&pm="+pm+"&te="+te+"&th="+th+"&td="+td+"&to="+to;
 console.log("parametros?" + msg);
 xhttp.open("GET", "parametros?" + msg, true);
 xhttp.send();
 };