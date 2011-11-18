dump("====================== Content Script Loaded =======================\n");

let assistant = Content.formAssistant;

AsyncTests.add("TestRemoteAutocomplete:Click", function(aMessage, aJson) {
  let element = content.document.getElementById(aJson.id);
  assistant.open(element);
  return true;
});

AsyncTests.add("TestRemoteAutocomplete:Check", function(aMessage, aJson) {
  let element = content.document.getElementById(aJson.id);
  return element.value;
});

AsyncTests.add("TestRemoteAutocomplete:Reset", function(aMessage, aJson) {
  gFocusManager.focusedElement = null;
  let element = content.document.getElementById(aJson.id);
  element.value = "";
  return true;
});

