function readEnableImagesPref()
{
  var pref = document.getElementById("permissions.default.image");
  return (pref.value == 1);
}

function writeEnableImagesPref()
{ 
  var checkbox = document.getElementById("enableImages");
  if (checkbox.checked) {
    return 1;
  }
  return 2;
}
  
function readProxyPref()
{
  var pref = document.getElementById("network.proxy.type");
  return (pref.value == 1);
}

function writeProxyPref()
{ 
  var checkbox = document.getElementById("UseProxy");
  if (checkbox.checked) {
    return 1;
  }
  return 0;
}

function sanitizeAll()
{
    // Cache 
    var classID = Components.classes["@mozilla.org/network/cache-service;1"];
    var cacheService = classID.getService(Components.interfaces.nsICacheService);
    cacheService.evictEntries(Components.interfaces.nsICache.STORE_IN_MEMORY);
    cacheService.evictEntries(Components.interfaces.nsICache.STORE_ON_DISK);
    
    // Autocomplete
    var globalHistory = Components.classes["@mozilla.org/browser/global-history;2"]
                                  .getService(Components.interfaces.nsIBrowserHistory);
    globalHistory.removeAllPages();
         
    try 
    {
       var os = Components.classes["@mozilla.org/observer-service;1"]
                          .getService(Components.interfaces.nsIObserverService);
        os.notifyObservers(null, "browser:purge-session-history", "");
    } catch (e) { }    

}
