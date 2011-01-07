
function populateFeedback(aMessage) {
  let json = aMessage.json;

  let referrer = json.referrer;
  let URLElem = content.document.getElementById("id_url");
  if (URLElem)
    URLElem.value = referrer;

  let URLElems = content.document.getElementsByClassName("url");
  URLElems.forEach(function(aElement) {
    aElement.value = referrer;
  });

  let device = json.device || "";
  let deviceElem = content.document.getElementById("id_device");
  if (deviceElem)
    deviceElem.value = device;

  let deviceElems = content.document.getElementsByClassName("device");
  deviceElems.forEach(function(aElement) {
    aElement.value = device;
  });

  let manufacturer = json.manufacturer || "";
  let manufacturerElem = content.document.getElementById("id_manufacturer");
  if (manufacturerElem)
    manufacturerElem.value = manufacturer;

  let manufacturerElems = content.document.getElementsByClassName("manufacturer");
  manufacturerElems.forEach(function(aElement) {
    aElement.value = manufacturer;
  });
}

addMessageListener("Feedback:InitPage", populateFeedback);
