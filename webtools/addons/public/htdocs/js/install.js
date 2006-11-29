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
  if (gPlatform == PLATFORM_WINDOWS)
    return "Windows";
  if (gPlatform == PLATFORM_LINUX)
    return "Linux";
  if (gPlatform == PLATFORM_MACOSX)
    return "MacOSX";
  return "Unknown";
}

function getInstallURL(aEvent) {
 
    // The event target might be the link itself or one of its children
    var target = aEvent.target;
    while (target && !target.href)
      target = target.parentNode;
    
    return target && target.href;
}

function install( aEvent, extName, iconURL, extHash)  {   

    if (aEvent.altKey || !window.InstallTrigger)
        return true;

    var url = getInstallURL();

    if (url && url.match(/^.+\.xpi$/)) {

        var params = new Array();

        params[extName] = {
            URL: url,
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
            p.open("GET", "/install.php?uri=" + url, true);
            p.send(null);
        } catch(e) { }

        return false;
    }
    return true;
}

function installTheme( aEvent, extName) {
    if(!window.InstallTrigger) 
        return true;
    
    var url = getInstallURL(aEvent);
    if (!url)
        return true;

    InstallTrigger.installChrome(InstallTrigger.SKIN,url,extName);

    try {
        var p = new XMLHttpRequest();
        p.open("GET", "/install.php?uri="+url, true);
        p.send(null);
    } catch(e) { }
    return false;
}

function fixPlatformLinks(addonID, name)
{
    var platform = getPlatformName();
    var outer = document.getElementById("install-"+ addonID);
    var installs = outer.getElementsByTagName("p");
    var found = false;
    for (var i = 0; i < installs.length; i++) {
        var className = installs[i].className;
        if (className.indexOf("platform-" + platform) != -1 ||
            className.indexOf("platform-ALL") != -1) {
                found = true;
        } else {
                installs[i].style.display = "none";
        }
    }
    if (!found)
        outer.appendChild(document.createTextNode(name + " is not available for " + platform)); 
}
