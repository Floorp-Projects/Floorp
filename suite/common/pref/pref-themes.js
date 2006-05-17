
// Theme Selector
// ( 05/09/2000, Ben Goodger <ben@netscape.com> )

var gPrefutilitiesBundle;
var gBrandBundle;

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
  gBrandBundle = document.getElementById("bundle_brand");

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
  if (!chromeRegistry.isSkinSelected(skinName, DEBUG_USE_PROFILE)) {
    chromeRegistry.selectSkin( skinName, DEBUG_USE_PROFILE );
    chromeRegistry.refreshSkins();
  }
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

  if (!tree)
    return;

  var selectedItem = tree.selectedItems.length ? tree.selectedItems[0] : null;
  var applyButton = document.getElementById("applySkin");
  if (selectedItem && selectedItem.getAttribute("skin") == "true") {
    var themeName = selectedItem.getAttribute("displayName");
    var skinName = selectedItem.getAttribute("name");


    var oldTheme;
    try {
      oldTheme = !chromeRegistry.checkThemeVersion(skinName);
    }
    catch(e) {
      oldTheme = false;
    }

    var nameField = document.getElementById("displayName");
    var author = document.getElementById("author");
    var image = document.getElementById("previewImage");
    var descText = document.createTextNode(selectedItem.getAttribute("description"));
    var description = document.getElementById("description");
    var uninstallButton = document.getElementById("uninstallSkin");
    var applyLabel = gPrefutilitiesBundle.getString("applyThemePrefix");
    var uninstallLabel = gPrefutilitiesBundle.getString("uninstallThemePrefix");

    while (description.hasChildNodes())
      description.removeChild(description.firstChild);

    nameField.setAttribute("value", themeName);
    author.setAttribute("value", selectedItem.getAttribute("author"));
    image.setAttribute("src", selectedItem.getAttribute("image"));


    if (!oldTheme) {    
      description.appendChild(descText);

      applyButton.removeAttribute("disabled");
      applyLabel = applyLabel.replace(/%theme_name%/, themeName);
      uninstallLabel = uninstallLabel.replace(/%theme_name%/, themeName);
      applyButton.label = applyLabel;
      uninstallButton.label = uninstallLabel;

      var locType = selectedItem.getAttribute("loctype");
      uninstallButton.disabled = (locType == "install");
    }
    else{
      applyLabel = gPrefutilitiesBundle.getString("applyThemePrefix");
      applyLabel = applyLabel.replace(/%theme_name%/, themeName);
      applyButton.label = applyLabel;

      uninstallLabel = uninstallLabel.replace(/%theme_name%/, themeName);
      uninstallButton.label = uninstallLabel;

      applyButton.setAttribute("disabled", true);
      uninstallButton.disabled = false;

      var newText = gPrefutilitiesBundle.getString("oldTheme");
      newText = newText.replace(/%theme_name%/, themeName);
      
      newText = newText.replace(/%brand%/g, gBrandBundle.getString("brandShortName"));

      descText = document.createTextNode(newText);
      description.appendChild(descText);
    }

  }
  else {
    applyButton.setAttribute("disabled", true);
  }
}



