/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
// This is redefined below, for strange and unfortunate reasons.
var { PromptUtils } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);

function Prompter() {
  // Note that EmbedPrompter clones this implementation.
}

Prompter.prototype = {
  classID: Components.ID("{1c978d25-b37f-43a8-a2d6-0c7a239ead87}"),
  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIPromptFactory,
    Ci.nsIPromptService,
  ]),

  /* ----------  private members  ---------- */

  pickPrompter(options) {
    return new ModalPrompter(options);
  },

  /* ----------  nsIPromptFactory  ---------- */

  getPrompt(domWin, iid) {
    // This is still kind of dumb; the C++ code delegated to login manager
    // here, which in turn calls back into us via nsIPromptService.
    if (iid.equals(Ci.nsIAuthPrompt2) || iid.equals(Ci.nsIAuthPrompt)) {
      try {
        let pwmgr = Cc[
          "@mozilla.org/passwordmanager/authpromptfactory;1"
        ].getService(Ci.nsIPromptFactory);
        return pwmgr.getPrompt(domWin, iid);
      } catch (e) {
        Cu.reportError(
          "nsPrompter: Delegation to password manager failed: " + e
        );
      }
    }

    let p = new ModalPrompter({ domWin });
    p.QueryInterface(iid);
    return p;
  },

  /* ----------  nsIPromptService  ---------- */

  alert(domWin, title, text) {
    let p = this.pickPrompter({ domWin });
    p.alert(title, text);
  },

  alertBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    p.alert(...promptArgs);
  },

  alertCheck(domWin, title, text, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    p.alertCheck(title, text, checkLabel, checkValue);
  },

  alertCheckBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    p.alertCheck(...promptArgs);
  },

  confirm(domWin, title, text) {
    let p = this.pickPrompter({ domWin });
    return p.confirm(title, text);
  },

  confirmBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.confirm(...promptArgs);
  },

  confirmCheck(domWin, title, text, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.confirmCheck(title, text, checkLabel, checkValue);
  },

  confirmCheckBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.confirmCheck(...promptArgs);
  },

  confirmEx(
    domWin,
    title,
    text,
    flags,
    button0,
    button1,
    button2,
    checkLabel,
    checkValue
  ) {
    let p = this.pickPrompter({ domWin });
    return p.confirmEx(
      title,
      text,
      flags,
      button0,
      button1,
      button2,
      checkLabel,
      checkValue
    );
  },

  confirmExBC(
    browsingContext,
    modalType,
    title,
    text,
    flags,
    button0,
    button1,
    button2,
    checkLabel,
    checkValue
  ) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.confirmEx(
      title,
      text,
      flags,
      button0,
      button1,
      button2,
      checkLabel,
      checkValue
    );
  },

  prompt(domWin, title, text, value, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_prompt(title, text, value, checkLabel, checkValue);
  },

  promptBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_prompt(...promptArgs);
  },

  promptUsernameAndPassword(
    domWin,
    title,
    text,
    user,
    pass,
    checkLabel,
    checkValue
  ) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_promptUsernameAndPassword(
      title,
      text,
      user,
      pass,
      checkLabel,
      checkValue
    );
  },

  promptUsernameAndPasswordBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_promptUsernameAndPassword(...promptArgs);
  },

  promptPassword(domWin, title, text, pass, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_promptPassword(
      title,
      text,
      pass,
      checkLabel,
      checkValue
    );
  },

  promptPasswordBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_promptPassword(...promptArgs);
  },

  select(domWin, title, text, list, selected) {
    let p = this.pickPrompter({ domWin });
    return p.select(title, text, list, selected);
  },

  selectBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.select(...promptArgs);
  },

  promptAuth(domWin, channel, level, authInfo, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.promptAuth(channel, level, authInfo, checkLabel, checkValue);
  },

  promptAuthBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.promptAuth(...promptArgs);
  },

  asyncPromptAuth(
    domWin,
    channel,
    callback,
    context,
    level,
    authInfo,
    checkLabel,
    checkValue
  ) {
    let p = this.pickPrompter({ domWin });
    return p.asyncPromptAuth(
      channel,
      callback,
      context,
      level,
      authInfo,
      checkLabel,
      checkValue
    );
  },

  asyncPromptAuthBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.asyncPromptAuth(...promptArgs);
  },
};

