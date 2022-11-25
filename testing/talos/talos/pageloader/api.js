/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DOM Inspector.
 *
 * The Initial Developer of the Original Code is
 * Christopher A. Aillon <christopher@aillon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher A. Aillon <christopher@aillon.com>
 *   L. David Baron, Mozilla Corporation <dbaron@dbaron.org> (modified for reftest)
 *   Vladimir Vukicevic, Mozilla Corporation <dbaron@dbaron.org> (modified for tp)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* globals AppConstants, Services, XPCOMUtils */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "aomStartup",
  "@mozilla.org/addons/addon-manager-startup;1",
  "amIAddonManagerStartup"
);

async function talosStart() {
  // Tests are driven from pageloader.xhtml.  We need to be careful to open
  // pageloader.xhtml before dismissing the default browser window or we
  // may inadvertently cause the browser to exit before the pageloader.xhtml
  // window is opened.  Start by finding or waiting for the default window.
  let defaultWin = Services.wm.getMostRecentWindow("navigator:browser");
  if (!defaultWin) {
    defaultWin = await new Promise(resolve => {
      const listener = {
        onOpenWindow(win) {
          if (
            win.docShell.domWindow.location.href ==
            AppConstants.BROWSER_CHROME_URL
          ) {
            Services.wm.removeListener(listener);
            resolve(win);
          }
        },
      };
      Services.wm.addListener(listener);
    });
  }

  // Wwe've got the default window, it is time for pageloader to take over.
  // Open pageloader.xhtml in a new window and then close the default window.
  let chromeURL = "chrome://pageloader/content/pageloader.xhtml";

  let args = {};
  args.wrappedJSObject = args;
  let newWin = Services.ww.openWindow(
    null,
    chromeURL,
    "_blank",
    "chrome,dialog=no,all",
    args
  );

  await new Promise(resolve => {
    newWin.addEventListener("load", resolve);
  });
  defaultWin.close();
}

/* globals ExtensionAPI */
this.pageloader = class extends ExtensionAPI {
  onStartup() {
    const manifestURI = Services.io.newURI(
      "manifest.json",
      null,
      this.extension.rootURI
    );
    this.chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "pageloader", "chrome/"],
    ]);

    if (Services.env.exists("MOZ_USE_PAGELOADER")) {
      // TalosPowers is a separate WebExtension that may or may not already have
      // finished loading. tryLoad is used to wait for TalosPowers to be around
      // before continuing.
      async function tryLoad() {
        try {
          ChromeUtils.importESModule(
            "resource://talos-powers/TalosParentProfiler.sys.mjs"
          );
        } catch (err) {
          await new Promise(resolve => setTimeout(resolve, 500));
          return tryLoad();
        }

        return null;
      }

      // talosStart is async but we're deliberately not await-ing or return-ing
      // it here since it doesn't block extension startup.
      tryLoad().then(() => {
        talosStart();
      });
    }
  }

  onShutdown() {
    this.chromeHandle.destruct();
  }
};
