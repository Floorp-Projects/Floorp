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
  var socks = document.getElementById("networkProxySOCKS");
  var socksPort = document.getElementById("networkProxySOCKS_Port");
  var socksVersion = document.getElementById("networkProxySOCKSVersion");
  var socksVersion4 = document.getElementById("networkProxySOCKSVersion4");
  var socksVersion5 = document.getElementById("networkProxySOCKSVersion5");
  var ssl = document.getElementById("networkProxySSL");
  var sslPort = document.getElementById("networkProxySSL_Port");
  var noProxy = document.getElementById("networkProxyNone");
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var autoReload = document.getElementById("autoReload");
  var copyButton = document.getElementById("reuseProxy");

  // convenience arrays
  var manual = [ftp, ftpPort, gopher, gopherPort, http, httpPort, socks, socksPort, socksVersion, socksVersion4, socksVersion5, ssl, sslPort, noProxy, copyButton];
  var auto = [autoURL, autoReload];

  // radio buttons
  var radiogroup = document.getElementById("networkProxyType");

  var prefstring;
  switch ( radiogroup.value ) {
    case "0":
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      break;
    case "1":
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled)
	for (i = 0; i < manual.length; i++) {
	  prefstring = manual[i].getAttribute( "prefstring" );
	  if (!parent.hPrefWindow.getPrefIsLocked(prefstring))
	    manual[i].removeAttribute( "disabled" );
	}
      break;
    case "2":
    default:
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled)
        for (i = 0; i < auto.length; i++) {
	  prefstring = manual[i].getAttribute( "prefstring" );
	  if (!parent.hPrefWindow.getPrefIsLocked(prefstring))
            auto[i].removeAttribute( "disabled" );
	}
      break;
  }
}

const nsIProtocolProxyService = Components.interfaces.nsIProtocolProxyService;
const kPROTPROX_CID = '{e9b301c0-e0e4-11D3-a1a8-0050041caf44}';

function ReloadPAC() {
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var pps = Components.classesByID[kPROTPROX_CID]
                       .getService(nsIProtocolProxyService);
  pps.configureFromPAC(autoURL.value);
}

function DoProxyCopy()
{
  var http = document.getElementById("networkProxyHTTP");
  var httpPort = document.getElementById("networkProxyHTTP_Port");
  var httpValue = http.value;
  var httpPortValue = httpPort.value;
  if (httpValue && httpPortValue && parseInt(httpPortValue) > 0) {
    var ftp = document.getElementById("networkProxyFTP");
    var gopher = document.getElementById("networkProxyGopher");
    var ssl = document.getElementById("networkProxySSL");
    var ftpPort = document.getElementById("networkProxyFTP_Port");
    var gopherPort = document.getElementById("networkProxyGopher_Port");
    var sslPort = document.getElementById("networkProxySSL_Port");
    ftp.value = httpValue;
    gopher.value = httpValue;
    ssl.value = httpValue;
    ftpPort.value = httpPortValue;
    gopherPort.value = httpPortValue;
    sslPort.value = httpPortValue;
  }
}

function FixProxyURL()
{
  const nsIURIFixup = Components.interfaces.nsIURIFixup;
  var proxyURL = document.getElementById("networkProxyAutoconfigURL");
  try {
    var URIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                             .getService(nsIURIFixup);
    proxyURL.value = URIFixup.createFixupURI(proxyURL.value,
                                             nsIURIFixup.FIXUP_FLAG_NONE).spec;
  } catch (e) {}
}
