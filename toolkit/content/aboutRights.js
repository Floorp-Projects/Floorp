var servicesDiv = document.getElementById("webservices-container");
servicesDiv.style.display = "none";

function showServices() {
  servicesDiv.style.display = "";
}
let showWebServices = document.getElementById("showWebServices");
showWebServices.addEventListener("click",showServices);

var disablingServicesDiv = document.getElementById("disabling-webservices-container");

function showDisablingServices() {
  disablingServicesDiv.style.display = "";
}

if (disablingServicesDiv != null) {
  disablingServicesDiv.style.display = "none";
  let showDisablingWebServices = document.getElementById("showDisablingWebServices");
  showDisablingWebServices.addEventListener("click",showDisablingServices);
}
