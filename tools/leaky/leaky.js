var lastIn;
function I(event) {
  var it = event.target;
  if (it) {
    var s = it.src;
    var s1 = s.replace(".gif", "-over.gif");
    it.src = s1;
    lastIn = it;
  }
}
function O(event) {
  var it = lastIn;
  if (it) {
    var s = it.src;
    var s1 = s.replace("-over", "");
    it.src = s1;
    lastIn = null;
  }
}
function C(event) {
  var it = event.target;
  if (!it) return;
  var kids = it.parentNode.childNodes;
  if (!kids) return;
  for (var i = 0; i < kids.length; i++) {
    var kid = kids[i];
    if ((kid.nodeType == Node.ELEMENT_NODE) && (kid.tagName == "DIV")) {
      var d = kid.style.display;
      if ((d == "") || (d == null) || (d == "none")) {
        it.src = it.src.replace("close", "open");
        kid.style.display = "block";
      } else {
        kid.style.display = "none";
        it.src = it.src.replace("open", "close");
      }
    }
  }
}
