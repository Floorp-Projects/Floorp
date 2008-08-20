netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

var gWindowSnapshotCompareHelper;

try {
  gWindowSnapshotCompareHelper =
    Components.classes["@mozilla.org/reftest-helper;1"]
              .getService(Components.interfaces.nsIReftestHelper);
} catch (e) {
  gWindowSnapshotCompareHelper = null;
}

function snapshotWindow(win) {
  var el = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  el.width = win.innerWidth;
  el.height = win.innerHeight;

  // drawWindow requires privileges
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
  
  el.getContext("2d").drawWindow(win, win.scrollX, win.scrollY,
				 win.innerWidth, win.innerHeight,
				 "rgb(255,255,255)");
  return el;
}

// If the two snapshots aren't equal, returns their serializations as data URIs.
function compareSnapshots(s1, s2) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var s1Str, s2Str;
  var equal = false;
  if (gWindowSnapshotCompareHelper) {
    equal = (gWindowSnapshotCompareHelper.compareCanvas(s1, s2) == 0);
  }

  if (!equal) {
    s1Str = s1.toDataURL();
    s2Str = s2.toDataURL();

    if (!gWindowSnapshotCompareHelper) {
      equal = (s1Str == s2Str);
    }
  }

  return [equal, s1Str, s2Str];
}
