// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BUNDLE_URL = "chrome://global/locale/viewSource.properties";

/**
 * ViewSourcePageParent manages the view source <browser> from the chrome side.
 */
export class ViewSourcePageParent extends JSWindowActorParent {
  constructor() {
    super();

    /**
     * Holds the value of the last line found via the "Go to line"
     * command, to pre-populate the prompt the next time it is
     * opened.
     */
    this.lastLineFound = null;
  }

  /**
   * Anything added to the messages array will get handled here, and should
   * get dispatched to a specific function for the message name.
   */
  receiveMessage(message) {
    let data = message.data;

    switch (message.name) {
      case "ViewSource:PromptAndGoToLine":
        this.promptAndGoToLine();
        break;
      case "ViewSource:GoToLine:Success":
        this.onGoToLineSuccess(data.lineNumber);
        break;
      case "ViewSource:GoToLine:Failed":
        this.onGoToLineFailed();
        break;
      case "ViewSource:StoreWrapping":
        this.storeWrapping(data.state);
        break;
      case "ViewSource:StoreSyntaxHighlighting":
        this.storeSyntaxHighlighting(data.state);
        break;
    }
  }

  /**
   * A getter for the view source string bundle.
   */
  get bundle() {
    if (this._bundle) {
      return this._bundle;
    }
    return (this._bundle = Services.strings.createBundle(BUNDLE_URL));
  }

  /**
   * Opens the "Go to line" prompt for a user to hop to a particular line
   * of the source code they're viewing. This will keep prompting until the
   * user either cancels out of the prompt, or enters a valid line number.
   */
  promptAndGoToLine() {
    let input = { value: this.lastLineFound };
    let window = Services.wm.getMostRecentWindow(null);

    let ok = Services.prompt.prompt(
      window,
      this.bundle.GetStringFromName("goToLineTitle"),
      this.bundle.GetStringFromName("goToLineText"),
      input,
      null,
      { value: 0 }
    );

    if (!ok) {
      return;
    }

    let line = parseInt(input.value, 10);

    if (!(line > 0)) {
      Services.prompt.alert(
        window,
        this.bundle.GetStringFromName("invalidInputTitle"),
        this.bundle.GetStringFromName("invalidInputText")
      );
      this.promptAndGoToLine();
    } else {
      this.goToLine(line);
    }
  }

  /**
   * Go to a particular line of the source code. This act is asynchronous.
   *
   * @param lineNumber
   *        The line number to try to go to to.
   */
  goToLine(lineNumber) {
    this.sendAsyncMessage("ViewSource:GoToLine", { lineNumber });
  }

  /**
   * Called when the frame script reports that a line was successfully gotten
   * to.
   *
   * @param lineNumber
   *        The line number that we successfully got to.
   */
  onGoToLineSuccess(lineNumber) {
    // We'll pre-populate the "Go to line" prompt with this value the next
    // time it comes up.
    this.lastLineFound = lineNumber;
  }

  /**
   * Called when the child reports that we failed to go to a particular
   * line. This informs the user that their selection was likely out of range,
   * and then reprompts the user to try again.
   */
  onGoToLineFailed() {
    let window = Services.wm.getMostRecentWindow(null);
    Services.prompt.alert(
      window,
      this.bundle.GetStringFromName("outOfRangeTitle"),
      this.bundle.GetStringFromName("outOfRangeText")
    );
    this.promptAndGoToLine();
  }

  /**
   * @return {boolean} the wrapping state
   */
  queryIsWrapping() {
    return this.sendQuery("ViewSource:IsWrapping");
  }

  /**
   * @return {boolean} the syntax highlighting state
   */
  queryIsSyntaxHighlighting() {
    return this.sendQuery("ViewSource:IsSyntaxHighlighting");
  }

  /**
   * Update the wrapping pref based on the child's current state.
   * @param state
   *        Whether wrapping is currently enabled in the child.
   */
  storeWrapping(state) {
    Services.prefs.setBoolPref("view_source.wrap_long_lines", state);
  }

  /**
   * Update the syntax highlighting pref based on the child's current state.
   * @param state
   *        Whether syntax highlighting is currently enabled in the child.
   */
  storeSyntaxHighlighting(state) {
    Services.prefs.setBoolPref("view_source.syntax_highlight", state);
  }
}
