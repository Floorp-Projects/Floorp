dump("====================== Content Script Loaded =======================\n");

let assistant = Content.formAssistant;

AsyncTests.add("FormAssist:Show", function(aMessage, aJson) {
  let element = content.document.getElementById(aJson.id);
  assistant.open(element);
});

