// Require selectors.js to be included before this.

/*
 * Create and append special elements that cannot be created correctly with HTML markup alone.
 */
function setupSpecialElements(parent) {
  // Setup null and undefined tests
  parent.appendChild(doc.createElement("null"));
  parent.appendChild(doc.createElement("undefined"));

  // Setup namespace tests
  var anyNS = doc.createElement("div");
  var noNS = doc.createElement("div");
  anyNS.id = "any-namespace";
  noNS.id = "no-namespace";

  var divs;
  div = [doc.createElement("div"),
         doc.createElementNS("http://www.w3.org/1999/xhtml", "div"),
         doc.createElementNS("", "div"),
         doc.createElementNS("http://www.example.org/ns", "div")];

  div[0].id = "any-namespace-div1";
  div[1].id = "any-namespace-div2";
  div[2].setAttribute("id", "any-namespace-div3"); // Non-HTML elements can't use .id property
  div[3].setAttribute("id", "any-namespace-div4");

  for (var i = 0; i < div.length; i++) {
    anyNS.appendChild(div[i])
  }

  div = [doc.createElement("div"),
         doc.createElementNS("http://www.w3.org/1999/xhtml", "div"),
         doc.createElementNS("", "div"),
         doc.createElementNS("http://www.example.org/ns", "div")];

  div[0].id = "no-namespace-div1";
  div[1].id = "no-namespace-div2";
  div[2].setAttribute("id", "no-namespace-div3"); // Non-HTML elements can't use .id property
  div[3].setAttribute("id", "no-namespace-div4");

  for (i = 0; i < div.length; i++) {
    noNS.appendChild(div[i])
  }

  parent.appendChild(anyNS);
  parent.appendChild(noNS);
}

function traverse(elem, fn) {
  if (elem.nodeType === elem.ELEMENT_NODE) {
    fn(elem);
  }
  elem = elem.firstChild;
  while (elem) {
    traverse(elem, fn);
    elem = elem.nextSibling;
  }
}

function getNodeType(node) {
  switch (node.nodeType) {
    case Node.DOCUMENT_NODE:
      return "document";
    case Node.ELEMENT_NODE:
      return node.parentNode ? "element" : "detached";
    case Node.DOCUMENT_FRAGMENT_NODE:
      return "fragment";
    default:
      console.log("Reached unreachable code path.");
      return "unknown"; // This should never happen.
  }
}
