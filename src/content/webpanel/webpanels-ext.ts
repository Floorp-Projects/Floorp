/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "../../components/solid-xul/solid-xul";
import { webpanel2browser } from "./webPanel";

// //@ts-ignore
// ChromeUtils.defineESModuleGetters(this, {
//   ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
// });

//@ts-ignore
const { ExtensionParent } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionParent.sys.mjs",
);

//@ts-ignore
const { ExtensionUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUtils.sys.mjs",
);

const { promiseEvent } = ExtensionUtils;

interface Panel {
  uri: string;
  extension: {
    remote: boolean;
    policy?: {
      browsingContextGroupId: number;
    };
  };
  browserInsertedData?: {
    devtoolsToolboxInfo: {
      toolboxPanelId: string;
      inspectedWindowTabId: string;
    };
  };
  browserStyle: unknown;
  viewType: string; //"sidebar"
}

function getBrowser(
  // panel: Panel
) {
  // const _browser = document.getElementById("webext-panels-browser");
  // if (_browser) {
  //   return Promise.resolve(_browser);
  // }

  let stack = document.getElementById("webext-panels-stack");
  if (!stack) {
    //@ts-ignore
    stack = document.createXULElement("stack");
    stack.setAttribute("flex", "1");
    stack.setAttribute("id", "webext-panels-stack");
    document.documentElement.appendChild(stack);
  }

  // //@ts-ignore
  // browser = document.createXULElement("browser");
  // browser.setAttribute("id", "webext-panels-browser");
  // browser.setAttribute("type", "content");
  // browser.setAttribute("flex", "1");
  // browser.setAttribute("disableglobalhistory", "true");
  // browser.setAttribute("messagemanagergroup", "webext-browsers");
  // browser.setAttribute("webextension-view-type", panel.viewType);
  // browser.setAttribute("context", "contentAreaContextMenu");
  // browser.setAttribute("tooltip", "aHTMLTooltip");
  // browser.setAttribute("autocompletepopup", "PopupAutoComplete");

  // Ensure that the browser is going to run in the same bc group as the other
  // extension pages from the same addon.
  // browser.setAttribute(
  //   "initialBrowsingContextGroupId",
  //   panel.extension.policy.browsingContextGroupId.toString(),
  // );

  //let readyPromise: Promise<unknown>;
  // if (panel.extension.remote) {
  //   browser.setAttribute("remote", "true");

  //   //@ts-ignore
  //   const oa = E10SUtils.predictOriginAttributes({ browser });
  //   browser.setAttribute(
  //     "remoteType",
  //     //@ts-ignore
  //     E10SUtils.getRemoteTypeForURI(
  //       panel.uri,
  //       /* remote */ true,
  //       /* fission */ false,
  //       //@ts-ignore
  //       E10SUtils.EXTENSION_REMOTE_TYPE,
  //       null,
  //       oa,
  //     ),
  //   );
  //   browser.setAttribute("maychangeremoteness", "true");

  //   readyPromise = promiseEvent(browser, "XULFrameLoaderCreated");
  // } else {
  //readyPromise = Promise.resolve();
  // }

  //stack.appendChild(browser);

  // browser.addEventListener(
  //   "DoZoomEnlargeBy10",
  //   () => {
  //     const { ZoomManager } = browser.ownerGlobal;
  //     let zoom = browser.fullZoom;
  //     zoom += 0.1;
  //     if (zoom > ZoomManager.MAX) {
  //       zoom = ZoomManager.MAX;
  //     }
  //     browser.fullZoom = zoom;
  //   },
  //   true,
  // );
  // browser.addEventListener(
  //   "DoZoomReduceBy10",
  //   () => {
  //     let { ZoomManager } = browser.ownerGlobal;
  //     let zoom = browser.fullZoom;
  //     zoom -= 0.1;
  //     if (zoom < ZoomManager.MIN) {
  //       zoom = ZoomManager.MIN;
  //     }
  //     browser.fullZoom = zoom;
  //   },
  //   true,
  // );

  const initBrowser = () => {
    const browser = document.getElementById("tmp");
    //@ts-ignore
    // ExtensionParent.apiManager.emit(
    //   "extension-browser-inserted",
    //   browser,
    //   null,
    //   //panel.browserInsertedData,
    // );

    // browser.messageManager.loadFrameScript(
    //   "chrome://extensions/content/ext-browser-content.js",
    //   false,
    //   true,
    // );

    const options: {
      stylesheets?: string[];
    } = {};
    // if (panel.browserStyle) {
    //   options.stylesheets = ["chrome://browser/content/extension.css"];
    // }
    //browser.messageManager.sendAsyncMessage("Extension:InitBrowser", options);
    return browser;
  };

  //browser.addEventListener("DidChangeBrowserRemoteness", initBrowser);
  const browser = webpanel2browser("tmp", "noraneko_main_panel", initBrowser);
  render(() => browser, stack);
  return Promise.resolve(initBrowser());
}

// function updatePosition() {
//   // We need both of these to make sure we update the position
//   // after any lower level updates have finished.
//   requestAnimationFrame(() =>
//     setTimeout(() => {
//       const browser = document.getElementById("webext-panels-browser");
//       if (browser?.isRemoteBrowser) {
//         browser.frameLoader.requestUpdatePosition();
//       }
//     }, 0),
//   );
// }

function loadPanel(
  //extensionId: string,
  extensionUrl: string,
  //browserStyle: string,
) {
  // const browserEl = document.getElementById("webext-panels-browser");
  // if (browserEl) {
  //   if (browserEl.currentURI.spec === extensionUrl) {
  //     return;
  //   }
  //   // Forces runtime disconnect.  Remove the stack (parent).
  //   browserEl.parentNode.remove();
  // }

  //const policy = WebExtensionPolicy.getByID(extensionId);

  // const sidebar = {
  //   uri: extensionUrl,
  //   extension: policy.extension,
  //   browserStyle,
  //   viewType: "sidebar",
  // };

  getBrowser().then((browser) => {
    //const uri = Services.io.newURI(policy.getURL());
    // const triggeringPrincipal =
    //   Services.scriptSecurityManager.createContentPrincipal(uri, {});
    // browser.loadURI(
    //   extensionUrl,
    //   // { triggeringPrincipal }
    // );
  });
}

// Stub tabbrowser implementation to make sure that links from inside
// extension sidebar panels open in new tabs, see bug 1488055.
//@ts-ignore
window.gBrowser = {
  get selectedBrowser() {
    return document.getElementById("tmp");
  },

  getTabForBrowser() {
    return null;
  },
};

//@ts-ignore
window.loadPanel = loadPanel;

window.addEventListener("DOMContentLoaded", () => {
  loadPanel("https://google.com");
  console.log(document.getElementById("tmp"));
});
