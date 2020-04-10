/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabModalPrompt"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

var TabModalPrompt = class {
  constructor(win) {
    this.win = win;
    let newPrompt = (this.element = win.document.createXULElement(
      "tabmodalprompt"
    ));
    newPrompt.setAttribute("role", "dialog");
    let randomIdSuffix = Math.random()
      .toString(32)
      .substr(2);
    newPrompt.setAttribute("aria-describedby", `infoBody-${randomIdSuffix}`);
    newPrompt.appendChild(
      win.MozXULElement.parseXULToFragment(
        `
      <spacer flex="1"/>
        <hbox pack="center">
          <vbox class="tabmodalprompt-mainContainer">
            <grid class="tabmodalprompt-topContainer" flex="1">
              <columns>
                <column/>
                <column flex="1"/>
              </columns>

              <rows class="tabmodalprompt-rows">
                <vbox class="tabmodalprompt-infoContainer" align="center" pack="center" flex="1">
                  <description class="tabmodalprompt-infoTitle infoTitle" hidden="true" />
                  <description class="tabmodalprompt-infoBody infoBody" id="infoBody-${randomIdSuffix}"/>
                </vbox>

                <row class="tabmodalprompt-loginContainer" hidden="true" align="center">
                  <label class="tabmodalprompt-loginLabel" value="&editfield0.label;" control="loginTextbox-${randomIdSuffix}"/>
                  <html:input class="tabmodalprompt-loginTextbox" id="loginTextbox-${randomIdSuffix}"/>
                </row>

                <row class="tabmodalprompt-password1Container" hidden="true" align="center">
                  <label class="tabmodalprompt-password1Label" value="&editfield1.label;" control="password1Textbox-${randomIdSuffix}"/>
                  <html:input class="tabmodalprompt-password1Textbox" type="password" id="password1Textbox-${randomIdSuffix}"/>
                </row>

                <row class="tabmodalprompt-checkboxContainer" hidden="true">
                  <spacer/>
                  <checkbox class="tabmodalprompt-checkbox"/>
                </row>

                <!-- content goes here -->
              </rows>
            </grid>
            <hbox class="tabmodalprompt-buttonContainer">
              <button class="tabmodalprompt-button3" hidden="true"/>
              <spacer class="tabmodalprompt-buttonSpacer" flex="1"/>
              <button class="tabmodalprompt-button0" label="&okButton.label;"/>
              <button class="tabmodalprompt-button2" hidden="true"/>
              <button class="tabmodalprompt-button1" label="&cancelButton.label;"/>
            </hbox>
          </vbox>
      </hbox>
      <spacer flex="2"/>
    `,
        [
          "chrome://global/locale/commonDialog.dtd",
          "chrome://global/locale/dialogOverlay.dtd",
        ]
      )
    );

    this.ui = {
      prompt: this,
      promptContainer: this.element,
      mainContainer: newPrompt.querySelector(".tabmodalprompt-mainContainer"),
      loginContainer: newPrompt.querySelector(".tabmodalprompt-loginContainer"),
      loginTextbox: newPrompt.querySelector(".tabmodalprompt-loginTextbox"),
      loginLabel: newPrompt.querySelector(".tabmodalprompt-loginLabel"),
      password1Container: newPrompt.querySelector(
        ".tabmodalprompt-password1Container"
      ),
      password1Textbox: newPrompt.querySelector(
        ".tabmodalprompt-password1Textbox"
      ),
      password1Label: newPrompt.querySelector(".tabmodalprompt-password1Label"),
      infoContainer: newPrompt.querySelector(".tabmodalprompt-infoContainer"),
      infoBody: newPrompt.querySelector(".tabmodalprompt-infoBody"),
      infoTitle: newPrompt.querySelector(".tabmodalprompt-infoTitle"),
      infoIcon: null,
      rows: newPrompt.querySelector(".tabmodalprompt-rows"),
      checkbox: newPrompt.querySelector(".tabmodalprompt-checkbox"),
      checkboxContainer: newPrompt.querySelector(
        ".tabmodalprompt-checkboxContainer"
      ),
      button3: newPrompt.querySelector(".tabmodalprompt-button3"),
      button2: newPrompt.querySelector(".tabmodalprompt-button2"),
      button1: newPrompt.querySelector(".tabmodalprompt-button1"),
      button0: newPrompt.querySelector(".tabmodalprompt-button0"),
      // focusTarget (for BUTTON_DELAY_ENABLE) not yet supported
    };

    if (AppConstants.XP_UNIX) {
      // Reorder buttons on Linux
      let buttonContainer = newPrompt.querySelector(
        ".tabmodalprompt-buttonContainer"
      );
      buttonContainer.appendChild(this.ui.button3);
      buttonContainer.appendChild(this.ui.button2);
      buttonContainer.appendChild(
        newPrompt.querySelector(".tabmodalprompt-buttonSpacer")
      );
      buttonContainer.appendChild(this.ui.button1);
      buttonContainer.appendChild(this.ui.button0);
    }

    this.ui.button0.addEventListener(
      "command",
      this.onButtonClick.bind(this, 0)
    );
    this.ui.button1.addEventListener(
      "command",
      this.onButtonClick.bind(this, 1)
    );
    this.ui.button2.addEventListener(
      "command",
      this.onButtonClick.bind(this, 2)
    );
    this.ui.button3.addEventListener(
      "command",
      this.onButtonClick.bind(this, 3)
    );
    // Anonymous wrapper used here because |Dialog| doesn't exist until init() is called!
    this.ui.checkbox.addEventListener("command", () => {
      this.Dialog.onCheckbox();
    });

    /**
     * Based on dialog.xml handlers
     */
    this.element.addEventListener(
      "keypress",
      event => {
        switch (event.keyCode) {
          case KeyEvent.DOM_VK_RETURN:
            this.onKeyAction("default", event);
            break;

          case KeyEvent.DOM_VK_ESCAPE:
            this.onKeyAction("cancel", event);
            break;

          default:
            if (
              AppConstants.platform == "macosx" &&
              event.key == "." &&
              event.metaKey
            ) {
              this.onKeyAction("cancel", event);
            }
            break;
        }
      },
      { mozSystemGroup: true }
    );

    this.element.addEventListener(
      "focus",
      event => {
        let bnum = this.args.defaultButtonNum || 0;
        let defaultButton = this.ui["button" + bnum];

        if (AppConstants.platform == "macosx") {
          // On OS X, the default button always stays marked as such (until
          // the entire prompt blurs).
          defaultButton.setAttribute("default", "true");
        } else {
          // On other platforms, the default button is only marked as such
          // when no other button has focus. XUL buttons on not-OSX will
          // react to pressing enter as a command, so you can't trigger the
          // default without tabbing to it or something that isn't a button.
          let focusedDefault = event.originalTarget == defaultButton;
          let someButtonFocused =
            event.originalTarget.localName == "button" ||
            event.originalTarget.localName == "toolbarbutton";
          if (focusedDefault || !someButtonFocused) {
            defaultButton.setAttribute("default", "true");
          }
        }
      },
      true
    );

    this.element.addEventListener("blur", () => {
      // If focus shifted to somewhere else in the browser, don't make
      // the default button look active.
      let bnum = this.args.defaultButtonNum || 0;
      let button = this.ui["button" + bnum];
      button.removeAttribute("default");
    });
  }

  init(args, linkedTab, onCloseCallback) {
    this.args = args;
    this.linkedTab = linkedTab;
    this.onCloseCallback = onCloseCallback;

    if (args.enableDelay) {
      throw new Error(
        "BUTTON_DELAY_ENABLE not yet supported for tab-modal prompts"
      );
    }

    // We need to remove the prompt when the tab or browser window is closed or
    // the page navigates, else we never unwind the event loop and that's sad times.
    // Remember to cleanup in shutdownPrompt()!
    this.win.addEventListener("resize", this);
    this.win.addEventListener("unload", this);
    if (linkedTab) {
      linkedTab.addEventListener("TabClose", this);
    }
    // Note:
    // nsPrompter.js or in e10s mode browser-parent.js call abortPrompt,
    // when the domWindow, for which the prompt was created, generates
    // a "pagehide" event.

    let tmp = {};
    ChromeUtils.import("resource://gre/modules/CommonDialog.jsm", tmp);
    this.Dialog = new tmp.CommonDialog(args, this.ui);
    this.Dialog.onLoad(null);

    // Display the tabprompt title that shows the prompt origin when
    // the prompt origin is not the same as that of the top window.
    if (!args.showAlertOrigin) {
      this.ui.infoTitle.removeAttribute("hidden");
    }

    // TODO: should unhide buttonSpacer on Windows when there are 4 buttons.
    //       Better yet, just drop support for 4-button dialogs. (bug 609510)

    this.onResize();
  }

  shutdownPrompt() {
    // remove our event listeners
    try {
      this.win.removeEventListener("resize", this);
      this.win.removeEventListener("unload", this);
      if (this.linkedTab) {
        this.linkedTab.removeEventListener("TabClose", this);
      }
    } catch (e) {}
    // invoke callback
    this.onCloseCallback();
    this.win = null;
    this.ui = null;
    // Intentionally not cleaning up |this.element| here --
    // TabModalPromptBox.removePrompt() would need it and it might not
    // be called yet -- see browser_double_close_tabs.js.
  }

  abortPrompt() {
    // Called from other code when the page changes.
    this.Dialog.abortPrompt();
    this.shutdownPrompt();
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "resize":
        this.onResize();
        break;
      case "unload":
      case "TabClose":
        this.abortPrompt();
        break;
    }
  }

  onResize() {
    let availWidth = this.element.clientWidth;
    let availHeight = this.element.clientHeight;
    if (availWidth == this.availWidth && availHeight == this.availHeight) {
      return;
    }
    this.availWidth = availWidth;
    this.availHeight = availHeight;

    let main = this.ui.mainContainer;
    let info = this.ui.infoContainer;
    let body = this.ui.infoBody;

    // cap prompt dimensions at 60% width and 60% height of content area
    if (!this.minWidth) {
      this.minWidth = parseInt(this.win.getComputedStyle(main).minWidth);
    }
    if (!this.minHeight) {
      this.minHeight = parseInt(this.win.getComputedStyle(main).minHeight);
    }
    let maxWidth =
      Math.max(Math.floor(availWidth * 0.6), this.minWidth) +
      info.clientWidth -
      main.clientWidth;
    let maxHeight =
      Math.max(Math.floor(availHeight * 0.6), this.minHeight) +
      info.clientHeight -
      main.clientHeight;
    body.style.maxWidth = maxWidth + "px";
    info.style.overflow = info.style.width = info.style.height = "";

    // when prompt text is too long, use scrollbars
    if (info.clientWidth > maxWidth) {
      info.style.overflow = "auto";
      info.style.width = maxWidth + "px";
    }
    if (info.clientHeight > maxHeight) {
      info.style.overflow = "auto";
      info.style.height = maxHeight + "px";
    }
  }

  onButtonClick(buttonNum) {
    // We want to do all the work her asynchronously off a Gecko
    // runnable, because of situations like the one described in
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1167575#c35 : we
    // get here off processing of an OS event and will also process
    // one more Gecko runnable before we break out of the event loop
    // spin whoever posted the prompt is doing.  If we do all our
    // work sync, we will exit modal state _before_ processing that
    // runnable, and if exiting moral state posts a runnable we will
    // incorrectly process that runnable before leaving our event
    // loop spin.
    Services.tm.dispatchToMainThread(() => {
      this.Dialog["onButton" + buttonNum]();
      this.shutdownPrompt();
    });
  }

  onKeyAction(action, event) {
    if (event.defaultPrevented) {
      return;
    }

    event.stopPropagation();
    if (action == "default") {
      let bnum = this.args.defaultButtonNum || 0;
      this.onButtonClick(bnum);
    } else {
      // action == "cancel"
      this.onButtonClick(1); // Cancel button
    }
  }
};
