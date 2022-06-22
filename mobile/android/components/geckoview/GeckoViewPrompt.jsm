/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["PromptFactory"];

const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  GeckoViewPrompter: "resource://gre/modules/GeckoViewPrompter.jsm",
});

ChromeUtils.defineModuleGetter(
  lazy,
  "DeferredTask",
  "resource://gre/modules/DeferredTask.jsm"
);

const { debug, warn } = GeckoViewUtils.initLogging("GeckoViewPrompt");

class PromptFactory {
  constructor() {
    this.wrappedJSObject = this;
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mozshowdropdown":
      case "mozshowdropdown-sourcetouch":
        this._handleSelect(aEvent.composedTarget, /* aIsDropDown = */ true);
        break;
      case "MozOpenDateTimePicker":
        this._handleDateTime(aEvent.composedTarget);
        break;
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
  }

  _handleClick(aEvent) {
    const target = aEvent.composedTarget;
    const className = ChromeUtils.getClassName(target);
    if (className !== "HTMLInputElement" && className !== "HTMLSelectElement") {
      return;
    }

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

    if (className === "HTMLSelectElement") {
      if (!target.isCombobox) {
        this._handleSelect(target, /* aIsDropDown = */ false);
        return;
      }
      // combobox select is handled by mozshowdropdown.
      return;
    }

    const type = target.type;
    if (type === "month" || type === "week") {
      // If there's a shadow root, the MozOpenDateTimePicker event takes care
      // of this. Right now for these input types there's never a shadow root.
      // Once we support UA widgets for month/week inputs (see bug 888320), we
      // can remove this.
      if (!target.openOrClosedShadowRoot) {
        this._handleDateTime(target);
        aEvent.preventDefault();
      }
    }
  }

