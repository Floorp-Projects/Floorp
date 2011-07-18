netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

var gWindowUtils;

try {
  gWindowUtils = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowUtils);
  if (gWindowUtils && !gWindowUtils.compareCanvases)
    gWindowUtils = null;
} catch (e) {
  gWindowUtils = null;
}

function snapshotWindow(win, withCaret) {
  // drawWindow requires privileges, as might innerWidth/innerHeight if it's
  // a cross domain window
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var el = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  el.width = win.innerWidth;
  el.height = win.innerHeight;

  var ctx = el.getContext("2d");
  ctx.drawWindow(win, win.scrollX, win.scrollY,
                 win.innerWidth, win.innerHeight,
                 "rgb(255,255,255)",
                 withCaret ? ctx.DRAWWINDOW_DRAW_CARET : 0);
  return el;
}

// If the two snapshots don't compare as expected (true for equal, false for
// unequal), returns their serializations as data URIs.  In all cases, returns
// whether the comparison was as expected.
function compareSnapshots(s1, s2, expected) {
  netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

  var s1Str, s2Str;
  var correct = false;
  if (gWindowUtils) {
    // First, check that the canvases are the same size.
    var equal;
    if (s1.width != s2.width || s1.height != s2.height) {
      equal = false;
    } else {
      try {
        equal = (gWindowUtils.compareCanvases(s1, s2, {}) == 0);
      } catch (e) {
        equal = false;
        ok(false, "Exception thrown from compareCanvases: " + e);
      }
    }
    correct = (equal == expected);
  }

  if (!correct) {
    s1Str = s1.toDataURL();
    s2Str = s2.toDataURL();

    if (!gWindowUtils) {
	correct = ((s1Str == s2Str) == expected);
    }
  }

  return [correct, s1Str, s2Str];
}