// Common utils not specific to a particular prompter style.
var PromptUtilsTemp = {
  __proto__: PromptUtils,

  getLocalizedString(key, formatArgs) {
    if (formatArgs) {
      return this.strBundle.formatStringFromName(key, formatArgs);
    }
    return this.strBundle.GetStringFromName(key);
  },

  confirmExHelper(flags, button0, button1, button2) {
    const BUTTON_DEFAULT_MASK = 0x03000000;
    let defaultButtonNum = (flags & BUTTON_DEFAULT_MASK) >> 24;
    let isDelayEnabled = flags & Ci.nsIPrompt.BUTTON_DELAY_ENABLE;

    // Flags can be used to select a specific pre-defined button label or
    // a caller-supplied string (button0/button1/button2). If no flags are
    // set for a button, then the button won't be shown.
    let argText = [button0, button1, button2];
    let buttonLabels = [null, null, null];
    for (let i = 0; i < 3; i++) {
      let buttonLabel;
      switch (flags & 0xff) {
        case Ci.nsIPrompt.BUTTON_TITLE_OK:
          buttonLabel = PromptUtils.getLocalizedString("OK");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_CANCEL:
          buttonLabel = PromptUtils.getLocalizedString("Cancel");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_YES:
          buttonLabel = PromptUtils.getLocalizedString("Yes");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_NO:
          buttonLabel = PromptUtils.getLocalizedString("No");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_SAVE:
          buttonLabel = PromptUtils.getLocalizedString("Save");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_DONT_SAVE:
          buttonLabel = PromptUtils.getLocalizedString("DontSave");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_REVERT:
          buttonLabel = PromptUtils.getLocalizedString("Revert");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_IS_STRING:
          buttonLabel = argText[i];
          break;
      }
      if (buttonLabel) {
        buttonLabels[i] = buttonLabel;
      }
      flags >>= 8;
    }

    return [
      buttonLabels[0],
      buttonLabels[1],
      buttonLabels[2],
      defaultButtonNum,
      isDelayEnabled,
    ];
  },

  getAuthInfo(authInfo) {
    let username, password;

    let flags = authInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN && authInfo.domain) {
      username = authInfo.domain + "\\" + authInfo.username;
    } else {
      username = authInfo.username;
    }

    password = authInfo.password;

    return [username, password];
  },

  setAuthInfo(authInfo, username, password) {
    let flags = authInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
      // Domain is separated from username by a backslash
      let idx = username.indexOf("\\");
      if (idx == -1) {
        authInfo.username = username;
      } else {
        authInfo.domain = username.substring(0, idx);
        authInfo.username = username.substring(idx + 1);
      }
    } else {
      authInfo.username = username;
    }
    authInfo.password = password;
  },

  /**
   * Strip out things like userPass and path for display.
   */
  getFormattedHostname(uri) {
    return uri.scheme + "://" + uri.hostPort;
  },

  // Copied from login manager
  getAuthTarget(aChannel, aAuthInfo) {
    let hostname, realm;

    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
      if (!(aChannel instanceof Ci.nsIProxiedChannel)) {
        throw new Error("proxy auth needs nsIProxiedChannel");
      }

      let info = aChannel.proxyInfo;
      if (!info) {
        throw new Error("proxy auth needs nsIProxyInfo");
      }

      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      let idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
        Ci.nsIIDNService
      );
      hostname =
        "moz-proxy://" +
        idnService.convertUTF8toACE(info.host) +
        ":" +
        info.port;
      realm = aAuthInfo.realm;
      if (!realm) {
        realm = hostname;
      }

      return [hostname, realm];
    }

    hostname = this.getFormattedHostname(aChannel.URI);

    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    realm = aAuthInfo.realm;
    if (!realm) {
      realm = hostname;
    }

    return [hostname, realm];
  },

  makeAuthMessage(channel, authInfo) {
    let isProxy = authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY;
    let isPassOnly = authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD;
    let isCrossOrig =
      authInfo.flags & Ci.nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE;

    let username = authInfo.username;
    let [displayHost, realm] = this.getAuthTarget(channel, authInfo);

    // Suppress "the site says: $realm" when we synthesized a missing realm.
    if (!authInfo.realm && !isProxy) {
      realm = "";
    }

    // Trim obnoxiously long realms.
    if (realm.length > 150) {
      realm = realm.substring(0, 150);
      // Append "..." (or localized equivalent).
      realm += this.ellipsis;
    }

    let text;
    if (isProxy) {
      text = PromptUtils.getLocalizedString("EnterLoginForProxy3", [
        realm,
        displayHost,
      ]);
    } else if (isPassOnly) {
      text = PromptUtils.getLocalizedString("EnterPasswordFor", [
        username,
        displayHost,
      ]);
    } else if (isCrossOrig) {
      text = PromptUtils.getLocalizedString(
        "EnterUserPasswordForCrossOrigin2",
        [displayHost]
      );
    } else if (!realm) {
      text = PromptUtils.getLocalizedString("EnterUserPasswordFor2", [
        displayHost,
      ]);
    } else {
      text = PromptUtils.getLocalizedString("EnterLoginForRealm3", [
        realm,
        displayHost,
      ]);
    }

    return text;
  },

  getBrandFullName() {
    return this.brandBundle.GetStringFromName("brandFullName");
  },
};

