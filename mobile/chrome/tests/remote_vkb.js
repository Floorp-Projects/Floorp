dump("====================== Content Script Loaded =======================\n");

AsyncTests.add("Test:Focus", function(aMessage, aJson) {
  content.document.getElementById("root").focus();
  return true;
});
