
/**
 * Go into online/offline mode
 **/
function setOfflineStatus(aToggleFlag)
{
  var ioService = nsJSComponentManager.getServiceByID("{9ac9e770-18bc-11d3-9337-00104ba0fd40}", 
                                                      "nsIIOService");
  var broadcaster = document.getElementById("Communicator:WorkMode");
  if (aToggleFlag)
    ioService.offline = !ioService.offline;

  if (ioService.offline && broadcaster)
    {
      broadcaster.setAttribute("offline", "true");
      broadcaster.setAttribute("value", bundle.GetStringFromName("goonline"));
    }
  else if (broadcaster)
    {
      broadcaster.removeAttribute("offline");
      broadcaster.setAttribute("value", bundle.GetStringFromName("gooffline"));
    }
}