  _generateSelectItems(aElement) {
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
        if (win.HTMLOptGroupElement.isInstance(child)) {
          item.label = child.label;
          item.items = enumList(child, item.disabled);
        } else if (win.HTMLOptionElement.isInstance(child)) {
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

    return [items, map, id];
  }

  _handleSelect(aElement, aIsDropDown) {
    const win = aElement.ownerGlobal;
    const [items] = this._generateSelectItems(aElement);

    if (aIsDropDown) {
      aElement.openInParentProcess = true;
    }

    const prompt = new lazy.GeckoViewPrompter(win);

    // Something changed the <select> while it was open.
    const deferredUpdate = new lazy.DeferredTask(() => {
      // Inner contents in choice prompt are updated.
      const [newItems] = this._generateSelectItems(aElement);
      prompt.update({
        type: "choice",
        mode: aElement.multiple ? "multiple" : "single",
        choices: newItems,
      });
    }, 0);
    const mut = new win.MutationObserver(() => {
      deferredUpdate.arm();
    });
    mut.observe(aElement, {
      childList: true,
      subtree: true,
      attributes: true,
    });

    const dismissPrompt = () => prompt.dismiss();
    aElement.addEventListener("blur", dismissPrompt, { mozSystemGroup: true });
    const hidedropdown = event => {
      if (aElement === event.target) {
        prompt.dismiss();
      }
    };
    const chromeEventHandler = aElement.ownerGlobal.docShell.chromeEventHandler;
    chromeEventHandler.addEventListener("mozhidedropdown", hidedropdown, {
      mozSystemGroup: true,
    });

    prompt.asyncShowPrompt(
      {
        type: "choice",
        mode: aElement.multiple ? "multiple" : "single",
        choices: items,
      },
      result => {
        deferredUpdate.disarm();
        mut.disconnect();
        aElement.removeEventListener("blur", dismissPrompt, {
          mozSystemGroup: true,
        });
        chromeEventHandler.removeEventListener(
          "mozhidedropdown",
          hidedropdown,
          { mozSystemGroup: true }
        );

        if (aIsDropDown) {
          aElement.openInParentProcess = false;
        }
        // OK: result
        // Cancel: !result
        if (!result || result.choices === undefined) {
          return;
        }

        const [, map, id] = this._generateSelectItems(aElement);
        let dispatchEvents = false;
        if (!aElement.multiple) {
          const elem = map[result.choices[0]];
          if (elem && win.HTMLOptionElement.isInstance(elem)) {
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
              win.HTMLOptionElement.isInstance(elem) &&
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
  }

  _handleDateTime(aElement) {
    const prompt = new lazy.GeckoViewPrompter(aElement.ownerGlobal);

    const chromeEventHandler = aElement.ownerGlobal.docShell.chromeEventHandler;
    const dismissPrompt = () => prompt.dismiss();
    // Some controls don't have UA widget (bug 888320)
    if (
      ["month", "week"].includes(aElement.type) &&
      !aElement.openOrClosedShadowRoot
    ) {
      aElement.addEventListener("blur", dismissPrompt, {
        mozSystemGroup: true,
      });
    } else {
      chromeEventHandler.addEventListener(
        "MozCloseDateTimePicker",
        dismissPrompt
      );
    }

    prompt.asyncShowPrompt(
      {
        type: "datetime",
        mode: aElement.type,
        value: aElement.value,
        min: aElement.min,
        max: aElement.max,
        step: aElement.step,
      },
      result => {
        // Some controls don't have UA widget (bug 888320)
        if (
          ["month", "week"].includes(aElement.type) &&
          !aElement.openOrClosedShadowRoot
        ) {
          aElement.removeEventListener("blur", dismissPrompt, {
            mozSystemGroup: true,
          });
        } else {
          chromeEventHandler.removeEventListener(
            "MozCloseDateTimePicker",
            dismissPrompt
          );
        }

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
  }

  _dispatchEvents(aElement) {
    // Fire both "input" and "change" events for <select> and <input> for
    // date/time.
    aElement.dispatchEvent(
      new aElement.ownerGlobal.Event("input", { bubbles: true, composed: true })
    );
    aElement.dispatchEvent(
      new aElement.ownerGlobal.Event("change", { bubbles: true })
    );
  }

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

    const prompt = new lazy.GeckoViewPrompter(target.ownerGlobal);
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
  }

  _handlePopupBlocked(aEvent) {
    const dwi = aEvent.requestingWindow;
    const popupWindowURISpec = aEvent.popupWindowURI
      ? aEvent.popupWindowURI.displaySpec
      : "about:blank";

    const prompt = new lazy.GeckoViewPrompter(aEvent.requestingWindow);
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
  }

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
  }

  /* ----------  private memebers  ---------- */

  // nsIPromptService methods proxy to our Prompt class
  callProxy(aMethod, aArguments) {
    const prompt = new PromptDelegate(aArguments[0]);
    let promptArgs;
    if (BrowsingContext.isInstance(aArguments[0])) {
      // Called by BrowsingContext prompt method, strip modalType.
      [, , /*browsingContext*/ /*modalType*/ ...promptArgs] = aArguments;
    } else {
      [, /*domWindow*/ ...promptArgs] = aArguments;
    }
    return prompt[aMethod].apply(prompt, promptArgs);
  }

  /* ----------  nsIPromptService  ---------- */

  alert() {
    return this.callProxy("alert", arguments);
  }
  alertBC() {
    return this.callProxy("alert", arguments);
  }
  alertCheck() {
    return this.callProxy("alertCheck", arguments);
  }
  alertCheckBC() {
    return this.callProxy("alertCheck", arguments);
  }
  confirm() {
    return this.callProxy("confirm", arguments);
  }
  confirmBC() {
    return this.callProxy("confirm", arguments);
  }
  confirmCheck() {
    return this.callProxy("confirmCheck", arguments);
  }
  confirmCheckBC() {
    return this.callProxy("confirmCheck", arguments);
  }
  confirmEx() {
    return this.callProxy("confirmEx", arguments);
  }
  confirmExBC() {
    return this.callProxy("confirmEx", arguments);
  }
  prompt() {
    return this.callProxy("prompt", arguments);
  }
  promptBC() {
    return this.callProxy("prompt", arguments);
  }
  promptUsernameAndPassword() {
    return this.callProxy("promptUsernameAndPassword", arguments);
  }
  promptUsernameAndPasswordBC() {
    return this.callProxy("promptUsernameAndPassword", arguments);
  }
  promptPassword() {
    return this.callProxy("promptPassword", arguments);
  }
  promptPasswordBC() {
    return this.callProxy("promptPassword", arguments);
  }
  select() {
    return this.callProxy("select", arguments);
  }
  selectBC() {
    return this.callProxy("select", arguments);
  }
  promptAuth() {
    return this.callProxy("promptAuth", arguments);
  }
  promptAuthBC() {
    return this.callProxy("promptAuth", arguments);
  }
  asyncPromptAuth() {
    return this.callProxy("asyncPromptAuth", arguments);
  }
}

PromptFactory.prototype.classID = Components.ID(
  "{076ac188-23c1-4390-aa08-7ef1f78ca5d9}"
);
PromptFactory.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIPromptFactory",
  "nsIPromptService",
]);

class PromptDelegate {
  constructor(aParent) {
    this._prompter = new lazy.GeckoViewPrompter(aParent);
  }

  BUTTON_TYPE_POSITIVE = 0;
  BUTTON_TYPE_NEUTRAL = 1;
  BUTTON_TYPE_NEGATIVE = 2;

  /* ---------- internal methods ---------- */

  _addText(aTitle, aText, aMsg) {
    return Object.assign(aMsg, {
      title: aTitle,
      msg: aText,
    });
  }

  _addCheck(aCheckMsg, aCheckState, aMsg) {
    return Object.assign(aMsg, {
      hasCheck: !!aCheckMsg,
      checkMsg: aCheckMsg,
      checkValue: aCheckState && aCheckState.value,
    });
  }

  /* ----------  nsIPrompt  ---------- */

  alert(aTitle, aText) {
    this.alertCheck(aTitle, aText);
  }

  alertCheck(aTitle, aText, aCheckMsg, aCheckState) {
    const result = this._prompter.showPrompt(
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
  }

  confirm(aTitle, aText) {
    // Button 0 is OK.
    return this.confirmCheck(aTitle, aText);
  }

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
  }

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

    const result = this._prompter.showPrompt(
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
  }

  prompt(aTitle, aText, aValue, aCheckMsg, aCheckState) {
    const result = this._prompter.showPrompt(
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
  }

  promptPassword(aTitle, aText, aPassword) {
    return this._promptUsernameAndPassword(
      aTitle,
      aText,
      /* aUsername */ undefined,
      aPassword
    );
  }

  promptUsernameAndPassword(aTitle, aText, aUsername, aPassword) {
    const msg = {
      type: "auth",
      mode: aUsername ? "auth" : "password",
      options: {
        flags: aUsername ? 0 : Ci.nsIAuthInformation.ONLY_PASSWORD,
        username: aUsername ? aUsername.value : undefined,
        password: aPassword.value,
      },
    };
    const result = this._prompter.showPrompt(this._addText(aTitle, aText, msg));
    // OK: result && result.password !== undefined
    // Cancel: result && result.password === undefined
    // Error: !result
    if (!result || result.password === undefined) {
      return false;
    }
    if (aUsername) {
      aUsername.value = result.username || "";
    }
    aPassword.value = result.password || "";
    return true;
  }

  select(aTitle, aText, aSelectList, aOutSelection) {
    const choices = Array.prototype.map.call(aSelectList, (item, index) => ({
      id: String(index),
      label: item,
      disabled: false,
      selected: false,
    }));
    const result = this._prompter.showPrompt(
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
  }

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
  }

  _fillAuthInfo(aAuthInfo, aResult) {
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
  }

  promptAuth(aChannel, aLevel, aAuthInfo) {
    const result = this._prompter.showPrompt(
      this._getAuthMsg(aChannel, aLevel, aAuthInfo)
    );
    // OK: result && result.password !== undefined
    // Cancel: result && result.password === undefined
    // Error: !result
    return this._fillAuthInfo(aAuthInfo, result);
  }

  async asyncPromptAuth(aChannel, aLevel, aAuthInfo) {
    const result = await this._prompter.asyncShowPromptPromise(
      this._getAuthMsg(aChannel, aLevel, aAuthInfo)
    );
    // OK: result && result.password !== undefined
    // Cancel: result && result.password === undefined
    // Error: !result
    return this._fillAuthInfo(aAuthInfo, result);
  }

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
  }

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
  }
}

PromptDelegate.prototype.QueryInterface = ChromeUtils.generateQI(["nsIPrompt"]);
