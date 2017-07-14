/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AsyncPrefs",
                                  "resource://gre/modules/AsyncPrefs.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ContentPrefServiceParent",
                                  "resource://gre/modules/ContentPrefServiceParent.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
                                  "resource://gre/modules/Messaging.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "UUIDGen",
                                   "@mozilla.org/uuid-generator;1", "nsIUUIDGenerator");

function PromptFactory() {
}

PromptFactory.prototype = {
  classID: Components.ID("{076ac188-23c1-4390-aa08-7ef1f78ca5d9}"),

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIObserver, Ci.nsIPromptFactory, Ci.nsIPromptService, Ci.nsIPromptService2]),

  /* ----------  nsIObserver  ---------- */
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "app-startup": {
        Services.obs.addObserver(this, "chrome-document-global-created");
        Services.obs.addObserver(this, "content-document-global-created");
        break;
      }
      case "profile-after-change": {
        // ContentPrefServiceParent is needed for e10s file picker.
        ContentPrefServiceParent.init();
        // AsyncPrefs is needed for reader mode.
        AsyncPrefs.init();
        Services.mm.addMessageListener("GeckoView:Prompt", this);
        break;
      }
      case "chrome-document-global-created":
      case "content-document-global-created": {
        let win = aSubject.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDocShell).QueryInterface(Ci.nsIDocShellTreeItem)
                          .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindow);
        if (win !== aSubject) {
          // Only attach to top-level windows.
          return;
        }
        win.addEventListener("click", this); // non-capture
        win.addEventListener("contextmenu", this); // non-capture
        break;
      }
    }
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "click":
        this._handleClick(aEvent);
        break;
      case "contextmenu":
        this._handleContextMenu(aEvent);
        break;
    }
  },

  _handleClick: function(aEvent) {
    let target = aEvent.target;
    if (aEvent.defaultPrevented || target.isContentEditable ||
        target.disabled || target.readOnly || !target.willValidate) {
      // target.willValidate is false when any associated fieldset is disabled,
      // in which case this element is treated as disabled per spec.
      return;
    }

    let win = target.ownerGlobal;
    if (target instanceof win.HTMLSelectElement) {
      this._handleSelect(target);
      aEvent.preventDefault();
    } else if (target instanceof win.HTMLInputElement) {
      let type = target.type;
      if (type === "date" || type === "month" || type === "week" ||
          type === "time" || type === "datetime-local") {
        this._handleDateTime(target, type);
        aEvent.preventDefault();
      }
    }
  },

  _handleSelect: function(aElement) {
    let win = aElement.ownerGlobal;
    let id = 0;
    let map = {};

    let items = (function enumList(elem, disabled) {
      let items = [];
      let children = elem.children;
      for (let i = 0; i < children.length; i++) {
        let child = children[i];
        if (win.getComputedStyle(child).display === "none") {
          continue;
        }
        let item = {
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

    let prompt = new PromptDelegate(win);
    prompt.asyncShowPrompt({
      type: "choice",
      mode: aElement.multiple ? "multiple" : "single",
      choices: items,
    }, result => {
      // OK: result
      // Cancel: !result
      if (!result || result.choices === undefined) {
        return;
      }

      let dispatchEvents = false;
      if (!aElement.multiple) {
        let elem = map[result.choices[0]];
        if (elem && elem instanceof win.HTMLOptionElement) {
          dispatchEvents = !elem.selected;
          elem.selected = true;
        } else {
          Cu.reportError("Invalid id for select result: " + result.choices[0]);
        }
      } else {
        for (let i = 0; i < id; i++) {
          let elem = map[i];
          let index = result.choices.indexOf(String(i));
          if (elem instanceof win.HTMLOptionElement &&
              elem.selected !== (index >= 0)) {
            // Current selected is not the same as the new selected state.
            dispatchEvents = true;
            elem.selected = !elem.selected;
          }
          result.choices[index] = undefined;
        }
        for (let i = 0; i < result.choices.length; i++) {
          if (result.choices[i] !== undefined && result.choices[i] !== null) {
            Cu.reportError("Invalid id for select result: " + result.choices[i]);
            break;
          }
        }
      }

      if (dispatchEvents) {
        this._dispatchEvents(aElement);
      }
    });
  },

  _handleDateTime: function(aElement, aType) {
    let prompt = new PromptDelegate(aElement.ownerGlobal);
    prompt.asyncShowPrompt({
      type: "datetime",
      mode: aType,
      value: aElement.value,
      min: aElement.min,
      max: aElement.max,
    }, result => {
      // OK: result
      // Cancel: !result
      if (!result || result.datetime === undefined || result.datetime === aElement.value) {
        return;
      }
      aElement.value = result.datetime;
      this._dispatchEvents(aElement);
    });
  },

  _dispatchEvents: function(aElement) {
    // Fire both "input" and "change" events for <select> and <input>.
    aElement.dispatchEvent(new aElement.ownerGlobal.Event("input", { bubbles: true }));
    aElement.dispatchEvent(new aElement.ownerGlobal.Event("change", { bubbles: true }));
  },

  _handleContextMenu: function(aEvent) {
    let target = aEvent.target;
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

    let builder = {
      _cursor: undefined,
      _id: 0,
      _map: {},
      _stack: [],
      items: [],

      // nsIMenuBuilder
      openContainer: function(aLabel) {
        if (!this._cursor) {
          // Top-level
          this._cursor = this;
          return;
        }
        let newCursor = {
          id: String(this._id++),
          items: [],
          label: aLabel,
        };
        this._cursor.items.push(newCursor);
        this._stack.push(this._cursor);
        this._cursor = newCursor;
      },

      addItemFor: function(aElement, aCanLoadIcon) {
        this._cursor.items.push({
          disabled: aElement.disabled,
          icon: aCanLoadIcon && aElement.icon &&
                aElement.icon.length ? aElement.icon : null,
          id: String(this._id),
          label: aElement.label,
          selected: aElement.checked,
        });
        this._map[this._id++] = aElement;
      },

      addSeparator: function() {
        this._cursor.items.push({
          disabled: true,
          id: String(this._id++),
          separator: true,
        });
      },

      undoAddSeparator: function() {
        let sep = this._cursor.items[this._cursor.items.length - 1];
        if (sep && sep.separator) {
          this._cursor.items.pop();
        }
      },

      closeContainer: function() {
        let childItems = (this._cursor.label === "") ? this._cursor.items : null;
        this._cursor = this._stack.pop();

        if (childItems !== null && this._cursor && this._cursor.items.length === 1) {
          // Merge a single nameless child container into the parent container.
          // This lets us build an HTML contextmenu within a submenu.
          this._cursor.items = childItems;
        }
      },

      toJSONString: function() {
        return JSON.stringify(this.items);
      },

      click: function(aId) {
        let item = this._map[aId];
        if (item) {
          item.click();
        }
      },
    };

    // XXX the "show" event is not cancelable but spec says it should be.
    menu.sendShowEvent();
    menu.build(builder);

    let prompt = new PromptDelegate(target.ownerGlobal);
    prompt.asyncShowPrompt({
      type: "choice",
      mode: "menu",
      choices: builder.items,
    }, result => {
      // OK: result
      // Cancel: !result
      if (result && result.choices !== undefined) {
        builder.click(result.choices[0]);
      }
    });

    aEvent.preventDefault();
  },

  receiveMessage: function(aMsg) {
    if (aMsg.name !== "GeckoView:Prompt") {
      return;
    }

    let prompt = new PromptDelegate(aMsg.target.contentWindow || aMsg.target.ownerGlobal);
    prompt.asyncShowPrompt(aMsg.data, result => {
      aMsg.target.messageManager.sendAsyncMessage("GeckoView:PromptClose", {
        uuid: aMsg.data.uuid,
        result: result,
      });
    });
  },

  /* ----------  nsIPromptFactory  ---------- */
  getPrompt: function(aDOMWin, aIID) {
    // Delegated to login manager here, which in turn calls back into us via nsIPromptService2.
    if (aIID.equals(Ci.nsIAuthPrompt2) || aIID.equals(Ci.nsIAuthPrompt)) {
      try {
        let pwmgr = Cc["@mozilla.org/passwordmanager/authpromptfactory;1"].getService(Ci.nsIPromptFactory);
        return pwmgr.getPrompt(aDOMWin, aIID);
      } catch (e) {
        Cu.reportError("Delegation to password manager failed: " + e);
      }
    }

    let p = new PromptDelegate(aDOMWin);
    p.QueryInterface(aIID);
    return p;
  },

  /* ----------  private memebers  ---------- */

  // nsIPromptService and nsIPromptService2 methods proxy to our Prompt class
  callProxy: function(aMethod, aArguments) {
    let prompt = new PromptDelegate(aArguments[0]);
    return prompt[aMethod].apply(prompt, Array.prototype.slice.call(aArguments, 1));
  },

  /* ----------  nsIPromptService  ---------- */

  alert: function() {
    return this.callProxy("alert", arguments);
  },
  alertCheck: function() {
    return this.callProxy("alertCheck", arguments);
  },
  confirm: function() {
    return this.callProxy("confirm", arguments);
  },
  confirmCheck: function() {
    return this.callProxy("confirmCheck", arguments);
  },
  confirmEx: function() {
    return this.callProxy("confirmEx", arguments);
  },
  prompt: function() {
    return this.callProxy("prompt", arguments);
  },
  promptUsernameAndPassword: function() {
    return this.callProxy("promptUsernameAndPassword", arguments);
  },
  promptPassword: function() {
    return this.callProxy("promptPassword", arguments);
  },
  select: function() {
    return this.callProxy("select", arguments);
  },

  /* ----------  nsIPromptService2  ---------- */
  promptAuth: function() {
    return this.callProxy("promptAuth", arguments);
  },
  asyncPromptAuth: function() {
    return this.callProxy("asyncPromptAuth", arguments);
  }
};

function PromptDelegate(aDomWin) {
  this._domWin = aDomWin;

  if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
    return;
  }

  this._dispatcher = EventDispatcher.instance;

  if (!aDomWin) {
    return;
  }
  let gvWin = aDomWin.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDocShell).QueryInterface(Ci.nsIDocShellTreeItem)
                     .rootTreeItem.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindow);
  try {
    this._dispatcher = EventDispatcher.for(gvWin);
  } catch (e) {
    // Use global dispatcher.
  }
}

