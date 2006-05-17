function showManualProxyConfig()
{
  var manualRow = document.getElementById("manual-proxy");
  bundle = srGetStrBundle("chrome://communicator/locale/pref/prefutilities.properties");
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

function Startup()
{
  DoEnabling();
}

function DoEnabling()
{
  var ftp = document.getElementById("networkProxyFTP");
  var ftpPort = document.getElementById("networkProxyFTP_Port");
  var http = document.getElementById("networkProxyHTTP");
  var httpPort = document.getElementById("networkProxyHTTP_Port");
  var noProxy = document.getElementById("networkProxyNone");
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var autoReload = document.getElementById("autoReload");

  // convenience arrays
  var manual = [ftp, ftpPort, http, httpPort, noProxy];
  var auto = [autoURL, autoReload];
  
  // radio buttons
  var radio0 = document.getElementById("networkProxyType0");
  var radio1 = document.getElementById("networkProxyType1");
  var radio2 = document.getElementById("networkProxyType2");
  
  if( radio0.checked ) {
    for( var i = 0; i < manual.length; i++ ) 
      manual[i].setAttribute( "disabled", "true" );
    for( var i = 0; i < auto.length; i++ ) 
      auto[i].setAttribute( "disabled", "true" );
  }
  else if ( radio1.checked ) {
    for( var i = 0; i < auto.length; i++ ) 
      auto[i].setAttribute( "disabled", "true" );
    for( var i = 0; i < manual.length; i++ ) 
      manual[i].removeAttribute( "disabled" );
  }
  else if ( radio2.checked ) {
    for( var i = 0; i < manual.length; i++ ) 
      manual[i].setAttribute( "disabled", "true" );
    for( var i = 0; i < auto.length; i++ ) 
      auto[i].removeAttribute( "disabled" );
  }
}
