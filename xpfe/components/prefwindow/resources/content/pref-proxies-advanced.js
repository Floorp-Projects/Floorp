var httpParent;
var httpPortParent;
var sslParent;
var sslPortParent;
var ftpParent;
var ftpPortParent;
var gopherParent;
var gopherPortParent;
var socksParent;
var socksPortParent;
var socksVersionParent;
var socksRemoteDNSParent;
var shareSettingsParent;

var http;
var httpPort;
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
var shareSettings;

var settings;
var parentSettings;

function onLoad()
{
  initElementVars();
  receiveSettingsFromProxyPanel();
  DoEnabling();
}

function initElementVars()
{
  httpParent = opener.document.getElementById("networkProxyHTTP");
  httpPortParent = opener.document.getElementById("networkProxyHTTP_Port");
  sslParent = opener.document.getElementById("networkProxySSL");
  sslPortParent = opener.document.getElementById("networkProxySSL_Port");
  ftpParent = opener.document.getElementById("networkProxyFTP");
  ftpPortParent = opener.document.getElementById("networkProxyFTP_Port");
  gopherParent = opener.document.getElementById("networkProxyGopher");
  gopherPortParent = opener.document.getElementById("networkProxyGopher_Port");
  socksParent = opener.document.getElementById("networkProxySOCKS");
  socksPortParent = opener.document.getElementById("networkProxySOCKS_Port");
  socksVersionParent = opener.document.getElementById("networkProxySOCKSVersion");
  socksRemoteDNSParent = opener.document.getElementById("networkProxySOCKSRemoteDNS");
  shareSettingsParent = opener.document.getElementById("networkProxyShareSettings");

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
  shareSettings = document.getElementById("networkProxyShareSettings");

  // Convenience Arrays
  settings = [http, httpPort, ssl, sslPort, ftp, ftpPort, gopher, gopherPort,
              socks, socksPort, socksVersion, socksRemoteDNS, shareSettings];
  parentSettings = [httpParent, httpPortParent, sslParent, sslPortParent,
                    ftpParent, ftpPortParent, gopherParent, gopherPortParent,
                    socksParent, socksPortParent, socksVersionParent,
                    socksRemoteDNSParent, shareSettingsParent];
}

// Use "" instead of "0" as the default for the port number.
// "0" doesn't make sense as a port number.
function replaceZero(value)
{
  return (value == "0") ? "" : value;
}

function receiveSettingsFromProxyPanel()
{
  // <textbox>es do have a "value" property ...
  http.value = httpParent.value;
  httpPort.value = httpPortParent.value;
  
  // ... <data> elements only a "value" attribute.
  ssl.value = sslParent.getAttribute("value");
  sslPort.value = replaceZero(sslPortParent.getAttribute("value"));
  ftp.value = ftpParent.getAttribute("value");
  ftpPort.value = replaceZero(ftpPortParent.getAttribute("value"));
  gopher.value = gopherParent.getAttribute("value");
  gopherPort.value = replaceZero(gopherPortParent.getAttribute("value"));
  socks.value = socksParent.getAttribute("value");
  socksPort.value = replaceZero(socksPortParent.getAttribute("value"));
  socksVersion.value = socksVersionParent.getAttribute("value");
  socksRemoteDNS.checked  = (socksRemoteDNSParent.getAttribute("value") == "true");
  shareSettings.checked  = (shareSettingsParent.getAttribute("value") == "true");
}

function sendSettingsToProxyPanel()
{
  // <textbox>es do have a "value" property ...
  httpParent.value = http.value;
  httpPortParent.value = httpPort.value;

  // ... <data> elements only a "value" attribute.
  sslParent.setAttribute("value", ssl.value);
  sslPortParent.setAttribute("value", sslPort.value);
  ftpParent.setAttribute("value", ftp.value);
  ftpPortParent.setAttribute("value", ftpPort.value);
  gopherParent.setAttribute("value", gopher.value);
  gopherPortParent.setAttribute("value", gopherPort.value);
  socksParent.setAttribute("value", socks.value);
  socksPortParent.setAttribute("value", socksPort.value);
  socksVersionParent.setAttribute("value", socksVersion.value);
  socksRemoteDNSParent.setAttribute("value", socksRemoteDNS.checked);
  shareSettingsParent.setAttribute("value", shareSettings.checked);
}

function DoProxyCopy()
{
  if (shareSettings.checked)
  {
    ftp.value = gopher.value = ssl.value = http.value;
    ftpPort.value = gopherPort.value = sslPort.value = httpPort.value;
  }
}

function DoEnabling()
{
  ftp.disabled = gopher.disabled = ssl.disabled =
    ftpPort.disabled = gopherPort.disabled = sslPort.disabled =
      shareSettings.checked;

  disableLockedElements();
}

function disableLockedElements()
{
  for (var i = 0; i < parentSettings.length; i++)
    if (parentSettings[i].getAttribute("disabled") == "true")
      settings[i].disabled = true;
}
