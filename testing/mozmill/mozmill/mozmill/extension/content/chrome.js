/* Adds tooltip support to the Mozmill window. Taken from browser.js in Firefox */
function fillTooltip(tipElement) {
    var retVal = false;
    if (tipElement.namespaceURI == "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")
      return retVal;
    const XLinkNS = "http://www.w3.org/1999/xlink";
    var titleText = null;
    var XLinkTitleText = null;
    var direction = tipElement.ownerDocument.dir;

    while (!titleText && !XLinkTitleText && tipElement) {
      if (tipElement.nodeType == Node.ELEMENT_NODE) {
        titleText = tipElement.getAttribute("title");
        XLinkTitleText = tipElement.getAttributeNS(XLinkNS, "title");
        var defView = tipElement.ownerDocument.defaultView;
        // Work around bug 350679: "Tooltips can be fired in documents with no view"
        if (!defView)
          return retVal;
        direction = defView.getComputedStyle(tipElement, "")
          .getPropertyValue("direction");
      }
      tipElement = tipElement.parentNode;
    }

    var tipNode = document.getElementById("mozmill-tooltip");
    tipNode.style.direction = direction;
  
    for each (var t in [titleText, XLinkTitleText]) {
      if (t && /\S/.test(t)) {
        // Per HTML 4.01 6.2 (CDATA section), literal CRs and tabs should be
        // replaced with spaces, and LFs should be removed entirely
        t = t.replace(/[\r\t]/g, ' ');
        t = t.replace(/\n/g, '');

        tipNode.setAttribute("label", t);
        retVal = true;
      }
    }
    return retVal;
}
