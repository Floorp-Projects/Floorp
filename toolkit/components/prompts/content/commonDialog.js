/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { CommonDialog } = ChromeUtils.importESModule(
  "resource://gre/modules/CommonDialog.sys.mjs"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gContentAnalysis",
  "@mozilla.org/contentanalysis;1",
  Ci.nsIContentAnalysis
);

// imported by adjustableTitle.js loaded in the same context:
/* globals PromptUtils */

var propBag, args, Dialog;

function commonDialogOnLoad() {
  propBag = window.arguments[0]
    .QueryInterface(Ci.nsIWritablePropertyBag2)
    .QueryInterface(Ci.nsIWritablePropertyBag);
  // Convert to a JS object
  args = {};
  for (let prop of propBag.enumerator) {
    args[prop.name] = prop.value;
  }

  let dialog = document.getElementById("commonDialog");

  let needIconifiedHeader =
    args.modalType == Ci.nsIPrompt.MODAL_TYPE_CONTENT ||
    ["promptUserAndPass", "promptPassword"].includes(args.promptType) ||
    args.headerIconURL;
  let root = document.documentElement;
  if (needIconifiedHeader) {
    root.setAttribute("neediconheader", "true");
  }
  let title = { raw: args.title };
  let { promptPrincipal } = args;
  if (promptPrincipal) {
    if (promptPrincipal.isNullPrincipal) {
      title = { l10nId: "common-dialog-title-null" };
    } else if (promptPrincipal.isSystemPrincipal) {
      title = { l10nId: "common-dialog-title-system" };
      root.style.setProperty(
        "--icon-url",
        "url('chrome://branding/content/icon32.png')"
      );
    } else if (promptPrincipal.addonPolicy) {
      title.raw = promptPrincipal.addonPolicy.name;
    } else if (promptPrincipal.isContentPrincipal) {
      try {
        title.raw = promptPrincipal.URI.displayHostPort;
      } catch (ex) {
        // hostPort getter can throw, e.g. for about URIs.
        title.raw = promptPrincipal.originNoSuffix;
      }
      // hostPort can be empty for file URIs.
      if (!title.raw) {
        title.raw = promptPrincipal.prePath;
      }
    } else {
      title = { l10nId: "common-dialog-title-unknown" };
    }
  } else if (args.authOrigin) {
    title = { raw: args.authOrigin };
  }
  if (args.headerIconURL) {
    root.style.setProperty("--icon-url", `url('${args.headerIconURL}')`);
  }
  // Fade and crop potentially long raw titles, e.g., origins and hostnames.
  title.shouldUseMaskFade = title.raw && (args.authOrigin || promptPrincipal);
  root.setAttribute("headertitle", JSON.stringify(title));
  if (args.isInsecureAuth) {
    dialog.setAttribute("insecureauth", "true");
  }

  let ui = {
    prompt: window,
    loginContainer: document.getElementById("loginContainer"),
    loginTextbox: document.getElementById("loginTextbox"),
    loginLabel: document.getElementById("loginLabel"),
    password1Container: document.getElementById("password1Container"),
    password1Textbox: document.getElementById("password1Textbox"),
    password1Label: document.getElementById("password1Label"),
    infoRow: document.getElementById("infoRow"),
    infoBody: document.getElementById("infoBody"),
    infoTitle: document.getElementById("infoTitle"),
    infoIcon: document.getElementById("infoIcon"),
    checkbox: document.getElementById("checkbox"),
    checkboxContainer: document.getElementById("checkboxContainer"),
    button3: dialog.getButton("extra2"),
    button2: dialog.getButton("extra1"),
    button1: dialog.getButton("cancel"),
    button0: dialog.getButton("accept"),
    focusTarget: window,
  };

  Dialog = new CommonDialog(args, ui);
  window.addEventListener("dialogclosing", function (aEvent) {
    if (aEvent.detail?.abort) {
      Dialog.abortPrompt();
    }
  });
  document.addEventListener("dialogaccept", function () {
    Dialog.onButton0();
  });
  document.addEventListener("dialogcancel", function () {
    Dialog.onButton1();
  });
  document.addEventListener("dialogextra1", function () {
    Dialog.onButton2();
    window.close();
  });
  document.addEventListener("dialogextra2", function () {
    Dialog.onButton3();
    window.close();
  });
  document.subDialogSetDefaultFocus = isInitialFocus =>
    Dialog.setDefaultFocus(isInitialFocus);
  Dialog.onLoad(dialog);

  // resize the window to the content
  window.sizeToContent();

  // If the icon hasn't loaded yet, size the window to the content again when
  // it does, as its layout can change.
  ui.infoIcon.addEventListener("load", () => window.sizeToContent());
  if (lazy.gContentAnalysis.isActive && args.owningBrowsingContext?.isContent) {
    ui.loginTextbox?.addEventListener("paste", async event => {
      let data = event.clipboardData.getData("text/plain");
      if (data?.length > 0) {
        // Prevent the paste from happening until content analysis returns a response
        event.preventDefault();
        // Selections can be forward or backward, so use min/max
        const startIndex = Math.min(
          ui.loginTextbox.selectionStart,
          ui.loginTextbox.selectionEnd
        );
        const endIndex = Math.max(
          ui.loginTextbox.selectionStart,
          ui.loginTextbox.selectionEnd
        );
        const selectionDirection =
          endIndex < startIndex ? "backward" : "forward";
        try {
          const response = await lazy.gContentAnalysis.analyzeContentRequest(
            {
              requestToken: Services.uuid.generateUUID().toString(),
              resources: [],
              analysisType: Ci.nsIContentAnalysisRequest.eBulkDataEntry,
              operationTypeForDisplay: Ci.nsIContentAnalysisRequest.eClipboard,
              url: args.owningBrowsingContext.currentURI,
              textContent: data,
              windowGlobalParent:
                args.owningBrowsingContext.currentWindowContext,
            },
            true
          );
          if (response.shouldAllowContent) {
            ui.loginTextbox.value =
              ui.loginTextbox.value.slice(0, startIndex) +
              data +
              ui.loginTextbox.value.slice(endIndex);
            ui.loginTextbox.focus();
            if (startIndex !== endIndex) {
              // Select the pasted text
              ui.loginTextbox.setSelectionRange(
                startIndex,
                startIndex + data.length,
                selectionDirection
              );
            }
          }
        } catch (error) {
          console.error("Content analysis request returned error: ", error);
        }
      }
    });
  }

  window.getAttention();
}

function commonDialogOnUnload() {
  // Convert args back into property bag
  for (let propName in args) {
    propBag.setProperty(propName, args[propName]);
  }
}
