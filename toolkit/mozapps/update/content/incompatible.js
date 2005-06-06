

function init() {
  var updateBundle = document.getElementById("updateBundle");
  var incompatibleItems = document.getElementById("incompatibleItems");
  var items = window.arguments[0];
  for (var i = 0; i < items.length; ++i) {
    var listitem = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
                                            "listitem");
    var itemName = updateBundle.getFormattedString("updateName", [items[i].name, items[i].version]);
    listitem.setAttribute("label", itemName);
    incompatibleItems.appendChild(listitem);
  }
  
  var closebuttonlabel = document.documentElement.getAttribute("closebuttonlabel");
  var cancelbutton = document.documentElement.getButton("cancel");
  cancelbutton.label = closebuttonlabel;
  cancelbutton.focus();
}

