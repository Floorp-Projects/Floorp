
function populateFeedback(aMessage) {
  content.document.getElementById("id_url").value = aMessage.json.referrer;

  let device = content.document.getElementById("id_device");
  if (device)
    device.value = aMessage.json.device || "";

  let manufacturer = content.document.getElementById("id_manufacturer");
  if (manufacturer)
    manufacturer.value = aMessage.json.manufacturer || "";
}

addMessageListener("Feedback:InitPage", populateFeedback);
