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

function install( aEvent, extName, iconURL, extHash)  {   

    if (aEvent.target.href.match(/^.+\.xpi$/)) {

        var params = new Array();

        params[extName] = {
            URL: aEvent.target.href,
            IconURL: iconURL,
            toString: function () { return this.URL; }
        };

        // Only add the Hash param if it exists.
        //
        // We optionally add this to params[] because installTrigger
        // will still try to compare a null hash as long as the var is set.
        if (extHash) {
            params[extName].Hash = extHash;
        }

        InstallTrigger.install(params);

        try {
            var p = new XMLHttpRequest();
            p.open("GET", "/install.php?uri="+aEvent.target.href, true);
            p.send(null);
        } catch(e) { }

        return false;
    }
}

function installTheme( aEvent, extName) {
    InstallTrigger.installChrome(InstallTrigger.SKIN,aEvent.target.href,extName);

    try {
        var p = new XMLHttpRequest();
        p.open("GET", "/install.php?uri="+aEvent.target.href, true);
        p.send(null);
    } catch(e) { }
    return false;
}
