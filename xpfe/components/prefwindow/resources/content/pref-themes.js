
// Theme Selector
// ( 05/09/2000, Ben Goodger <ben@netscape.com> )

const DEBUG_USE_PROFILE = true;


try 
  {
    var chromeRegistry = Components.classes["component://netscape/chrome/chrome-registry"].getService();
    if ( chromeRegistry )
      chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
  }
catch(e) 
  {
  }

function selectSkin()
  {
    var tree = document.getElementById( "skinsTree" );
    
    var commonDialogs = nsJSComponentManager.getService("component://netscape/appshell/commonDialogs", 
                                                        "nsICommonDialogs");
   
    var selectedSkinItem = tree.selectedItems[0];
    var skinName = selectedSkinItem.getAttribute( "name" );
    chromeRegistry.selectSkin( skinName, DEBUG_USE_PROFILE ); 
    chromeRegistry.refreshSkins();
  }  
 
function deselectSkin()
  {
    var tree = document.getElementById( "skinsTree" );
    var selectedSkinItem = tree.selectedItems[0];
    var skinName = selectedSkinItem.getAttribute( "name" );
    chromeRegistry.deselectSkin( skinName, DEBUG_USE_PROFILE );
    chromeRegistry.refreshSkins();
  }
  
// XXX DEBUG ONLY. DO NOT LOCALIZE
function installSkin()
  {
    var themeURL = prompt( "Enter URL for a skin to install:","");
    chromeRegistry.installSkin( themeURL, DEBUG_USE_PROFILE, false );
  }

function themeSelect()
  {
    var tree = document.getElementById("skinsTree");
    var selectedItem = tree.selectedItems.length ? tree.selectedItems[0] : null;
    if (selectedItem && selectedItem.getAttribute("skin") == "true") 
      {
        var displayName = document.getElementById("displayName");
        displayName.setAttribute("value", selectedItem.getAttribute("displayName"));
        var author = document.getElementById("author");
        author.setAttribute("value", selectedItem.getAttribute("author"));
        var email = document.getElementById("email");
        var address = selectedItem.getAttribute("email");
        if (address)
          {
            email.removeAttribute("disabled");
            email.setAttribute("emailaddress", address);
            email.setAttribute("tooltiptext", address);
            email.setAttribute("emailtitle", selectedItem.getAttribute("displayName"));
          }
        else
          email.setAttribute("disabled", "true");
        var website = document.getElementById("website");
        var url = selectedItem.getAttribute("website");
        if (url)  
          {
            website.removeAttribute("disabled");
            website.setAttribute("website", url);
            website.setAttribute("tooltiptext", url);
          }
        else 
          website.setAttribute("disabled", "true");
        var image = document.getElementById("previewImage");
        image.setAttribute("src", selectedItem.getAttribute("image"));
      }
  }
  
function gotoWebsite(aElement)
  {
    openTopWin(aElement.getAttribute("website"));
  } 
  
function sendEmail(aElement)
  {
    window.openDialog( "chrome://messenger/content/messengercompose/messengercompose.xul", "_blank", 
                     "chrome,all,dialog=no", "subject='" + aElement.getAttribute('displayName') + "',bodyislink=true");
    
  } 
  
var bundle = srGetStrBundle("chrome://communicator/locale/pref/prefutilities.properties");
  