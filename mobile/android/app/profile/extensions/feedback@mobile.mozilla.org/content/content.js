
function populateFeedback(aMessage) {
  let json = aMessage.json;

  let referrer = json.referrer;
  let URLElem = content.document.getElementById("id_url");
  if (URLElem)
    URLElem.value = referrer;

  let URLElems = content.document.getElementsByClassName("url");
  for (let index=0; index<URLElems.length; index++)
    URLElems[index].value = referrer;

  let device = json.device || "";
  let deviceElem = content.document.getElementById("id_device");
  if (deviceElem)
    deviceElem.value = device;

  let deviceElems = content.document.getElementsByClassName("device");
  for (let index=0; index<deviceElems.length; index++)
    deviceElems[index].value = device;

  let manufacturer = json.manufacturer || "";
  let manufacturerElem = content.document.getElementById("id_manufacturer");
  if (manufacturerElem)
    manufacturerElem.value = manufacturer;

  let manufacturerElems = content.document.getElementsByClassName("manufacturer");
  for (let index=0; index<manufacturerElems.length; index++)
    manufacturerElems[index].value = manufacturer;
}

addMessageListener("Feedback:InitPage", populateFeedback);
