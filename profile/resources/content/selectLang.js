function Startup()
{
  var selectedLanguage = window.arguments.length ? window.arguments[0] : null;
  var tree = document.getElementById("langList");
  if (selectedLanguage) {
    var elements = tree.getElementsByAttribute("value", selectedLanguage);
    if (elements.length)
      tree.selectItem(elements[0].parentNode.parentNode);
  }
  else {
      tree.selectedIndex = 0;
  }

  var selectedRegion = window.arguments.length ? window.arguments[1] : null;
  var list = document.getElementById("regionList");
  if (selectedRegion) {
    var elements = list.getElementsByAttribute("value", selectedRegion);
    if (elements.length)
      list.selectedItem = elements[0];
  } else {
      list.selectedIndex = 1;
  }
}

function onAccept()
{
  //cache language on the parent window
  var tree = document.getElementById("langList");
  var selectedItem = tree.selectedItems.length ? tree.selectedItems[0] : null;
  if (selectedItem) {
    var langName = selectedItem.firstChild.firstChild.getAttribute("value");
    var langStore = opener.document.getElementById("ProfileLanguage");
    if (langStore)
      langStore.setAttribute("data", langName);
  }

  //cache region on the parent window
  var list = document.getElementById("regionList");
  var selectedItem = list.selectedItem;
  if (selectedItem) {
    var regionName = selectedItem.getAttribute("value");
    var regionStore = opener.document.getElementById("ProfileRegion");
    if (regionStore)
      regionStore.setAttribute("data", regionName);
  }

  return true;
}
