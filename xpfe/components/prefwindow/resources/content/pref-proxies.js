var http;
var httpPort;
var noProxy;
var ssl;
var sslPort;
var ftp;
var ftpPort;
var gopher;
var gopherPort;
var socks;
var socksPort;
var socksVersion;
var socksRemoteDNS;
var advancedButton;
var autoURL;
var autoReload;
var radiogroup;
var shareSettings;

function Startup()
{
  initElementVars();

  // Calculate a sane default for network.proxy.share_proxy_settings.
  if (shareSettings.getAttribute("value") == "")
    shareSettings.setAttribute("value", defaultForShareSettingsPref());

  DoEnabling();
  
  // Use "" instead of "0" as the default for the port number.
  // "0" doesn't make sense as a port number.
  if (httpPort.value == "0")
    httpPort.setAttribute("value", "");

  // The pref value 3 for network.proxy.type is unused to maintain
  // backwards compatibility. Treat 3 equally to 0. See bug 115720.
  if (radiogroup.value == 3)
    radiogroup.selectedIndex = 0;
}

function initElementVars()
{
  http = document.getElementById("networkProxyHTTP");
  httpPort = document.getElementById("networkProxyHTTP_Port");
  ssl = document.getElementById("networkProxySSL");
  sslPort = document.getElementById("networkProxySSL_Port");
  ftp = document.getElementById("networkProxyFTP");
  ftpPort = document.getElementById("networkProxyFTP_Port");
  gopher = document.getElementById("networkProxyGopher");
  gopherPort = document.getElementById("networkProxyGopher_Port");
  socks = document.getElementById("networkProxySOCKS");
  socksPort = document.getElementById("networkProxySOCKS_Port");
  socksVersion = document.getElementById("networkProxySOCKSVersion");
  socksRemoteDNS = document.getElementById("networkProxySOCKSRemoteDNS");
  noProxy = document.getElementById("networkProxyNone");
  advancedButton = document.getElementById("advancedButton");
  autoURL = document.getElementById("networkProxyAutoconfigURL");
  autoReload = document.getElementById("autoReload");
  radiogroup = document.getElementById("networkProxyType");
  shareSettings = document.getElementById("networkProxyShareSettings");
}

// Returns true if all protocol specific proxies and all their
// ports are set to the same value, false otherwise.
function defaultForShareSettingsPref()
{
  return http.value == ftp.getAttribute("value") &&
         http.value == gopher.getAttribute("value") &&
         http.value == ssl.getAttribute("value") &&
         httpPort.value == ftpPort.getAttribute("value") &&
         httpPort.value == sslPort.getAttribute("value") &&
         httpPort.value == gopherPort.getAttribute("value");
}

function DoEnabling()
{  
  // convenience arrays
  var manual = [ftp, ftpPort, gopher, gopherPort, http, httpPort, socks,
                socksPort, socksVersion, socksRemoteDNS, ssl, sslPort, noProxy,
                advancedButton, shareSettings];
  var auto = [autoURL, autoReload];

  switch (radiogroup.value)
  {
    case "0":
    case "4":
      disable(manual);
      disable(auto);
      break;
    case "1":
      disable(auto);
      if (!radiogroup.disabled)
        enableUnlockedElements(manual);
      break;
    case "2":
    default:
      disable(manual);
      if (!radiogroup.disabled)
        enableUnlockedElements(auto);
      break;
  }
}

function disable(elements)
{
  for (var i = 0; i < elements.length; i++)
    elements[i].setAttribute("disabled", "true");
}

function enableUnlockedElements(elements)
{
  for (var i = 0; i < elements.length; i++) {
    var prefstring = elements[i].getAttribute("prefstring");
    if (!parent.hPrefWindow.getPrefIsLocked(prefstring))
      elements[i].removeAttribute("disabled");
  }
}

const nsPIProtocolProxyService = Components.interfaces.nsPIProtocolProxyService;

function ReloadPAC() {
  var pps = Components.classes["@mozilla.org/network/protocol-proxy-service;1"]
                       .getService(nsPIProtocolProxyService);
  pps.configureFromPAC(autoURL.value);
}

function FixProxyURL()
{
  const nsIURIFixup = Components.interfaces.nsIURIFixup;
  try {
    var URIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                             .getService(nsIURIFixup);
    autoURL.value = URIFixup.createFixupURI(autoURL.value,
                                            nsIURIFixup.FIXUP_FLAG_NONE).spec;
  } catch (e) {}
}

function openAdvancedDialog()
{
  openDialog("chrome://communicator/content/pref/pref-proxies-advanced.xul",
             "AdvancedProxyPreferences",
             "chrome,titlebar,centerscreen,resizable=no,modal");
}

function DoProxyCopy()
{
  if (shareSettings.getAttribute("value") != "true")
    return;

  ftp.setAttribute("value", http.value);
  ssl.setAttribute("value", http.value);
  gopher.setAttribute("value", http.value);

  ftpPort.setAttribute("value", httpPort.value);
  sslPort.setAttribute("value", httpPort.value);
  gopherPort.setAttribute("value", httpPort.value);
}
