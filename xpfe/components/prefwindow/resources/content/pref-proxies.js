function showManualProxyConfig()
{
  var manualRow = document.getElementById("manual-proxy");
  var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
  var viewHideButton = document.getElementById("viewhideManual");
  if( manualRow.style.display == "none" ) {
    viewHideButton.value = prefutilitiesBundle.getString("hiderow");
    manualRow.style.display = "inherit";
  }
  else {
    viewHideButton.value = prefutilitiesBundle.getString("viewrow");
    manualRow.style.display = "none"
  }
}

function Startup()
{
  DoEnabling();
}

function DoEnabling()
{
  var i;
  var ftp = document.getElementById("networkProxyFTP");
  var ftpPort = document.getElementById("networkProxyFTP_Port");
  var gopher = document.getElementById("networkProxyGopher");
  var gopherPort = document.getElementById("networkProxyGopher_Port");
  var http = document.getElementById("networkProxyHTTP");
  var httpPort = document.getElementById("networkProxyHTTP_Port");
  var ssl = document.getElementById("networkProxySSL");
  var sslPort = document.getElementById("networkProxySSL_Port");
  var socks = document.getElementById("networkProxySOCKS");
  var socksPort = document.getElementById("networkProxySOCKS_Port");
  var noProxy = document.getElementById("networkProxyNone");
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var autoReload = document.getElementById("autoReload");

  // convenience arrays
  var manual = [ftp, ftpPort, gopher, gopherPort, http, httpPort, ssl, sslPort, socks, socksPort, noProxy];
  var auto = [autoURL, autoReload];
  
  // radio buttons
  var radiogroup = document.getElementById("networkProxyType");

  switch ( radiogroup.data ) {
    case "0":  
      for (i = 0; i < manual.length; i++) 
        manual[i].setAttribute( "disabled", "true" );
      for (i = 0; i < auto.length; i++) 
        auto[i].setAttribute( "disabled", "true" );
      break;
    case "1":
      for (i = 0; i < auto.length; i++) 
        auto[i].setAttribute( "disabled", "true" );
      for (i = 0; i < manual.length; i++) 
        manual[i].removeAttribute( "disabled" );
      break;
    case "2":
    default:
      for (i = 0; i < manual.length; i++) 
        manual[i].setAttribute( "disabled", "true" );
      for (i = 0; i < auto.length; i++) 
        auto[i].removeAttribute( "disabled" );
      break;
  }
}
