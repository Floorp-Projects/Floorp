/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  GeckoViewLoginStorage: "resource://gre/modules/GeckoViewLoginStorage.jsm",
  LoginEntry: "resource://gre/modules/GeckoViewLoginStorage.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "UUIDGen",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

const domBundle = Services.strings.createBundle(
  "chrome://global/locale/dom/dom.properties"
);

function PromptFactory() {
  this.wrappedJSObject = this;
}

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPrompt"); // eslint-disable-line no-unused-vars

PromptFactory.prototype = {
  classID: Components.ID("{076ac188-23c1-4390-aa08-7ef1f78ca5d9}"),

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIPromptFactory,
    Ci.nsIPromptService,
  ]),

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        this._handleClick(aEvent);
        break;
      case "contextmenu":
        this._handleContextMenu(aEvent);
        break;
      case "DOMPopupBlocked":
        this._handlePopupBlocked(aEvent);
        break;
    }
  },

  _handleClick(aEvent) {
    const target = aEvent.composedTarget;
    if (
      target.isContentEditable ||
      target.disabled ||
      target.readOnly ||
      !target.willValidate
    ) {
      // target.willValidate is false when any associated fieldset is disabled,
      // in which case this element is treated as disabled per spec.
      return;
    }

    const win = target.ownerGlobal;
    if (target instanceof win.HTMLSelectElement) {
      this._handleSelect(target);
      aEvent.preventDefault();
    } else if (target instanceof win.HTMLInputElement) {
      const type = target.type;
      if (
        type === "date" ||
        type === "month" ||
        type === "week" ||
        type === "time" ||
        type === "datetime-local"
      ) {
        this._handleDateTime(target, type);
        aEvent.preventDefault();
      }
    }
  },

  _handleSelect(aElement) {
    const win = aElement.ownerGlobal;
    let id = 0;
    const map = {};

    const items = (function enumList(elem, disabled) {
      const items = [];
      const children = elem.children;
      for (let i = 0; i < children.length; i++) {
        const child = children[i];
        if (win.getComputedStyle(child).display === "none") {
          continue;
        }
        const item = {
          id: String(id),
          disabled: disabled || child.disabled,
        };
        if (child instanceof win.HTMLOptGroupElement) {
          item.label = child.label;
          item.items = enumList(child, item.disabled);
        } else if (child instanceof win.HTMLOptionElement) {
          item.label = child.label || child.text;
          item.selected = child.selected;
        } else {
          continue;
        }
        items.push(item);
        map[id++] = child;
      }
      return items;
    })(aElement);

    const prompt = new PromptDelegate(win);
    prompt.asyncShowPrompt(
      {
        type: "choice",
        mode: aElement.multiple ? "multiple" : "single",
        choices: items,
      },
      result => {
        // OK: result
        // Cancel: !result
        if (!result || result.choices === undefined) {
          return;
        }

        let dispatchEvents = false;
        if (!aElement.multiple) {
          const elem = map[result.choices[0]];
          if (elem && elem instanceof win.HTMLOptionElement) {
            dispatchEvents = !elem.selected;
            elem.selected = true;
          } else {
            Cu.reportError(
              "Invalid id for select result: " + result.choices[0]
            );
          }
        } else {
          for (let i = 0; i < id; i++) {
            const elem = map[i];
            const index = result.choices.indexOf(String(i));
            if (
              elem instanceof win.HTMLOptionElement &&
              elem.selected !== index >= 0
            ) {
              // Current selected is not the same as the new selected state.
              dispatchEvents = true;
              elem.selected = !elem.selected;
            }
            result.choices[index] = undefined;
          }
          for (let i = 0; i < result.choices.length; i++) {
            if (result.choices[i] !== undefined && result.choices[i] !== null) {
              Cu.reportError(
                "Invalid id for select result: " + result.choices[i]
              );
              break;
            }
          }
        }

        if (dispatchEvents) {
          this._dispatchEvents(aElement);
        }
      }
    );
  },

  _handleDateTime(aElement, aType) {
    const prompt = new PromptDelegate(aElement.ownerGlobal);
    prompt.asyncShowPrompt(
      {
        type: "datetime",
        mode: aType,
        value: aElement.value,
        min: aElement.min,
        max: aElement.max,
      },
      result => {
        // OK: result
        // Cancel: !result
        if (
          !result ||
          result.datetime === undefined ||
          result.datetime === aElement.value
        ) {
          return;
        }
        aElement.value = result.datetime;
        this._dispatchEvents(aElement);
      }
    );
  },

  _dispatchEvents(aElement) {
    // Fire both "input" and "change" events for <select> and <input> for
    // date/time.
    aElement.dispatchEvent(
      new aElement.ownerGlobal.Event("input", { bubbles: true })
    );
    aElement.dispatchEvent(
      new aElement.ownerGlobal.Event("change", { bubbles: true })
    );
  },

  _handleContextMenu(aEvent) {
    const target = aEvent.composedTarget;
    if (aEvent.defaultPrevented || target.isContentEditable) {
      return;
    }

    // Look through all ancestors for a context menu per spec.
    let parent = target;
    let menu = target.contextMenu;
    while (!menu && parent) {
      menu = parent.contextMenu;
      parent = parent.parentElement;
    }
    if (!menu) {
      return;
    }

    const builder = {
      _cursor: undefined,
      _id: 0,
      _map: {},
      _stack: [],
      items: [],

      // nsIMenuBuilder
      openContainer(aLabel) {
        if (!this._cursor) {
          // Top-level
          this._cursor = this;
          return;
        }
        const newCursor = {
          id: String(this._id++),
          items: [],
          label: aLabel,
        };
        this._cursor.items.push(newCursor);
        this._stack.push(this._cursor);
        this._cursor = newCursor;
      },

      addItemFor(aElement, aCanLoadIcon) {
        this._cursor.items.push({
          disabled: aElement.disabled,
          icon:
            aCanLoadIcon && aElement.icon && aElement.icon.length
              ? aElement.icon
              : null,
          id: String(this._id),
          label: aElement.label,
          selected: aElement.checked,
        });
        this._map[this._id++] = aElement;
      },

      addSeparator() {
        this._cursor.items.push({
          disabled: true,
          id: String(this._id++),
          separator: true,
        });
      },

      undoAddSeparator() {
        const sep = this._cursor.items[this._cursor.items.length - 1];
        if (sep && sep.separator) {
          this._cursor.items.pop();
        }
      },

      closeContainer() {
        const childItems =
          this._cursor.label === "" ? this._cursor.items : null;
        this._cursor = this._stack.pop();

        if (
          childItems !== null &&
          this._cursor &&
          this._cursor.items.length === 1
        ) {
          // Merge a single nameless child container into the parent container.
          // This lets us build an HTML contextmenu within a submenu.
          this._cursor.items = childItems;
        }
      },

      toJSONString() {
        return JSON.stringify(this.items);
      },

      click(aId) {
        const item = this._map[aId];
        if (item) {
          item.click();
        }
      },
    };

    // XXX the "show" event is not cancelable but spec says it should be.
    menu.sendShowEvent();
    menu.build(builder);

    const prompt = new PromptDelegate(target.ownerGlobal);
    prompt.asyncShowPrompt(
      {
        type: "choice",
        mode: "menu",
        choices: builder.items,
      },
      result => {
        // OK: result
        // Cancel: !result
        if (result && result.choices !== undefined) {
          builder.click(result.choices[0]);
        }
      }
    );

    aEvent.preventDefault();
  },

  _handlePopupBlocked(aEvent) {
    const dwi = aEvent.requestingWindow;
    const popupWindowURISpec = aEvent.popupWindowURI
      ? aEvent.popupWindowURI.displaySpec
      : "about:blank";

    const prompt = new PromptDelegate(aEvent.requestingWindow);
    prompt.asyncShowPrompt(
      {
        type: "popup",
        targetUri: popupWindowURISpec,
      },
      ({ response }) => {
        if (response && dwi) {
          dwi.open(
            popupWindowURISpec,
            aEvent.popupWindowName,
            aEvent.popupWindowFeatures
          );
        }
      }
    );
  },

  /* ----------  nsIPromptFactory  ---------- */
  getPrompt(aDOMWin, aIID) {
    // Delegated to login manager here, which in turn calls back into us via nsIPromptService.
    if (aIID.equals(Ci.nsIAuthPrompt2) || aIID.equals(Ci.nsIAuthPrompt)) {
      try {
        const pwmgr = Cc[
          "@mozilla.org/passwordmanager/authpromptfactory;1"
        ].getService(Ci.nsIPromptFactory);
        return pwmgr.getPrompt(aDOMWin, aIID);
      } catch (e) {
        Cu.reportError("Delegation to password manager failed: " + e);
      }
    }

    const p = new PromptDelegate(aDOMWin);
    p.QueryInterface(aIID);
    return p;
  },

  /* ----------  private memebers  ---------- */

  // nsIPromptService methods proxy to our Prompt class
  callProxy(aMethod, aArguments) {
    const prompt = new PromptDelegate(aArguments[0]);
    let promptArgs;
    if (aArguments[0] instanceof BrowsingContext) {
      // Called by BrowsingContext prompt method, strip modalType.
      [, , /*browsingContext*/ /*modalType*/ ...promptArgs] = aArguments;
    } else {
      [, /*domWindow*/ ...promptArgs] = aArguments;
    }
    return prompt[aMethod].apply(prompt, promptArgs);
  },

  /* ----------  nsIPromptService  ---------- */

  alert() {
    return this.callProxy("alert", arguments);
  },
  alertBC() {
    return this.callProxy("alert", arguments);
  },
  alertCheck() {
    return this.callProxy("alertCheck", arguments);
  },
  alertCheckBC() {
    return this.callProxy("alertCheck", arguments);
  },
  confirm() {
    return this.callProxy("confirm", arguments);
  },
  confirmBC() {
    return this.callProxy("confirm", arguments);
  },
  confirmCheck() {
    return this.callProxy("confirmCheck", arguments);
  },
  confirmCheckBC() {
    return this.callProxy("confirmCheck", arguments);
  },
  confirmEx() {
    return this.callProxy("confirmEx", arguments);
  },
  confirmExBC() {
    return this.callProxy("confirmEx", arguments);
  },
  prompt() {
    return this.callProxy("prompt", arguments);
  },
  promptBC() {
    return this.callProxy("prompt", arguments);
  },
  promptUsernameAndPassword() {
    return this.callProxy("promptUsernameAndPassword", arguments);
  },
  promptUsernameAndPasswordBC() {
    return this.callProxy("promptUsernameAndPassword", arguments);
  },
  promptPassword() {
    return this.callProxy("promptPassword", arguments);
  },
  promptPasswordBC() {
    return this.callProxy("promptPassword", arguments);
  },
  select() {
    return this.callProxy("select", arguments);
  },
  selectBC() {
    return this.callProxy("select", arguments);
  },

  promptAuth() {
    return this.callProxy("promptAuth", arguments);
  },
  promptAuthBC() {
    return this.callProxy("promptAuth", arguments);
  },
  asyncPromptAuth() {
    return this.callProxy("asyncPromptAuth", arguments);
  },
  asyncPromptAuthBC() {
    return this.callProxy("asyncPromptAuth", arguments);
  },
};

