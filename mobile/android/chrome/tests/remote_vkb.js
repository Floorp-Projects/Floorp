dump("====================== Content Script Loaded =======================\n");

AsyncTests.add("Test:FocusRoot", function(aMessage, aJson) {
  content.document.getElementById("root").focus();
  return true;
});
