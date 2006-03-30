var gPlatform = PLATFORM_WINDOWS;

var PLATFORM_OTHER    = 0;
var PLATFORM_WINDOWS  = 1;
var PLATFORM_LINUX    = 2;
var PLATFORM_MACOSX   = 3;
var PLATFORM_MAC      = 4;

if (navigator.platform.indexOf("Win32") != -1)
  gPlatform = PLATFORM_WINDOWS;
else if (navigator.platform.indexOf("Linux") != -1)
  gPlatform = PLATFORM_LINUX;
else if (navigator.userAgent.indexOf("Mac OS X") != -1)
  gPlatform = PLATFORM_MACOSX;
else if (navigator.userAgent.indexOf("MSIE 5.2") != -1)
  gPlatform = PLATFORM_MACOSX;
else if (navigator.platform.indexOf("Mac") != -1)
  gPlatform = PLATFORM_MAC;
else
  gPlatform = PLATFORM_OTHER;

function getPlatformName()
{
    aPlatform = gPlatform;
  if (aPlatform == PLATFORM_WINDOWS)
    return "Windows";
  if (aPlatform == PLATFORM_LINUX)
    return "Linux";
  if (aPlatform == PLATFORM_MACOSX)
    return "MacOSX";
  return "Unknown";
}

function install( aEvent, extName, iconURL)  {   
    var p = new XMLHttpRequest();
    p.open("GET", "/core/install.php?uri="+aEvent.target.href, false);
    p.send(null);

    var params = new Array();
    params[extName] = {
        URL: aEvent.target.href,
        IconURL: iconURL,
        toString: function () { return this.URL; }
    };
    InstallTrigger.install(params);
    return false;
}