PromptUtils = PromptUtilsTemp;

XPCOMUtils.defineLazyGetter(PromptUtils, "strBundle", function() {
  let bundle = Services.strings.createBundle(
    "chrome://global/locale/commonDialogs.properties"
  );
  if (!bundle) {
    throw new Error("String bundle for Prompter not present!");
  }
  return bundle;
});

XPCOMUtils.defineLazyGetter(PromptUtils, "brandBundle", function() {
  let bundle = Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
  if (!bundle) {
    throw new Error("String bundle for branding not present!");
  }
  return bundle;
});

XPCOMUtils.defineLazyGetter(PromptUtils, "ellipsis", function() {
  let ellipsis = "\u2026";
  try {
    ellipsis = Services.prefs.getComplexValue(
      "intl.ellipsis",
      Ci.nsIPrefLocalizedString
    ).data;
  } catch (e) {}
  return ellipsis;
});

class ModalPrompter {
  constructor({ browsingContext = null, domWin = null, modalType = null }) {
    if (browsingContext && domWin) {
      throw new Error("Pass either browsingContext or domWin");
    }
    this.browsingContext = browsingContext;
    this._domWin = domWin;

    if (this._domWin) {
      // We have a domWin, get the associated browsing context
      this.browsingContext = BrowsingContext.getFromWindow(this._domWin);
    } else if (this.browsingContext) {
      // We have a browsingContext, get the associated dom window
      if (this.browsingContext.window) {
        this._domWin = this.browsingContext.window;
      } else {
        this._domWin =
          this.browsingContext.embedderElement &&
          this.browsingContext.embedderElement.ownerGlobal;
      }
    }

    // Use given modal type or fallback to default
    this.modalType = modalType || ModalPrompter.defaultModalType;

    this.QueryInterface = ChromeUtils.generateQI([
      Ci.nsIPrompt,
      Ci.nsIAuthPrompt,
      Ci.nsIAuthPrompt2,
      Ci.nsIWritablePropertyBag2,
    ]);
  }

  set modalType(modalType) {
    // Setting modal type window is always allowed
    if (modalType == Ci.nsIPrompt.MODAL_TYPE_WINDOW) {
      this._modalType = modalType;
      return;
    }

    // If we have a chrome window and the browsing context isn't embedded
    // in a browser, we can't use tab/content prompts.
    // Or if we don't allow tab or content prompts, override modalType
    // argument to use window prompts
    if (
      !this.browsingContext ||
      !this._domWin ||
      (this._domWin.isChromeWindow &&
        !this.browsingContext.top.embedderElement) ||
      !ModalPrompter.tabModalEnabled
    ) {
      modalType = Ci.nsIPrompt.MODAL_TYPE_WINDOW;

      Cu.reportError(
        "Prompter: Browser not available or tab modal prompts disabled. Falling back to window prompt."
      );
    }
    this._modalType = modalType;
  }

  get modalType() {
    return this._modalType;
  }

  /* ---------- internal methods ---------- */