PromptDelegate.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPrompt]),

  BUTTON_TYPE_POSITIVE: 0,
  BUTTON_TYPE_NEUTRAL: 1,
  BUTTON_TYPE_NEGATIVE: 2,

  /* ---------- internal methods ---------- */

  _changeModalState: function(aEntering) {
    if (!this._domWin) {
      // Allow not having a DOM window.
      return true;
    }
    // Accessing the document object can throw if this window no longer exists. See bug 789888.
    try {
      let winUtils = this._domWin.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindowUtils);
      if (!aEntering) {
        winUtils.leaveModalState();
      }

      let event = this._domWin.document.createEvent("Events");
      event.initEvent(aEntering ? "DOMWillOpenModalDialog" : "DOMModalDialogClosed",
                      true, true);
      winUtils.dispatchEventToChromeOnly(this._domWin, event);

      if (aEntering) {
        winUtils.enterModalState();
      }
      return true;

    } catch(ex) {
      Cu.reportError("Failed to change modal state: " + e);
    }
    return false;
  },

  /**
   * Shows a native prompt, and then spins the event loop for this thread while we wait
   * for a response
   */
  _showPrompt: function(aMsg) {
    let result = undefined;
    if (!this._changeModalState(/* aEntering */ true)) {
      return;
    }
    try {
      this.asyncShowPrompt(aMsg, res => result = res);

      // Spin this thread while we wait for a result
      Services.tm.spinEventLoopUntil(() => result !== undefined);
    } finally {
      this._changeModalState(/* aEntering */ false);
    }
    return result;
  },

  asyncShowPrompt: function(aMsg, aCallback) {
    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
      let docShell = this._domWin.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDocShell)
                                 .QueryInterface(Ci.nsIDocShellTreeItem)
                                 .rootTreeItem;
      let messageManager = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsITabChild)
                                   .messageManager;

      let uuid = UUIDGen.generateUUID().toString();
      aMsg.uuid = uuid;

      messageManager.addMessageListener("GeckoView:PromptClose", function listener(msg) {
        if (msg.data.uuid !== uuid) {
          return;
        }
        messageManager.removeMessageListener(msg.name, listener);
        aCallback(msg.data.result);
      });
      messageManager.sendAsyncMessage("GeckoView:Prompt", aMsg);
      return;
    }

    let handled = false;
    this._dispatcher.dispatch("GeckoView:Prompt", aMsg, {
      onSuccess: response => {
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
      },
      onError: error => {
        Cu.reportError("Prompt error: " + error);
        this.onSuccess(null);
      },
    });
  },

  _addText: function(aTitle, aText, aMsg) {
    return Object.assign(aMsg, {
      title: aTitle,
      msg: aText,
    });
  },

  _addCheck: function(aCheckMsg, aCheckState, aMsg) {
    return Object.assign(aMsg, {
      hasCheck: !!aCheckMsg,
      checkMsg: aCheckMsg,
      checkValue: aCheckState && aCheckState.value,
    });
  },

  /* ----------  nsIPrompt  ---------- */

  alert: function(aTitle, aText) {
    this.alertCheck(aTitle, aText);
  },

  alertCheck: function(aTitle, aText, aCheckMsg, aCheckState) {
    let result = this._showPrompt(this._addText(aTitle, aText,
                                  this._addCheck(aCheckMsg, aCheckState, {
                                    type: "alert",
                                  })));
    if (result && aCheckState) {
      aCheckState.value = !!result.checkValue;
    }
  },

  confirm: function(aTitle, aText) {
    // Button 0 is OK.
    return this.confirmCheck(aTitle, aText) == 0;
  },

  confirmCheck: function(aTitle, aText, aCheckMsg, aCheckState) {
    // Button 0 is OK.
    return this.confirmEx(aTitle, aText, Ci.nsIPrompt.STD_OK_CANCEL_BUTTONS,
                          /* aButton0 */ null, /* aButton1 */ null, /* aButton2 */ null,
                          aCheckMsg, aCheckState) == 0;
  },

  confirmEx: function(aTitle, aText, aButtonFlags, aButton0,
                      aButton1, aButton2, aCheckMsg, aCheckState) {
    let btnMap = Array(3).fill(null);
    let btnTitle = Array(3).fill(null);
    let btnCustomTitle = Array(3).fill(null);
    let savedButtonId = [];
    for (let i = 0; i < 3; i++) {
      let btnFlags = aButtonFlags >> (i * 8);
      switch (btnFlags & 0xff) {
        case Ci.nsIPrompt.BUTTON_TITLE_OK :
          btnMap[this.BUTTON_TYPE_POSITIVE] = i;
          btnTitle[this.BUTTON_TYPE_POSITIVE] = "ok";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_CANCEL :
          btnMap[this.BUTTON_TYPE_NEGATIVE] = i;
          btnTitle[this.BUTTON_TYPE_NEGATIVE] = "cancel";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_YES :
          btnMap[this.BUTTON_TYPE_POSITIVE] = i;
          btnTitle[this.BUTTON_TYPE_POSITIVE] = "yes";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_NO :
          btnMap[this.BUTTON_TYPE_NEGATIVE] = i;
          btnTitle[this.BUTTON_TYPE_NEGATIVE] = "no";
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_IS_STRING :
          // We don't know if this is positive/negative/neutral, so save for later.
          savedButtonId.push(i);
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_SAVE :
        case Ci.nsIPrompt.BUTTON_TITLE_DONT_SAVE :
        case Ci.nsIPrompt.BUTTON_TITLE_REVERT :
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

    let result = this._showPrompt(this._addText(aTitle, aText,
                                  this._addCheck(aCheckMsg, aCheckState, {
                                    type: "button",
                                    btnTitle: btnTitle,
                                    btnCustomTitle: btnCustomTitle,
                                  })));
    if (result && aCheckState) {
      aCheckState.value = !!result.checkValue;
    }
    return (result && result.button in btnMap) ? btnMap[result.button] : -1;
  },

  prompt: function(aTitle, aText, aValue, aCheckMsg, aCheckState) {
    let result = this._showPrompt(this._addText(aTitle, aText,
                                  this._addCheck(aCheckMsg, aCheckState, {
                                    type: "text",
                                    value: aValue.value,
                                  })));
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

  promptPassword: function(aTitle, aText, aPassword, aCheckMsg, aCheckState) {
    return this._promptUsernameAndPassword(aTitle, aText, /* aUsername */ undefined,
                                           aPassword, aCheckMsg, aCheckState);
  },

  promptUsernameAndPassword: function(aTitle, aText, aUsername, aPassword,
                                      aCheckMsg, aCheckState) {
    let msg = {
      type: "auth",
      mode: aUsername ? "auth" : "password",
      options: {
        flags: aUsername ? 0 : Ci.nsIAuthInformation.ONLY_PASSWORD,
        username: aUsername ? aUsername.value : undefined,
        password: aPassword.value,
      },
    };
    let result = this._showPrompt(this._addText(aTitle, aText,
                                  this._addCheck(aCheckMsg, aCheckState, msg)));
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

  select: function(aTitle, aText, aCount, aSelectList, aOutSelection) {
    let choices = Array.prototype.map.call(aSelectList, (item, index) => ({
      id: String(index),
      label: item,
      disabled: false,
      selected: false,
    }));
    let result = this._showPrompt(this._addText(aTitle, aText, {
                                    type: "choice",
                                    mode: "single",
                                    choices: choices,
                                  }));
    // OK: result
    // Cancel: !result
    if (!result || result.choices === undefined) {
      return false;
    }
    aOutSelection.value = Number(result.choices[0]);
    return true;
  },

  _getAuthMsg: function(aChannel, aLevel, aAuthInfo) {
    let username;
    if ((aAuthInfo.flags & Ci.nsIAuthInformation.NEED_DOMAIN) && aAuthInfo.domain) {
      username = aAuthInfo.domain + "\\" + aAuthInfo.username;
    } else {
      username = aAuthInfo.username;
    }
    return this._addText(/* title */ null, this._getAuthText(aChannel, aAuthInfo), {
      type: "auth",
      mode: aAuthInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD ? "password" : "auth",
      options: {
        flags: aAuthInfo.flags,
        uri: aChannel && aChannel.URI.spec,
        level: aLevel,
        username: username,
        password: aAuthInfo.password,
      },
    });
  },

  _fillAuthInfo: function(aAuthInfo, aCheckState, aResult) {
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

    let username = aResult.username || "";
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

  promptAuth: function(aChannel, aLevel, aAuthInfo, aCheckMsg, aCheckState) {
    let result = this._showPrompt(this._addCheck(aCheckMsg, aCheckState,
                                  this._getAuthMsg(aChannel, aLevel, aAuthInfo)));
    // OK: result && result.password !== undefined
    // Cancel: result && result.password === undefined
    // Error: !result
    return this._fillAuthInfo(aAuthInfo, aCheckState, result);
  },

  asyncPromptAuth: function(aChannel, aCallback, aContext, aLevel, aAuthInfo,
                            aCheckLabel, aCheckState) {
    let responded = false;
    let callback = result => {
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
    this.asyncShowPrompt(this._addCheck(aCheckMsg, aCheckState,
                          this._getAuthMsg(aChannel, aLevel, aAuthInfo)), callback);
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsICancelable]),
      cancel: function() {
        if (responded) {
          return;
        }
        responded = true;
        aCallback.onAuthCancelled(aContext, /* userCancel */ false);
      }
    };
  },

  _getAuthText: function(aChannel, aAuthInfo) {
    let isProxy = (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY);
    let isPassOnly = (aAuthInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD);
    let isCrossOrig = (aAuthInfo.flags & Ci.nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE);

    let username = aAuthInfo.username;
    let [displayHost, realm] = this._getAuthTarget(aChannel, aAuthInfo);

    // Suppress "the site says: $realm" when we synthesized a missing realm.
    if (!aAuthInfo.realm && !isProxy) {
      realm = "";
    }

    // Trim obnoxiously long realms.
    if (realm.length > 50) {
      realm = realm.substring(0, 50) + "\u2026";
    }

    let bundle = Services.strings.createBundle(
        "chrome://global/locale/commonDialogs.properties");
    let text;
    if (isProxy) {
      text = bundle.formatStringFromName("EnterLoginForProxy3", [realm, displayHost], 2);
    } else if (isPassOnly) {
      text = bundle.formatStringFromName("EnterPasswordFor", [username, displayHost], 2);
    } else if (isCrossOrig) {
      text = bundle.formatStringFromName("EnterUserPasswordForCrossOrigin2", [displayHost], 1);
    } else if (!realm) {
      text = bundle.formatStringFromName("EnterUserPasswordFor2", [displayHost], 1);
    } else {
      text = bundle.formatStringFromName("EnterLoginForRealm3", [realm, displayHost], 2);
    }

    return text;
  },

  _getAuthTarget: function(aChannel, aAuthInfo) {
    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
      if (!(aChannel instanceof Ci.nsIProxiedChannel)) {
        throw "proxy auth needs nsIProxiedChannel";
      }
      let info = aChannel.proxyInfo;
      if (!info) {
        throw "proxy auth needs nsIProxyInfo";
      }
      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      let idnService = Cc["@mozilla.org/network/idn-service;1"].getService(Ci.nsIIDNService);
      let hostname = "moz-proxy://" + idnService.convertUTF8toACE(info.host) + ":" + info.port;
      let realm = aAuthInfo.realm;
      if (!realm) {
        realm = hostname;
      }
      return [hostname, realm];
    }

    let hostname = aChannel.URI.scheme + "://" + aChannel.URI.hostPort;
    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    let realm = aAuthInfo.realm;
    if (!realm) {
      realm = hostname;
    }
    return [hostname, realm];
  },
};

function FilePickerDelegate() {
}

FilePickerDelegate.prototype = {
  classID: Components.ID("{e4565e36-f101-4bf5-950b-4be0887785a9}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFilePicker]),

  /* ----------  nsIFilePicker  ---------- */
  init: function(aParent, aTitle, aMode) {
    if (aMode === Ci.nsIFilePicker.modeGetFolder ||
        aMode === Ci.nsIFilePicker.modeSave) {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }
    this._prompt = new PromptDelegate(aParent);
    this._msg = {
      type: "file",
      title: aTitle,
      mode: (aMode === Ci.nsIFilePicker.modeOpenMultiple) ? "multiple" : "single",
    };
    this._mode = aMode;
    this._mimeTypes = [];
    this._extensions = [];
  },

  get mode() {
    return this._mode;
  },

  appendFilters: function(aFilterMask) {
    if (aFilterMask & Ci.nsIFilePicker.filterAll) {
      this._mimeTypes.push("*/*");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterAudio) {
      this._mimeTypes.push("audio/*");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterImages) {
      this._mimeTypes.push("image/*");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterVideo) {
      this._mimeTypes.push("video/*");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterHTML) {
      this._mimeTypes.push("text/html");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterText) {
      this._mimeTypes.push("text/plain");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterXML) {
      this._mimeTypes.push("text/xml");
    }
    if (aFilterMask & Ci.nsIFilePicker.filterXUL) {
      this._mimeTypes.push("application/vnd.mozilla.xul+xml");
    }
  },

  appendFilter: function(aTitle, aFilter) {
    // Only include filter that specify extensions (i.e. exclude generic ones like "*").
    let filters = aFilter.split(/[\s,;]+/).filter(filter => filter.indexOf(".") >= 0);
    Array.prototype.push.apply(this._extensions, filters);
  },

  show: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  open: function(aFilePickerShownCallback) {
    this._msg.mimeTypes = this._mimeTypes;
    if (this._extensions.length) {
      this._msg.extensions = this._extensions;
    }
    this._prompt.asyncShowPrompt(this._msg, result => {
      // OK: result
      // Cancel: !result
      if (!result || !result.files || !result.files.length) {
        aFilePickerShownCallback.done(Ci.nsIFilePicker.returnCancel);
      } else {
        this._files = result.files;
        aFilePickerShownCallback.done(Ci.nsIFilePicker.returnOK);
      }
    });
  },

  get file() {
    if (!this._files) {
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }
    return new FileUtils.File(this._files[0]);
  },

  get fileURL() {
    return Services.io.newFileURI(this.file);
  },

  _getEnumerator(aDOMFile) {
    if (!this._files) {
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }
    return {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsISimpleEnumerator]),
      _owner: this,
      _index: 0,
      hasMoreElements: function() {
        return this._index < this._owner._files.length;
      },
      getNext: function() {
        let files = this._owner._files;
        if (this._index >= files.length) {
          throw Cr.NS_ERROR_FAILURE;
        }
        if (aDOMFile) {
          return this._owner._getDOMFile(files[this._index++]);
        }
        return new FileUtils.File(files[this._index++]);
      }
    };
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
    if (!this._files) {
      throw Cr.NS_ERROR_NOT_AVAILABLE;
    }
    return this._getDOMFile(this._files[0]);
  },

  get domFileOrDirectoryEnumerator() {
    return this._getEnumerator(/* aDOMFile */ true);
  },

  get defaultString() {
    return "";
  },

  set defaultString(aValue) {
  },

  get defaultExtension() {
    return "";
  },

  set defaultExtension(aValue) {
  },

  get filterIndex() {
    return 0;
  },

  set filterIndex(aValue) {
  },

  get displayDirectory() {
    return null;
  },

  set displayDirectory(aValue) {
  },

  get displaySpecialDirectory() {
    return "";
  },

  set displaySpecialDirectory(aValue) {
  },

  get addToRecentDocs() {
    return false;
  },

  set addToRecentDocs(aValue) {
  },

  get okButtonLabel() {
    return "";
  },

  set okButtonLabel(aValue) {
  },
};

function ColorPickerDelegate() {
}

ColorPickerDelegate.prototype = {
  classID: Components.ID("{aa0dd6fc-73dd-4621-8385-c0b377e02cee}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIColorPicker]),

  init: function(aParent, aTitle, aInitialColor) {
    this._prompt = new PromptDelegate(aParent);
    this._msg = {
      type: "color",
      title: aTitle,
      value: aInitialColor,
    };
  },

  open: function(aColorPickerShownCallback) {
    this._prompt.asyncShowPrompt(this._msg, result => {
      // OK: result
      // Cancel: !result
      aColorPickerShownCallback.done((result && result.color) || "");
    });
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  ColorPickerDelegate, FilePickerDelegate, PromptFactory]);
