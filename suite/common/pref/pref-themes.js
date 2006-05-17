
// Theme Selector
// ( 05/09/2000, Ben Goodger <ben@netscape.com> )

var gShowDescription = true;

const DEBUG_USE_PROFILE = true;


try {
  var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
  if ( chromeRegistry )
    chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
}
catch(e) {}

const kPrefSvcContractID = "@mozilla.org/preferences;1";
const kPrefSvcIID = Components.interfaces.nsIPref;
const kPrefSvc = Components.classes[kPrefSvcContractID].getService(kPrefSvcIID);

function Startup()
{
  var tree = document.getElementById( "skinsTree" );
  var theSkinKids = document.getElementById("theSkinKids");
  if (theSkinKids.hasChildNodes() && theSkinKids.firstChild)
    tree.selectItem(theSkinKids.firstChild);
  var navbundle = document.getElementById("bundle_navigator");
  var showSkinsDescription = navbundle.getString("showskinsdescription");
  if( showSkinsDescription == "false" )
  {
    gShowDescription = false;
    var description = document.getElementById("description");
    while (description.hasChildNodes())
      description.removeChild(description.firstChild);
  }
}

function applySkin()
{
  var tree = document.getElementById("skinsTree");
  var selectedSkinItem = tree.selectedItems[0];
  var skinName = selectedSkinItem.getAttribute("name");
  // XXX - this sucks and should only be temporary.
  kPrefSvc.SetUnicharPref("general.skins.selectedSkin", skinName);
  tree.selectItem(selectedSkinItem);

  var observerService = Components.classes["@mozilla.org/observer-service;1"].getService(Components.interfaces.nsIObserverService);
  observerService.notifyObservers(null, "skin-selected", null);

  var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);
  if (promptService) {
    var navbundle = document.getElementById("bundle_navigator");
    var brandbundle = document.getElementById("bundle_brand");
    var dialogTitle = navbundle.getString("switchskinstitle");
    var brandName = brandbundle.getString("brandShortName");
    var msg = navbundle.formatStringFromName("switchskins",
                                             [brandName],
                                             1);
    promptService.alert(window, dialogTitle, msg);
  }
}

function uninstallSkin()
{
  var tree = document.getElementById("skinsTree");
  var selectedSkinItem = tree.selectedItems[0];
  var skinName = selectedSkinItem.getAttribute("name");
  var inUse = chromeRegistry.isSkinSelected(skinName, DEBUG_USE_PROFILE);
  chromeRegistry.uninstallSkin(skinName, DEBUG_USE_PROFILE);
  if (inUse)
    chromeRegistry.refreshSkins();
  tree.selectedIndex = 0;
}

function themeSelect()
{
  var tree = document.getElementById("skinsTree");

  if (!tree)
    return;

  var prefbundle = document.getElementById("bundle_prefutilities");

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
    var applyLabel = prefbundle.getString("applyThemePrefix");
    var uninstallLabel = prefbundle.getString("uninstallThemePrefix");

    while (description.hasChildNodes())
      description.removeChild(description.firstChild);

    nameField.setAttribute("value", themeName);
    author.setAttribute("value", selectedItem.getAttribute("author"));
    image.setAttribute("src", selectedItem.getAttribute("image"));

    // XXX - this sucks and should only be temporary.
    var selectedSkin = "";
    try {
      selectedSkin = kPrefSvc.CopyCharPref("general.skins.selectedSkin");
    }
    catch (e) {
    }
    if (!oldTheme) {    
      if( gShowDescription ) 
        description.appendChild(descText);

      var locType = selectedItem.getAttribute("loctype");
      uninstallButton.disabled = (selectedSkin == skinName) || (locType == "install");
      applyButton.disabled = (selectedSkin == skinName);
      
      applyLabel = applyLabel.replace(/%theme_name%/, themeName);
      uninstallLabel = uninstallLabel.replace(/%theme_name%/, themeName);
      applyButton.label = applyLabel;
      uninstallButton.label = uninstallLabel;
    }
    else {
      var brandbundle = document.getElementById("bundle_brand");
      applyLabel = prefbundle.getString("applyThemePrefix");
      applyLabel = applyLabel.replace(/%theme_name%/, themeName);
      applyButton.label = applyLabel;
      applyButton.disabled = selectedSkin == skinName;

      uninstallLabel = uninstallLabel.replace(/%theme_name%/, themeName);
      uninstallButton.label = uninstallLabel;

      applyButton.setAttribute("disabled", true);
      uninstallButton.disabled = selectedSkin == skinName;

      var newText = prefbundle.getString("oldTheme");
      newText = newText.replace(/%theme_name%/, themeName);
      
      newText = newText.replace(/%brand%/g, brandbundle.getString("brandShortName"));

      if( gShowDescription )  {
        descText = document.createTextNode(newText);
        description.appendChild(descText);
      }
    }
  }
  else {
    applyButton.setAttribute("disabled", true);
  }
}