  openPrompt(args) {
    if (!this.browsingContext) {
      // We don't have a browsing context, fallback to a window prompt

      // There's an implied contract that says modal prompts should still work
      // when no "parent" window is passed for the dialog (eg, the "Master
      // Password" dialog does this).  These prompts must be shown even if there
      // are *no* visible windows at all.

      // We try and find a window to use as the parent, but don't consider
      // if that is visible before showing the prompt.
      let parentWindow = Services.ww.activeWindow;
      // parentWindow may still be null here if there are _no_ windows open.

      this.openWindowPrompt(parentWindow, args);
      return;
    }

    // Select prompts are not part of CommonDialog
    // and thus not supported as tab or content prompts yet. See Bug 1622817.
    // Once they are integrated this override should be removed.
    if (
      args.promptType == "select" &&
      this.modalType !== Ci.nsIPrompt.MODAL_TYPE_WINDOW
    ) {
      Cu.reportError(
        "Prompter: 'select' prompts do not support tab/content prompting. Falling back to window prompt."
      );
      args.modalType = Ci.nsIPrompt.MODAL_TYPE_WINDOW;
    } else {
      args.modalType = this.modalType;
    }

    args.browsingContext = this.browsingContext;

    let actor = this._domWin.windowGlobalChild.getActor("Prompt");

    let docShell =
      (this.browsingContext && this.browsingContext.docShell) ||
      this._domWin.docShell;
    let inPermitUnload =
      docShell.contentViewer && docShell.contentViewer.inPermitUnload;
    let eventDetail = Cu.cloneInto(
      {
        tabPrompt: this.modalType != Ci.nsIPrompt.MODAL_TYPE_WINDOW,
        inPermitUnload,
      },
      this._domWin
    );
    PromptUtils.fireDialogEvent(
      this._domWin,
      "DOMWillOpenModalDialog",
      null,
      eventDetail
    );

    let windowUtils =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT &&
      this._domWin.windowUtils;

    // Put content windows in the modal state while the prompt is open.
    if (windowUtils) {
      windowUtils.enterModalState();
    }

    // It is technically possible for multiple prompts to be sent from a single
    // BrowsingContext. See bug 1266353. We use a randomly generated UUID to
    // differentiate between the different prompts.
    let id =
      "id" +
      Cc["@mozilla.org/uuid-generator;1"]
        .getService(Ci.nsIUUIDGenerator)
        .generateUUID()
        .toString();

    let closed = false;

    args.promptPrincipal = this._domWin.document.nodePrincipal;
    args.inPermitUnload = inPermitUnload;
    args._remoteId = id;

    actor
      .sendQuery("Prompt:Open", args)
      .then(returnedArgs => {
        // Copy the response from the closed prompt into our args, it will be
        // read by our caller.
        if (!returnedArgs) {
          return;
        }

        if (returnedArgs.promptAborted) {
          throw Components.Exception(
            "prompt aborted by user",
            Cr.NS_ERROR_NOT_AVAILABLE
          );
        }

        if (returnedArgs._remoteId !== id) {
          return;
        }

        for (let key in returnedArgs) {
          args[key] = returnedArgs[key];
        }
      })
      .finally(() => {
        closed = true;
      });

    Services.tm.spinEventLoopUntilOrShutdown(() => closed);

    if (windowUtils) {
      windowUtils.leaveModalState();
    }
    PromptUtils.fireDialogEvent(this._domWin, "DOMModalDialogClosed");
  }

  openWindowPrompt(parentWindow, args) {
    const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";
    const SELECT_DIALOG = "chrome://global/content/selectDialog.xhtml";

    let uri = args.promptType == "select" ? SELECT_DIALOG : COMMON_DIALOG;
    let propBag = PromptUtils.objectToPropBag(args);
    Services.ww.openWindow(
      parentWindow,
      uri,
      "_blank",
      "centerscreen,chrome,modal,titlebar",
      propBag
    );
    PromptUtils.propBagToObject(propBag, args);
  }

  /*
   * ---------- interface disambiguation ----------
   *
   * nsIPrompt and nsIAuthPrompt share 3 method names with slightly
   * different arguments. All but prompt() have the same number of
   * arguments, so look at the arg types to figure out how we're being
   * called. :-(
   */
  prompt() {
    // also, the nsIPrompt flavor has 5 args instead of 6.
    if (typeof arguments[2] == "object") {
      return this.nsIPrompt_prompt.apply(this, arguments);
    }
    return this.nsIAuthPrompt_prompt.apply(this, arguments);
  }

  promptUsernameAndPassword() {
    // Both have 6 args, so use types.
    if (typeof arguments[2] == "object") {
      return this.nsIPrompt_promptUsernameAndPassword.apply(this, arguments);
    }
    return this.nsIAuthPrompt_promptUsernameAndPassword.apply(this, arguments);
  }

  promptPassword() {
    // Both have 5 args, so use types.
    if (typeof arguments[2] == "object") {
      return this.nsIPrompt_promptPassword.apply(this, arguments);
    }
    return this.nsIAuthPrompt_promptPassword.apply(this, arguments);
  }

  /* ----------  nsIPrompt  ---------- */

