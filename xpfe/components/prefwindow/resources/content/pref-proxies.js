function showManualProxyConfig()
{
  var manualRow = document.getElementById("manual-proxy");
  bundle = srGetStrBundle("chrome://pref/locale/prefutilities.properties");
  var viewHideButton = document.getElementById("viewhideManual");
  if( manualRow.style.display == "none" ) {
    viewHideButton.value = bundle.GetStringFromName("hiderow");
    manualRow.style.display = "inherit";
  }
  else {
    viewHideButton.value = bundle.GetStringFromName("viewrow");
    manualRow.style.display = "none"
  }
}