function PromptDelegate(aParent) {
  if (aParent) {
    if (aParent instanceof Window) {
      this._domWin = aParent;
    } else if (aParent.window) {
      this._domWin = aParent.window;
    } else {
      this._domWin =
        aParent.embedderElement && aParent.embedderElement.ownerGlobal;
    }
  }

  if (this._domWin) {
    this._dispatcher = GeckoViewUtils.getDispatcherForWindow(this._domWin);
  }

  if (!this._dispatcher) {
    [
      this._dispatcher,
      this._domWin,
    ] = GeckoViewUtils.getActiveDispatcherAndWindow();
  }
}

PromptDelegate.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),

  BUTTON_TYPE_POSITIVE: 0,
  BUTTON_TYPE_NEUTRAL: 1,
  BUTTON_TYPE_NEGATIVE: 2,

  /* ---------- internal methods ---------- */

  _changeModalState(aEntering) {
    if (!this._domWin) {
      // Allow not having a DOM window.
      return true;
    }
    // Accessing the document object can throw if this window no longer exists. See bug 789888.
    try {
      const winUtils = this._domWin.windowUtils;
      if (!aEntering) {
        winUtils.leaveModalState();
      }

      const event = this._domWin.document.createEvent("Events");
      event.initEvent(
        aEntering ? "DOMWillOpenModalDialog" : "DOMModalDialogClosed",
        true,
        true
      );
      winUtils.dispatchEventToChromeOnly(this._domWin, event);

      if (aEntering) {
        winUtils.enterModalState();
      }
      return true;
    } catch (ex) {
      Cu.reportError("Failed to change modal state: " + ex);
    }
    return false;
  },

  /**
   * Shows a native prompt, and then spins the event loop for this thread while we wait
   * for a response
   */
  _showPrompt(aMsg) {
    let result = undefined;
    if (!this._domWin || !this._changeModalState(/* aEntering */ true)) {
      return result;
    }
    try {
      this.asyncShowPrompt(aMsg, res => (result = res));

      // Spin this thread while we wait for a result
      Services.tm.spinEventLoopUntil(
        () => this._domWin.closed || result !== undefined
      );
    } finally {
      this._changeModalState(/* aEntering */ false);
    }
    return result;
  },

  asyncShowPrompt(aMsg, aCallback) {
    let handled = false;
    const onResponse = response => {
      if (handled) {
        return;
      }
      aCallback(response);
      // This callback object is tied to the Java garbage collector because
      // it is invoked from Java. Manually release the target callback
      // here; otherwise we may hold onto resources for too long, because
      // we would be relying on both the Java and the JS garbage collectors
      // to run.
      aMsg = undefined;
      aCallback = undefined;
      handled = true;
    };

    if (!this._dispatcher) {
      onResponse(null);
      return;
    }

    this._dispatcher.dispatch("GeckoView:Prompt", aMsg, {
      onSuccess: onResponse,
      onError: error => {
        Cu.reportError("Prompt error: " + error);
        onResponse(null);
      },
    });
  },

  _addText(aTitle, aText, aMsg) {
    return Object.assign(aMsg, {
      title: aTitle,
      msg: aText,
    });
  },

  _addCheck(aCheckMsg, aCheckState, aMsg) {
    return Object.assign(aMsg, {
      hasCheck: !!aCheckMsg,
      checkMsg: aCheckMsg,
      checkValue: aCheckState && aCheckState.value,
    });
  },

  /* ----------  nsIPrompt  ---------- */

  alert(aTitle, aText) {
    this.alertCheck(aTitle, aText);
  },

  alertCheck(aTitle, aText, aCheckMsg, aCheckState) {
    const result = this._showPrompt(
      this._addText(
        aTitle,
        aText,
        this._addCheck(aCheckMsg, aCheckState, {
          type: "alert",
        })
      )
    );
    if (result && aCheckState) {
      aCheckState.value = !!result.checkValue;
    }
  },

  confirm(aTitle, aText) {
    // Button 0 is OK.
    return this.confirmCheck(aTitle, aText);
  },

  confirmCheck(aTitle, aText, aCheckMsg, aCheckState) {
    // Button 0 is OK.
    return (
      this.confirmEx(
        aTitle,
        aText,
        Ci.nsIPrompt.STD_OK_CANCEL_BUTTONS,
        /* aButton0 */ null,
        /* aButton1 */ null,
        /* aButton2 */ null,
        aCheckMsg,
        aCheckState
      ) == 0
    );
  },

  confirmEx(
    aTitle,
    aText,
    aButtonFlags,
    aButton0,
    aButton1,
    aButton2,
    aCheckMsg,
    aCheckState
  ) {
    const btnMap = Array(3).fill(null);
    const btnTitle = Array(3).fill(null);
    const btnCustomTitle = Array(3).fill(null);
    const savedButtonId = [];
    for (let i = 0; i < 3; i++) {
      const btnFlags = aButtonFlags >> (i * 8);
      switch (btnFlags & 0xff) {
        case Ci.nsIPrompt.BUTTON_TITLE_OK:
          btnMap[this.BUTTON_TYPE_POSITIVE] = i;
          btnTitle[this.BUTTON_TYPE_POSITIVE] = "ok";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_CANCEL:
          btnMap[this.BUTTON_TYPE_NEGATIVE] = i;
          btnTitle[this.BUTTON_TYPE_NEGATIVE] = "cancel";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_YES:
          btnMap[this.BUTTON_TYPE_POSITIVE] = i;
          btnTitle[this.BUTTON_TYPE_POSITIVE] = "yes";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_NO:
          btnMap[this.BUTTON_TYPE_NEGATIVE] = i;
          btnTitle[this.BUTTON_TYPE_NEGATIVE] = "no";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_IS_STRING:
          // We don't know if this is positive/negative/neutral, so save for later.
          savedButtonId.push(i);
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_SAVE:
        case Ci.nsIPrompt.BUTTON_TITLE_DONT_SAVE:
        case Ci.nsIPrompt.BUTTON_TITLE_REVERT:
        // Not supported; fall-through.
        default:
          break;
      }
    }

    // Put saved buttons into available slots.
    for (let i = 0; i < 3 && savedButtonId.length; i++) {
      if (btnMap[i] === null) {
        btnMap[i] = savedButtonId.shift();
        btnTitle[i] = "custom";
        btnCustomTitle[i] = [aButton0, aButton1, aButton2][btnMap[i]];
      }
    }

    const result = this._showPrompt(
      this._addText(
        aTitle,
        aText,
        this._addCheck(aCheckMsg, aCheckState, {
          type: "button",
          btnTitle,
          btnCustomTitle,
        })
      )
    );
    if (result && aCheckState) {
      aCheckState.value = !!result.checkValue;
    }
    return result && result.button in btnMap ? btnMap[result.button] : -1;
  },

  prompt(aTitle, aText, aValue, aCheckMsg, aCheckState) {
    const result = this._showPrompt(
      this._addText(
        aTitle,
        aText,
        this._addCheck(aCheckMsg, aCheckState, {
          type: "text",
          value: aValue.value,
        })
      )
    );
    // OK: result && result.text !== undefined
    // Cancel: result && result.text === undefined
    // Error: !result
    if (result && aCheckState) {
      aCheckState.value = !!result.checkValue;
    }
    if (!result || result.text === undefined) {
      return false;
    }
    aValue.value = result.text || "";
    return true;
  },

  promptPassword(aTitle, aText, aPassword, aCheckMsg, aCheckState) {
    return this._promptUsernameAndPassword(
      aTitle,
      aText,
      /* aUsername */ undefined,
      aPassword,
      aCheckMsg,
      aCheckState
    );
  },

  promptUsernameAndPassword(
    aTitle,
    aText,
    aUsername,
    aPassword,
    aCheckMsg,
    aCheckState
  ) {
    const msg = {
      type: "auth",
      mode: aUsername ? "auth" : "password",
      options: {
        flags: aUsername ? 0 : Ci.nsIAuthInformation.ONLY_PASSWORD,
        username: aUsername ? aUsername.value : undefined,
        password: aPassword.value,
      },
    };
    const result = this._showPrompt(
      this._addText(aTitle, aText, this._addCheck(aCheckMsg, aCheckState, msg))
    );
    // OK: result && result.password !== undefined
    // Cancel: result && result.password === undefined
    // Error: !result
    if (result && aCheckState) {
      aCheckState.value = !!result.checkValue;
    }
    if (!result || result.password === undefined) {
      return false;
    }
    if (aUsername) {
      aUsername.value = result.username || "";
    }
    aPassword.value = result.password || "";
    return true;
  },

  select(aTitle, aText, aSelectList, aOutSelection) {
    const choices = Array.prototype.map.call(aSelectList, (item, index) => ({
      id: String(index),
      label: item,
      disabled: false,
      selected: false,
    }));
    const result = this._showPrompt(
      this._addText(aTitle, aText, {
        type: "choice",
        mode: "single",
        choices,
      })
    );
    // OK: result
    // Cancel: !result
    if (!result || result.choices === undefined) {
      return false;
    }
    aOutSelection.value = Number(result.choices[0]);
    return true;
  },

  _getAuthMsg(aChannel, aLevel, aAuthInfo) {
    let username;
    if (
      aAuthInfo.flags & Ci.nsIAuthInformation.NEED_DOMAIN &&
      aAuthInfo.domain
    ) {
      username = aAuthInfo.domain + "\\" + aAuthInfo.username;
    } else {
      username = aAuthInfo.username;
    }
    return this._addText(
      /* title */ null,
      this._getAuthText(aChannel, aAuthInfo),
      {
        type: "auth",
        mode:
          aAuthInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD
            ? "password"
            : "auth",
        options: {
          flags: aAuthInfo.flags,
          uri: aChannel && aChannel.URI.displaySpec,
          level: aLevel,
          username,
          password: aAuthInfo.password,
        },
      }
    );
  },

  _fillAuthInfo(aAuthInfo, aCheckState, aResult) {
    if (aResult && aCheckState) {
      aCheckState.value = !!aResult.checkValue;
    }
    if (!aResult || aResult.password === undefined) {
      return false;
    }

    aAuthInfo.password = aResult.password || "";
    if (aAuthInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD) {
      return true;
    }

    const username = aResult.username || "";
    if (aAuthInfo.flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
      // Domain is separated from username by a backslash
      var idx = username.indexOf("\\");
      if (idx >= 0) {
        aAuthInfo.domain = username.substring(0, idx);
        aAuthInfo.username = username.substring(idx + 1);
        return true;
      }
    }
    aAuthInfo.username = username;
    return true;
  },

  promptAuth(aChannel, aLevel, aAuthInfo, aCheckMsg, aCheckState) {
    const result = this._showPrompt(
      this._addCheck(
        aCheckMsg,
        aCheckState,
        this._getAuthMsg(aChannel, aLevel, aAuthInfo)
      )
    );
    // OK: result && result.password !== undefined
    // Cancel: result && result.password === undefined
    // Error: !result
    return this._fillAuthInfo(aAuthInfo, aCheckState, result);
  },

  asyncPromptAuth(
    aChannel,
    aCallback,
    aContext,
    aLevel,
    aAuthInfo,
    aCheckMsg,
    aCheckState
  ) {
    let responded = false;
    const callback = result => {
      // OK: result && result.password !== undefined
      // Cancel: result && result.password === undefined
      // Error: !result
      if (responded) {
        return;
      }
      responded = true;
      if (this._fillAuthInfo(aAuthInfo, aCheckState, result)) {
        aCallback.onAuthAvailable(aContext, aAuthInfo);
      } else {
        aCallback.onAuthCancelled(aContext, /* userCancel */ true);
      }
    };
    this.asyncShowPrompt(
      this._addCheck(
        aCheckMsg,
        aCheckState,
        this._getAuthMsg(aChannel, aLevel, aAuthInfo)
      ),
      callback
    );
    return {
      QueryInterface: ChromeUtils.generateQI([Ci.nsICancelable]),
      cancel() {
        if (responded) {
          return;
        }
        responded = true;
        aCallback.onAuthCancelled(aContext, /* userCancel */ false);
      },
    };
  },

  _getAuthText(aChannel, aAuthInfo) {
    const isProxy = aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY;
    const isPassOnly = aAuthInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD;
    const isCrossOrig =
      aAuthInfo.flags & Ci.nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE;

    const username = aAuthInfo.username;
    const authTarget = this._getAuthTarget(aChannel, aAuthInfo);
    const { displayHost } = authTarget;
    let { realm } = authTarget;

    // Suppress "the site says: $realm" when we synthesized a missing realm.
    if (!aAuthInfo.realm && !isProxy) {
      realm = "";
    }

    // Trim obnoxiously long realms.
    if (realm.length > 50) {
      realm = realm.substring(0, 50) + "\u2026";
    }

    const bundle = Services.strings.createBundle(
      "chrome://global/locale/commonDialogs.properties"
    );
    let text;
    if (isProxy) {
      text = bundle.formatStringFromName("EnterLoginForProxy3", [
        realm,
        displayHost,
      ]);
    } else if (isPassOnly) {
      text = bundle.formatStringFromName("EnterPasswordFor", [
        username,
        displayHost,
      ]);
    } else if (isCrossOrig) {
      text = bundle.formatStringFromName("EnterUserPasswordForCrossOrigin2", [
        displayHost,
      ]);
    } else if (!realm) {
      text = bundle.formatStringFromName("EnterUserPasswordFor2", [
        displayHost,
      ]);
    } else {
      text = bundle.formatStringFromName("EnterLoginForRealm3", [
        realm,
        displayHost,
      ]);
    }

    return text;
  },

  _getAuthTarget(aChannel, aAuthInfo) {
    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
      if (!(aChannel instanceof Ci.nsIProxiedChannel)) {
        throw new Error("proxy auth needs nsIProxiedChannel");
      }
      const info = aChannel.proxyInfo;
      if (!info) {
        throw new Error("proxy auth needs nsIProxyInfo");
      }
      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      const idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
        Ci.nsIIDNService
      );
      const displayHost =
        "moz-proxy://" +
        idnService.convertUTF8toACE(info.host) +
        ":" +
        info.port;
      let realm = aAuthInfo.realm;
      if (!realm) {
        realm = displayHost;
      }
      return { displayHost, realm };
    }

    const displayHost =
      aChannel.URI.scheme + "://" + aChannel.URI.displayHostPort;
    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    let realm = aAuthInfo.realm;
    if (!realm) {
      realm = displayHost;
    }
    return { displayHost, realm };
  },
};

