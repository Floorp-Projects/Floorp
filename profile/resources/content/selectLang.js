function Startup()
{
  doSetOKCancel(onOK);
  var selectedLanguage = window.arguments.length ? window.arguments[0] : null;
  var tree = document.getElementById("langList");
  if (selectedLanguage) {
    var elements = tree.getElementsByAttribute("value", selectedLanguage);
    if (elements.length)
      tree.selectItem(elements[0].parentNode.parentNode);
  }
  else {
    var kids = document.getElementById("treechildren");
    tree.selectItem(kids);
  }
}

function onOK()
{
  var tree = document.getElementById("langList");
  var selectedItem = tree.selectedItems.length ? tree.selectedItems[0] : null;
  if (selectedItem) {
    var langName = selectedItem.firstChild.firstChild.getAttribute("value");
    var langStore = opener.document.getElementById("ProfileLocale");
    if (langStore)
      langStore.setAttribute("data", langName);
  }
  window.close();
}


