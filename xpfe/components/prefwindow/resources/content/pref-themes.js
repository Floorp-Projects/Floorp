
// Theme Selector
// ( 05/09/2000, Ben Goodger <ben@netscape.com> )

var gPrefutilitiesBundle;

const DEBUG_USE_PROFILE = true;


try {
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
  if ( chromeRegistry )
    chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
}
catch(e) {}

function Startup()
{
  gPrefutilitiesBundle = document.getElementById("bundle_prefutilities");
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

function uninstallSkin()
{
  var tree = document.getElementById( "skinsTree" );
  var selectedSkinItem = tree.selectedItems[0];
  var skinName = selectedSkinItem.getAttribute( "name" );
  var inUse = chromeRegistry.isSkinSelected(skinName, DEBUG_USE_PROFILE);
  chromeRegistry.uninstallSkin( skinName, DEBUG_USE_PROFILE );
  if (inUse)
    chromeRegistry.refreshSkins();
  tree.selectedIndex = 0;
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
    var uninstallButton = document.getElementById("uninstallSkin");
    var applyLabel = gPrefutilitiesBundle.getString("applyThemePrefix");
    var uninstallLabel = gPrefutilitiesBundle.getString("uninstallThemePrefix");
    applyLabel = applyLabel.replace(/%theme_name%/, themeName);
    uninstallLabel = uninstallLabel.replace(/%theme_name%/, themeName);
    applyButton.label = applyLabel;
    uninstallButton.label = uninstallLabel;
    var locType = selectedItem.getAttribute("loctype");
    uninstallButton.disabled = (locType == "install");
  }
}

