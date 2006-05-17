
// Theme Selector
// ( 05/09/2000, Ben Goodger <ben@netscape.com> )

const DEBUG_USE_PROFILE = false;

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