function FilePickerDelegate() {}

FilePickerDelegate.prototype = {
  classID: Components.ID("{e4565e36-f101-4bf5-950b-4be0887785a9}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIFilePicker]),

  /* ----------  nsIFilePicker  ---------- */
  init(aParent, aTitle, aMode) {
    if (
      aMode === Ci.nsIFilePicker.modeGetFolder ||
      aMode === Ci.nsIFilePicker.modeSave
    ) {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }
    this._prompt = new PromptDelegate(aParent);
    this._msg = {
      type: "file",
      title: aTitle,
      mode: aMode === Ci.nsIFilePicker.modeOpenMultiple ? "multiple" : "single",
    };
    this._mode = aMode;
    this._mimeTypes = [];
    this._capture = 0;
  },

  get mode() {
    return this._mode;
  },

  appendRawFilter(aFilter) {
    this._mimeTypes.push(aFilter);
  },

  show() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  open(aFilePickerShownCallback) {
    this._msg.mimeTypes = this._mimeTypes;
    this._msg.capture = this._capture;
    this._prompt.asyncShowPrompt(this._msg, result => {
      // OK: result
      // Cancel: !result
      if (!result || !result.files || !result.files.length) {
        aFilePickerShownCallback.done(Ci.nsIFilePicker.returnCancel);
      } else {
        this._resolveFiles(result.files, aFilePickerShownCallback);
      }
    });
  },

  async _resolveFiles(aFiles, aCallback) {
    const fileData = [];

    try {
      for (const file of aFiles) {
        const domFile = await this._getDOMFile(file);
        fileData.push({
          file,
          domFile,
        });
      }
    } catch (ex) {
      warn`Error resolving files from file picker: ${ex}`;
      aCallback.done(Ci.nsIFilePicker.returnCancel);
      return;
    }

    this._fileData = fileData;
    aCallback.done(Ci.nsIFilePicker.returnOK);
  },

  get file() {
    if (!this._fileData) {
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }
    const fileData = this._fileData[0];
    if (!fileData) {
      return null;
    }
    return new FileUtils.File(fileData.file);
  },

  get fileURL() {
    return Services.io.newFileURI(this.file);
  },

  *_getEnumerator(aDOMFile) {
    if (!this._fileData) {
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }

    for (const fileData of this._fileData) {
      if (aDOMFile) {
        yield fileData.domFile;
      }
      yield new FileUtils.File(fileData.file);
    }
  },

  get files() {
    return this._getEnumerator(/* aDOMFile */ false);
  },

  _getDOMFile(aPath) {
    if (this._prompt._domWin) {
      return this._prompt._domWin.File.createFromFileName(aPath);
    }
    return File.createFromFileName(aPath);
  },

  get domFileOrDirectory() {
    if (!this._fileData) {
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }
    return this._fileData[0] ? this._fileData[0].domFile : null;
  },

  get domFileOrDirectoryEnumerator() {
    return this._getEnumerator(/* aDOMFile */ true);
  },

  get defaultString() {
    return "";
  },

  set defaultString(aValue) {},

  get defaultExtension() {
    return "";
  },

  set defaultExtension(aValue) {},

  get filterIndex() {
    return 0;
  },

  set filterIndex(aValue) {},

  get displayDirectory() {
    return null;
  },

  set displayDirectory(aValue) {},

  get displaySpecialDirectory() {
    return "";
  },

  set displaySpecialDirectory(aValue) {},

  get addToRecentDocs() {
    return false;
  },

  set addToRecentDocs(aValue) {},

  get okButtonLabel() {
    return "";
  },

  set okButtonLabel(aValue) {},

  get capture() {
    return this._capture;
  },

  set capture(aValue) {
    this._capture = aValue;
  },
};

function ColorPickerDelegate() {}

ColorPickerDelegate.prototype = {
  classID: Components.ID("{aa0dd6fc-73dd-4621-8385-c0b377e02cee}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIColorPicker]),

  init(aParent, aTitle, aInitialColor) {
    this._prompt = new PromptDelegate(aParent);
    this._msg = {
      type: "color",
      title: aTitle,
      value: aInitialColor,
    };
  },

  open(aColorPickerShownCallback) {
    this._prompt.asyncShowPrompt(this._msg, result => {
      // OK: result
      // Cancel: !result
      aColorPickerShownCallback.done((result && result.color) || "");
    });
  },
};

function ShareDelegate() {}

ShareDelegate.prototype = {
  classID: Components.ID("{1201d357-8417-4926-a694-e6408fbedcf8}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsISharePicker]),

  init(aParent) {
    this._openerWindow = aParent;
  },

  get openerWindow() {
    return this._openerWindow;
  },

  async share(aTitle, aText, aUri) {
    const ABORT = 2;
    const FAILURE = 1;
    const SUCCESS = 0;

    const msg = {
      type: "share",
      title: aTitle,
      text: aText,
      uri: aUri ? aUri.displaySpec : null,
    };
    const prompt = new PromptDelegate(this._openerWindow);
    const result = await new Promise(resolve => {
      prompt.asyncShowPrompt(msg, resolve);
    });

    if (!result) {
      // A null result is treated as a dismissal in PromptDelegate.
      throw new DOMException(
        domBundle.GetStringFromName("WebShareAPI_Aborted"),
        "AbortError"
      );
    }

    const res = result && result.response;
    switch (res) {
      case FAILURE:
        throw new DOMException(
          domBundle.GetStringFromName("WebShareAPI_Failed"),
          "DataError"
        );
      case ABORT: // Handle aborted attempt and invalid responses the same.
        throw new DOMException(
          domBundle.GetStringFromName("WebShareAPI_Aborted"),
          "AbortError"
        );
      case SUCCESS:
        return;
      default:
        throw new DOMException("Unknown error.", "UnknownError");
    }
  },
};

// Sync with  LoginStoragePrompt.Type in GeckoSession.java.
const LoginStorageType = { SAVE: 1 };
// Sync with  LoginStoragePrompt.Hint in GeckoSession.java.
const LoginStorageHint = { NONE: 0 };

class LoginStorageDelegate {
  get classID() {
    return Components.ID("{3d765750-1c3d-11ea-aaef-0800200c9a66}");
  }

  get QueryInterface() {
    return ChromeUtils.generateQI([Ci.nsILoginManagerPrompter]);
  }

  _createMessage(aType, aHint, aLogins) {
    return {
      // Sync with GeckoSession.handlePromptEvent.
      type: "loginStorage",
      lsType: aType,
      hint: aHint,
      logins: aLogins,
    };
  }

  promptToSavePassword(
    aBrowser,
    aLogin,
    dismissed = false,
    notifySaved = false
  ) {
    const prompt = new PromptDelegate(aBrowser.ownerGlobal);
    prompt.asyncShowPrompt(
      this._createMessage(LoginStorageType.SAVE, LoginStorageHint.NONE, [
        LoginEntry.fromLoginInfo(aLogin),
      ]),
      result => {
        if (!result || result.login === undefined) {
          return;
        }

        const loginInfo = LoginEntry.fromBundle(result.login).toLoginInfo();
        Services.obs.notifyObservers(loginInfo, "passwordmgr-prompt-save");

        GeckoViewLoginStorage.onLoginSave(result.login);
      }
    );
  }

  promptToChangePassword(
    aBrowser,
    aOldLogin,
    aNewLogin,
    dismissed = false,
    notifySaved = false,
    autoSavedLoginGuid = ""
  ) {
    const newLogin = LoginEntry.fromLoginInfo(aOldLogin || aNewLogin);
    const oldGuid = (aOldLogin && newLogin.guid) || null;
    newLogin.origin = aNewLogin.origin;
    newLogin.formActionOrigin = aNewLogin.formActionOrigin;
    newLogin.password = aNewLogin.password;
    newLogin.username = aNewLogin.username;

    const prompt = new PromptDelegate(aBrowser.ownerGlobal);
    prompt.asyncShowPrompt(
      this._createMessage(LoginStorageType.SAVE, LoginStorageHint.NONE, [
        newLogin,
      ]),
      result => {
        if (!result || result.login === undefined) {
          return;
        }

        GeckoViewLoginStorage.onLoginSave(result.login);

        const loginInfo = LoginEntry.fromBundle(result.login).toLoginInfo();
        Services.obs.notifyObservers(
          loginInfo,
          "passwordmgr-prompt-change",
          oldGuid
        );
      }
    );
  }

  promptToChangePasswordWithUsernames(aBrowser, aLogins, aNewLogin) {
    this.promptToChangePassword(aBrowser, null /* oldLogin */, aNewLogin);
  }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  ColorPickerDelegate,
  FilePickerDelegate,
  PromptFactory,
  ShareDelegate,
  LoginStorageDelegate,
]);
