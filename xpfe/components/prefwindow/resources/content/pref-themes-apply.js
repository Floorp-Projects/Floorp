
// applySkin() function from Theme Selector
// ( 05/09/2000, Ben Goodger <ben@netscape.com> )

function applySkin()
{
  try {
    var chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"].getService();
    if ( chromeRegistry )
      chromeRegistry = chromeRegistry.QueryInterface( Components.interfaces.nsIChromeRegistry );
  }
  catch(e) {}

  var tree = document.getElementById( "skinsTree" );
  var selectedSkinItem = tree.selectedItems[0];
  var skinName = selectedSkinItem.getAttribute( "name" );
  dump("got skin name to apply\n");
  if (!chromeRegistry.isSkinSelected(skinName, DEBUG_USE_PROFILE)) {
    chromeRegistry.selectSkin( skinName, DEBUG_USE_PROFILE );
    chromeRegistry.refreshSkins();
  }
}

