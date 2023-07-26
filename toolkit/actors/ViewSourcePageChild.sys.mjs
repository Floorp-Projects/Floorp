/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

// These are markers used to delimit the selection during processing. They
// are removed from the final rendering.
// We use noncharacter Unicode codepoints to minimize the risk of clashing
// with anything that might legitimately be present in the document.
// U+FDD0..FDEF <noncharacters>
const MARK_SELECTION_START = "\uFDD0";
const MARK_SELECTION_END = "\uFDEF";

/**
 * When showing selection source, chrome will construct a page fragment to
 * show, and then instruct content to draw a selection after load.  This is
 * set true when there is a pending request to draw selection.
 */
let gNeedsDrawSelection = false;

/**
 * Start at a specific line number.
 */
let gInitialLineNumber = -1;

export class ViewSourcePageChild extends JSWindowActorChild {
  constructor() {
    super();

    ChromeUtils.defineLazyGetter(this, "bundle", function () {
      return Services.strings.createBundle(BUNDLE_URL);
    });
  }

  static setNeedsDrawSelection(value) {
    gNeedsDrawSelection = value;
  }

  static setInitialLineNumber(value) {
    gInitialLineNumber = value;
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "ViewSource:GoToLine":
        this.goToLine(msg.data.lineNumber);
        break;
      case "ViewSource:IsWrapping":
        return this.isWrapping;
      case "ViewSource:IsSyntaxHighlighting":
        return this.isSyntaxHighlighting;
      case "ViewSource:ToggleWrapping":
        this.toggleWrapping();
        break;
      case "ViewSource:ToggleSyntaxHighlighting":
        this.toggleSyntaxHighlighting();
        break;
    }
    return undefined;
  }

  /**
   * Any events should get handled here, and should get dispatched to
   * a specific function for the event type.
   */
  handleEvent(event) {
    switch (event.type) {
      case "pageshow":
        this.onPageShow(event);
        break;
      case "click":
        this.onClick(event);
        break;
    }
  }

  /**
   * A shortcut to the nsISelectionController for the content.
   */
  get selectionController() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsISelectionDisplay)
      .QueryInterface(Ci.nsISelectionController);
  }

  /**
   * A shortcut to the nsIWebBrowserFind for the content.
   */
  get webBrowserFind() {
    return this.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebBrowserFind);
  }

  /**
   * This handler is for click events from:
   *   * error page content, which can show up if the user attempts to view the
   *     source of an attack page.
   */
  onClick(event) {
    let target = event.originalTarget;

    // Don't trust synthetic events
    if (!event.isTrusted || event.target.localName != "button") {
      return;
    }

    let errorDoc = target.ownerDocument;

    if (/^about:blocked/.test(errorDoc.documentURI)) {
      // The event came from a button on a malware/phishing block page

      if (target == errorDoc.getElementById("goBackButton")) {
        // Instead of loading some safe page, just close the window
        this.sendAsyncMessage("ViewSource:Close");
      }
    }
  }

  /**
   * Handler for the pageshow event.
   *
   * @param event
   *        The pageshow event being handled.
   */
  onPageShow(event) {
    // If we need to draw the selection, wait until an actual view source page
    // has loaded, instead of about:blank.
    if (
      gNeedsDrawSelection &&
      this.document.documentURI.startsWith("view-source:")
    ) {
      gNeedsDrawSelection = false;
      this.drawSelection();
    }

    if (gInitialLineNumber >= 0) {
      this.goToLine(gInitialLineNumber);
      gInitialLineNumber = -1;
    }
  }

  /**
   * Attempts to go to a particular line in the source code being
   * shown. If it succeeds in finding the line, it will fire a
   * "ViewSource:GoToLine:Success" message, passing up an object
   * with the lineNumber we just went to. If it cannot find the line,
   * it will fire a "ViewSource:GoToLine:Failed" message.
   *
   * @param lineNumber
   *        The line number to attempt to go to.
   */
  goToLine(lineNumber) {
    let body = this.document.body;

    // The source document is made up of a number of pre elements with
    // id attributes in the format <pre id="line123">, meaning that
    // the first line in the pre element is number 123.
    // Do binary search to find the pre element containing the line.
    // However, in the plain text case, we have only one pre without an
    // attribute, so assume it begins on line 1.
    let pre;
    for (let lbound = 0, ubound = body.childNodes.length; ; ) {
      let middle = (lbound + ubound) >> 1;
      pre = body.childNodes[middle];

      let firstLine = pre.id ? parseInt(pre.id.substring(4)) : 1;

      if (lbound == ubound - 1) {
        break;
      }

      if (lineNumber >= firstLine) {
        lbound = middle;
      } else {
        ubound = middle;
      }
    }

    let result = {};
    let found = this.findLocation(pre, lineNumber, null, -1, false, result);

    if (!found) {
      this.sendAsyncMessage("ViewSource:GoToLine:Failed");
      return;
    }

    let selection = this.document.defaultView.getSelection();
    selection.removeAllRanges();

    // In our case, the range's startOffset is after "\n" on the previous line.
    // Tune the selection at the beginning of the next line and do some tweaking
    // to position the focusNode and the caret at the beginning of the line.
    selection.interlinePosition = true;

    selection.addRange(result.range);

    if (!selection.isCollapsed) {
      selection.collapseToEnd();

      let offset = result.range.startOffset;
      let node = result.range.startContainer;
      if (offset < node.data.length) {
        // The same text node spans across the "\n", just focus where we were.
        selection.extend(node, offset);
      } else {
        // There is another tag just after the "\n", hook there. We need
        // to focus a safe point because there are edgy cases such as
        // <span>...\n</span><span>...</span> vs.
        // <span>...\n<span>...</span></span><span>...</span>
        node = node.nextSibling
          ? node.nextSibling
          : node.parentNode.nextSibling;
        selection.extend(node, 0);
      }
    }

    let selCon = this.selectionController;
    selCon.setDisplaySelection(Ci.nsISelectionController.SELECTION_ON);
    selCon.setCaretVisibilityDuringSelection(true);

    // Scroll the beginning of the line into view.
    selCon.scrollSelectionIntoView(
      Ci.nsISelectionController.SELECTION_NORMAL,
      Ci.nsISelectionController.SELECTION_FOCUS_REGION,
      true
    );

    this.sendAsyncMessage("ViewSource:GoToLine:Success", { lineNumber });
  }

  /**
   * Some old code from the original view source implementation. Original
   * documentation follows:
   *
   * "Loops through the text lines in the pre element. The arguments are either
   *  (pre, line) or (node, offset, interlinePosition). result is an out
   *  argument. If (pre, line) are specified (and node == null), result.range is
   *  a range spanning the specified line. If the (node, offset,
   *  interlinePosition) are specified, result.line and result.col are the line
   *  and column number of the specified offset in the specified node relative to
   *  the whole file."
   */
  findLocation(pre, lineNumber, node, offset, interlinePosition, result) {
    if (node && !pre) {
      // Look upwards to find the current pre element.
      // eslint-disable-next-line no-empty
      for (pre = node; pre.nodeName != "PRE"; pre = pre.parentNode) {}
    }

    // The source document is made up of a number of pre elements with
    // id attributes in the format <pre id="line123">, meaning that
    // the first line in the pre element is number 123.
    // However, in the plain text case, there is only one <pre> without an id,
    // so assume line 1.
    let curLine = pre.id ? parseInt(pre.id.substring(4)) : 1;

    // Walk through each of the text nodes and count newlines.
    let treewalker = this.document.createTreeWalker(
      pre,
      NodeFilter.SHOW_TEXT,
      null
    );

    // The column number of the first character in the current text node.
    let firstCol = 1;

    let found = false;
    for (
      let textNode = treewalker.firstChild();
      textNode && !found;
      textNode = treewalker.nextNode()
    ) {
      // \r is not a valid character in the DOM, so we only check for \n.
      let lineArray = textNode.data.split(/\n/);
      let lastLineInNode = curLine + lineArray.length - 1;

      // Check if we can skip the text node without further inspection.
      if (node ? textNode != node : lastLineInNode < lineNumber) {
        if (lineArray.length > 1) {
          firstCol = 1;
        }
        firstCol += lineArray[lineArray.length - 1].length;
        curLine = lastLineInNode;
        continue;
      }

      // curPos is the offset within the current text node of the first
      // character in the current line.
      for (
        var i = 0, curPos = 0;
        i < lineArray.length;
        curPos += lineArray[i++].length + 1
      ) {
        if (i > 0) {
          curLine++;
        }

        if (node) {
          if (offset >= curPos && offset <= curPos + lineArray[i].length) {
            // If we are right after the \n of a line and interlinePosition is
            // false, the caret looks as if it were at the end of the previous
            // line, so we display that line and column instead.

            if (i > 0 && offset == curPos && !interlinePosition) {
              result.line = curLine - 1;
              var prevPos = curPos - lineArray[i - 1].length;
              result.col = (i == 1 ? firstCol : 1) + offset - prevPos;
            } else {
              result.line = curLine;
              result.col = (i == 0 ? firstCol : 1) + offset - curPos;
            }
            found = true;

            break;
          }
        } else if (curLine == lineNumber && !("range" in result)) {
          result.range = this.document.createRange();
          result.range.setStart(textNode, curPos);

          // This will always be overridden later, except when we look for
          // the very last line in the file (this is the only line that does
          // not end with \n).
          result.range.setEndAfter(pre.lastChild);
        } else if (curLine == lineNumber + 1) {
          result.range.setEnd(textNode, curPos - 1);
          found = true;
          break;
        }
      }
    }

    return found || "range" in result;
  }

  /**
   * @return {boolean} whether the "wrap" class exists on the document body.
   */
  get isWrapping() {
    return this.document.body.classList.contains("wrap");
  }

  /**
   * @return {boolean} whether the "highlight" class exists on the document body.
   */
  get isSyntaxHighlighting() {
    return this.document.body.classList.contains("highlight");
  }

  /**
   * Toggles the "wrap" class on the document body, which sets whether
   * or not long lines are wrapped.  Notifies parent to update the pref.
   */
  toggleWrapping() {
    let body = this.document.body;
    let state = body.classList.toggle("wrap");
    this.sendAsyncMessage("ViewSource:StoreWrapping", { state });
  }

  /**
   * Toggles the "highlight" class on the document body, which sets whether
   * or not syntax highlighting is displayed.  Notifies parent to update the
   * pref.
   */
  toggleSyntaxHighlighting() {
    let body = this.document.body;
    let state = body.classList.toggle("highlight");
    this.sendAsyncMessage("ViewSource:StoreSyntaxHighlighting", { state });
  }

  /**
   * Using special markers left in the serialized source, this helper makes the
   * underlying markup of the selected fragment to automatically appear as
   * selected on the inflated view-source DOM.
   */
  drawSelection() {
    this.document.title = this.bundle.GetStringFromName(
      "viewSelectionSourceTitle"
    );

    // find the special selection markers that we added earlier, and
    // draw the selection between the two...
    var findService = null;
    try {
      // get the find service which stores the global find state
      findService = Cc["@mozilla.org/find/find_service;1"].getService(
        Ci.nsIFindService
      );
    } catch (e) {}
    if (!findService) {
      return;
    }

    // cache the current global find state
    var matchCase = findService.matchCase;
    var entireWord = findService.entireWord;
    var wrapFind = findService.wrapFind;
    var findBackwards = findService.findBackwards;
    var searchString = findService.searchString;
    var replaceString = findService.replaceString;

    // setup our find instance
    var findInst = this.webBrowserFind;
    findInst.matchCase = true;
    findInst.entireWord = false;
    findInst.wrapFind = true;
    findInst.findBackwards = false;

    // ...lookup the start mark
    findInst.searchString = MARK_SELECTION_START;
    var startLength = MARK_SELECTION_START.length;
    findInst.findNext();

    var selection = this.document.defaultView.getSelection();
    if (!selection.rangeCount) {
      return;
    }

    var range = selection.getRangeAt(0);

    var startContainer = range.startContainer;
    var startOffset = range.startOffset;

    // ...lookup the end mark
    findInst.searchString = MARK_SELECTION_END;
    var endLength = MARK_SELECTION_END.length;
    findInst.findNext();

    var endContainer = selection.anchorNode;
    var endOffset = selection.anchorOffset;

    // reset the selection that find has left
    selection.removeAllRanges();

    // delete the special markers now...
    endContainer.deleteData(endOffset, endLength);
    startContainer.deleteData(startOffset, startLength);
    if (startContainer == endContainer) {
      endOffset -= startLength;
    } // has shrunk if on same text node...
    range.setEnd(endContainer, endOffset);

    // show the selection and scroll it into view
    selection.addRange(range);
    // the default behavior of the selection is to scroll at the end of
    // the selection, whereas in this situation, it is more user-friendly
    // to scroll at the beginning. So we override the default behavior here
    try {
      this.selectionController.scrollSelectionIntoView(
        Ci.nsISelectionController.SELECTION_NORMAL,
        Ci.nsISelectionController.SELECTION_ANCHOR_REGION,
        true
      );
    } catch (e) {}

    // restore the current find state
    findService.matchCase = matchCase;
    findService.entireWord = entireWord;
    findService.wrapFind = wrapFind;
    findService.findBackwards = findBackwards;
    findService.searchString = searchString;
    findService.replaceString = replaceString;

    findInst.matchCase = matchCase;
    findInst.entireWord = entireWord;
    findInst.wrapFind = wrapFind;
    findInst.findBackwards = findBackwards;
    findInst.searchString = searchString;
  }
}
