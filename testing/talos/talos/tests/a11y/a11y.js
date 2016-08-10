gAccService = 0;

// Make sure not to touch Components before potentially invoking enablePrivilege,
// because otherwise it won't be there.
netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
nsIAccessible = Components.interfaces.nsIAccessible;
nsIDOMNode = Components.interfaces.nsIDOMNode;

function initAccessibility()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  if (!gAccService) {
    var service = Components.classes["@mozilla.org/accessibilityService;1"];
    if (service) { // fails if build lacks accessibility module
      gAccService =
      Components.classes["@mozilla.org/accessibilityService;1"]
                .getService(Components.interfaces.nsIAccessibilityService);
    }
  }
  return gAccService;
}

function getAccessible(aAccOrElmOrID, aInterfaces)
{
  if (!aAccOrElmOrID) {
    return null;
  }

  var elm = null;

  if (aAccOrElmOrID instanceof nsIAccessible) {
    elm = aAccOrElmOrID.DOMNode;

  } else if (aAccOrElmOrID instanceof nsIDOMNode) {
    elm = aAccOrElmOrID;

  } else {
    elm = document.getElementById(aAccOrElmOrID);
  }

  var acc = (aAccOrElmOrID instanceof nsIAccessible) ? aAccOrElmOrID : null;
  if (!acc) {
    try {
      acc = gAccService.getAccessibleFor(elm);
    } catch (e) {
    }
  }

  if (!aInterfaces) {
    return acc;
  }

  if (aInterfaces instanceof Array) {
    for (var index = 0; index < aInterfaces.length; index++) {
      try {
        acc.QueryInterface(aInterfaces[index]);
      } catch (e) {
      }
    }
    return acc;
  }
  
  try {
    acc.QueryInterface(aInterfaces);
  } catch (e) {
  }
  
  return acc;
}

// Walk accessible tree of the given identifier to ensure tree creation
function ensureAccessibleTree(aAccOrElmOrID)
{
  acc = getAccessible(aAccOrElmOrID);

  var child = acc.firstChild;
  while (child) {
    ensureAccessibleTree(child);
    try {
      child = child.nextSibling;
    } catch (e) {
      child = null;
    }
  }
}
