
// Theme Selector
// ( 05/09/2000, Ben Goodger <ben@netscape.com> )

var bundle = srGetStrBundle("chrome://communicator/locale/pref/prefutilities.properties");

const DEBUG_USE_PROFILE = true;


try {
  var chromeRegistry = Components.classes["component://netscape/chrome/chrome-registry"].getService();
  if ( chromeRegistry )
    chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
}
catch(e) {}

function Startup()
{
  var tree = document.getElementById( "skinsTree" );
  var theSkinKids = document.getElementById("theSkinKids");
  if (theSkinKids.hasChildNodes() && theSkinKids.firstChild)
    tree.selectItem(theSkinKids.firstChild);
}

function applySkin()
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

function themeSelect()
{
  var tree = document.getElementById("skinsTree");
  var selectedItem = tree.selectedItems.length ? tree.selectedItems[0] : null;
  if (selectedItem && selectedItem.getAttribute("skin") == "true") {
    var themeName = selectedItem.getAttribute("displayName");
    var nameField = document.getElementById("displayName");
    nameField.setAttribute("value", themeName);
    var author = document.getElementById("author");
    author.setAttribute("value", selectedItem.getAttribute("author"));
    var image = document.getElementById("previewImage");
    image.setAttribute("src", selectedItem.getAttribute("image"));
    var descText = document.createTextNode(selectedItem.getAttribute("description"));
    var description = document.getElementById("description");
    while (description.hasChildNodes())
      description.removeChild(description.firstChild);
    description.appendChild(descText);
    var applyButton = document.getElementById("applySkin");
    var applyLabel = bundle.GetStringFromName("applyThemePrefix");
    applyLabel = applyLabel.replace(/%theme_name%/, themeName);
    applyButton.value = applyLabel;
  }
}

function restoreDefaults()
{
  var theSkinKids = document.getElementById("theSkinKids");
  if (theSkinKids.hasChildNodes()) {
    var defaultTheme = theSkinKids.firstChild;
    var tree = document.getElementById( "skinsTree" );
    var selectedSkinItem = tree.selectedItems[0];
    var skinName = selectedSkinItem.getAttribute( "name" );
    chromeRegistry.selectSkin( skinName, DEBUG_USE_PROFILE ); 
    chromeRegistry.refreshSkins();
  }
}
  
  