  alert(title, text) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Alert");
    }

    let args = {
      promptType: "alert",
      title,
      text,
    };

    this.openPrompt(args);
  }

  alertCheck(title, text, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Alert");
    }

    let args = {
      promptType: "alertCheck",
      title,
      text,
      checkLabel,
      checked: checkValue.value,
    };

    this.openPrompt(args);

    // Checkbox state always returned, even if cancel clicked.
    checkValue.value = args.checked;
  }

  confirm(title, text) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Confirm");
    }

    let args = {
      promptType: "confirm",
      title,
      text,
      ok: false,
    };

    this.openPrompt(args);

    // Did user click Ok or Cancel?
    return args.ok;
  }

  confirmCheck(title, text, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("ConfirmCheck");
    }

    let args = {
      promptType: "confirmCheck",
      title,
      text,
      checkLabel,
      checked: checkValue.value,
      ok: false,
    };

    this.openPrompt(args);

    // Checkbox state always returned, even if cancel clicked.
    checkValue.value = args.checked;

    // Did user click Ok or Cancel?
    return args.ok;
  }

  confirmEx(
    title,
    text,
    flags,
    button0,
    button1,
    button2,
    checkLabel,
    checkValue
  ) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Confirm");
    }

    let args = {
      promptType: "confirmEx",
      title,
      text,
      checkLabel,
      checked: checkValue.value,
      ok: false,
      buttonNumClicked: 1,
    };

    let [
      label0,
      label1,
      label2,
      defaultButtonNum,
      isDelayEnabled,
    ] = PromptUtils.confirmExHelper(flags, button0, button1, button2);

    args.defaultButtonNum = defaultButtonNum;
    args.enableDelay = isDelayEnabled;

    if (label0) {
      args.button0Label = label0;
      if (label1) {
        args.button1Label = label1;
        if (label2) {
          args.button2Label = label2;
        }
      }
    }

    this.openPrompt(args);

    // Checkbox state always returned, even if cancel clicked.
    checkValue.value = args.checked;

    // Get the number of the button the user clicked.
    return args.buttonNumClicked;
  }

  nsIPrompt_prompt(title, text, value, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Prompt");
    }

    let args = {
      promptType: "prompt",
      title,
      text,
      value: value.value,
      checkLabel,
      checked: checkValue.value,
      ok: false,
    };

    this.openPrompt(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      checkValue.value = args.checked;
      value.value = args.value;
    }

    return ok;
  }

  nsIPrompt_promptUsernameAndPassword(
    title,
    text,
    user,
    pass,
    checkLabel,
    checkValue
  ) {
    if (!title) {
      title = PromptUtils.getLocalizedString("PromptUsernameAndPassword3", [
        PromptUtils.getBrandFullName(),
      ]);
    }

    let args = {
      promptType: "promptUserAndPass",
      title,
      text,
      user: user.value,
      pass: pass.value,
      checkLabel,
      checked: checkValue.value,
      ok: false,
    };

    this.openPrompt(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      checkValue.value = args.checked;
      user.value = args.user;
      pass.value = args.pass;
    }

    return ok;
  }

  nsIPrompt_promptPassword(title, text, pass, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("PromptPassword3", [
        PromptUtils.getBrandFullName(),
      ]);
    }

    let args = {
      promptType: "promptPassword",
      title,
      text,
      pass: pass.value,
      checkLabel,
      checked: checkValue.value,
      ok: false,
    };

    this.openPrompt(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      checkValue.value = args.checked;
      pass.value = args.pass;
    }

    return ok;
  }

  select(title, text, list, selected) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Select");
    }

    let args = {
      promptType: "select",
      title,
      text,
      list,
      selected: -1,
      ok: false,
    };

    this.openPrompt(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      selected.value = args.selected;
    }

    return ok;
  }

  /* ----------  nsIAuthPrompt  ---------- */

  nsIAuthPrompt_prompt(
    title,
    text,
    passwordRealm,
    savePassword,
    defaultText,
    result
  ) {
    // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
    if (defaultText) {
      result.value = defaultText;
    }
    return this.nsIPrompt_prompt(title, text, result, null, {});
  }

  nsIAuthPrompt_promptUsernameAndPassword(
    title,
    text,
    passwordRealm,
    savePassword,
    user,
    pass
  ) {
    // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
    return this.nsIPrompt_promptUsernameAndPassword(
      title,
      text,
      user,
      pass,
      null,
      {}
    );
  }

  nsIAuthPrompt_promptPassword(title, text, passwordRealm, savePassword, pass) {
    // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
    return this.nsIPrompt_promptPassword(title, text, pass, null, {});
  }

  /* ----------  nsIAuthPrompt2  ---------- */

  promptAuth(channel, level, authInfo, checkLabel, checkValue) {
    let message = PromptUtils.makeAuthMessage(channel, authInfo);

    let [username, password] = PromptUtils.getAuthInfo(authInfo);

    let userParam = { value: username };
    let passParam = { value: password };

    let ok;
    if (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD) {
      ok = this.nsIPrompt_promptPassword(
        null,
        message,
        passParam,
        checkLabel,
        checkValue
      );
    } else {
      ok = this.nsIPrompt_promptUsernameAndPassword(
        null,
        message,
        userParam,
        passParam,
        checkLabel,
        checkValue
      );
    }

    if (ok) {
      PromptUtils.setAuthInfo(authInfo, userParam.value, passParam.value);
    }
    return ok;
  }

  asyncPromptAuth(
    channel,
    callback,
    context,
    level,
    authInfo,
    checkLabel,
    checkValue
  ) {
    // Nothing calls this directly; netwerk ends up going through
    // nsIPromptService::GetPrompt, which delegates to login manager.
    // Login manger handles the async bits itself, and only calls out
    // promptAuth, never asyncPromptAuth.
    //
    // Bug 565582 will change this.
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  /* ----------  nsIWritablePropertyBag2 ---------- */
  // Legacy way to set modal type when prompting via nsIPrompt.
  // Please prompt via nsIPromptService. This will be removed in the future.
  setPropertyAsUint32(name, value) {
    if (name == "modalType") {
      this.modalType = value;
    } else {
      throw Cr.NS_ERROR_ILLEGAL_VALUE;
    }
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  ModalPrompter,
  "defaultModalType",
  "prompts.defaultModalType",
  Ci.nsIPrompt.MODAL_TYPE_WINDOW
);

XPCOMUtils.defineLazyPreferenceGetter(
  ModalPrompter,
  "tabModalEnabled",
  "prompts.tab_modal.enabled",
  true
);

function AuthPromptAdapterFactory() {}
AuthPromptAdapterFactory.prototype = {
  classID: Components.ID("{6e134924-6c3a-4d86-81ac-69432dd971dc}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAuthPromptAdapterFactory]),

  /* ----------  nsIAuthPromptAdapterFactory ---------- */

  createAdapter(oldPrompter) {
    return new AuthPromptAdapter(oldPrompter);
  },
};

// Takes an nsIAuthPrompt implementation, wraps it with a nsIAuthPrompt2 shell.
function AuthPromptAdapter(oldPrompter) {
  this.oldPrompter = oldPrompter;
}
AuthPromptAdapter.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAuthPrompt2]),
  oldPrompter: null,

  /* ----------  nsIAuthPrompt2 ---------- */

  promptAuth(channel, level, authInfo, checkLabel, checkValue) {
    let message = PromptUtils.makeAuthMessage(channel, authInfo);

    let [username, password] = PromptUtils.getAuthInfo(authInfo);
    let userParam = { value: username };
    let passParam = { value: password };

    let [host, realm] = PromptUtils.getAuthTarget(channel, authInfo);
    let authTarget = host + " (" + realm + ")";

    let ok;
    if (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD) {
      ok = this.oldPrompter.promptPassword(
        null,
        message,
        authTarget,
        Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY,
        passParam
      );
    } else {
      ok = this.oldPrompter.promptUsernameAndPassword(
        null,
        message,
        authTarget,
        Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY,
        userParam,
        passParam
      );
    }

    if (ok) {
      PromptUtils.setAuthInfo(authInfo, userParam.value, passParam.value);
    }
    return ok;
  },

  asyncPromptAuth(
    channel,
    callback,
    context,
    level,
    authInfo,
    checkLabel,
    checkValue
  ) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
};

// Wrapper using the old embedding contractID, since it's already common in
// the addon ecosystem.
function EmbedPrompter() {}
EmbedPrompter.prototype = new Prompter();
EmbedPrompter.prototype.classID = Components.ID(
  "{7ad1b327-6dfa-46ec-9234-f2a620ea7e00}"
);

var EXPORTED_SYMBOLS = [
  "Prompter",
  "EmbedPrompter",
  "AuthPromptAdapterFactory",
];
