function build_dom(n, elemName) {
  var ours = document.createElement(elemName);
  if (n != 1) {
    var leftSize = Math.floor(n / 2);
    var rightSize = Math.floor((n - 1) / 2);
    ours.appendChild(build_dom(leftSize, elemName));
    if (rightSize > 0)
      ours.appendChild(build_dom(rightSize, elemName));
  }
  return ours;
}

function build_rule(selector, selectorRepeat, declaration) {
  var s = document.createElement("style");
  s.textContent = Array(selectorRepeat).fill(selector).join(", ") + declaration;
  return s;
}
