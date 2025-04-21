/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

export const env = Services.env;
export const isMainBrowser = env.get("MOZ_BROWSER_TOOLBOX_PORT") === "";

export let isFirstRun = false;
export let isUpdated = false;

const executedFunctions = new Set<string>();

export function executeOnce(id: string, callback: () => void): boolean {
  if (executedFunctions.has(id)) {
    return false;
  }

  callback();

  executedFunctions.add(id);
  return true;
}

function initializeVersionInfo(): void {
  isFirstRun = !Services.prefs.getStringPref(
    "browser.startup.homepage_override.mstone",
    undefined,
  );

  const nowVersion = AppConstants.MOZ_APP_VERSION_DISPLAY;
  const oldVersionPref = Services.prefs.getStringPref(
    "floorp.startup.oldVersion",
    undefined,
  );

  if (oldVersionPref !== nowVersion && !isFirstRun) {
    isUpdated = true;
  }

  Services.prefs.setStringPref("floorp.startup.oldVersion", nowVersion);
}

export function onFinalUIStartup(): void {
  Services.obs.removeObserver(onFinalUIStartup, "final-ui-startup");

  createDefaultUserChromeFiles().catch((error) => {
    console.error("Failed to create default userChrome files:", error);
  });
}

async function createDefaultUserChromeFiles(): Promise<void> {
  const chromeDir = PathUtils.join(
    Services.dirsvc.get("ProfD", Ci.nsIFile).path,
    "chrome",
  );
  const chromeExists = await IOUtils.exists(chromeDir);

  if (!chromeExists) {
    const userChromeCssPath = PathUtils.join(chromeDir, "userChrome.css");
    const userContentCssPath = PathUtils.join(chromeDir, "userContent.css");

    await IOUtils.writeUTF8(
      userChromeCssPath,
      `
/*************************************************************************************************************************************************************************************************************************************************************

"userChrome.css" is a custom CSS file that can be used to specify CSS style rules for Floorp's interface (NOT internal site) using "chrome" privileges.
For instance, if you want to hide the tab bar, you can use the following CSS rule:

**************************************
#TabsToolbar {                       *
    display: none !important;        *
}                                    *
**************************************

NOTE: You can use the userChrome.css file without change preferences (about:config)

Quote: https://userChrome.org | https://github.com/topics/userchrome 

************************************************************************************************************************************************************************************************************************************************************/

@charset "UTF-8";
@-moz-document url(chrome://browser/content/browser.xhtml) {
/* Please write your custom CSS under this line*/


}
`,
    );

    await IOUtils.writeUTF8(
      userContentCssPath,
      `
/*************************************************************************************************************************************************************************************************************************************************************

"userContent.css" is a custom CSS file that can be used to specify CSS style rules for Floorp's internal site using "chrome" privileges.
For instance, if you want to apply CSS at "about:newtab" and "about:home", you can use the following CSS rule:

***********************************************************************
@-moz-document url-prefix("about:newtab"), url-prefix("about:home") { *
                                                                      *
* Write your css *                                                    *
                                                                      *
}                                                                     *
***********************************************************************

NOTE: You can use the userContent.css file without change preferences (about:config)

************************************************************************************************************************************************************************************************************************************************************/

@charset "UTF-8";
/* Please write your custom CSS under this line*/
`,
    );
  }
}


// Only for dev build
function setupNoranekoNewTab(): void {
  const { AboutNewTab } = ChromeUtils.importESModule(
    "resource:///modules/AboutNewTab.sys.mjs",
  );

  if (!isNewTabFileAvailable()) {
    AboutNewTab.newTabURL = "http://localhost:5186/";
    Services.prefs.setStringPref("browser.startup.homepage", "http://localhost:5186/");
  }
}

function isNewTabFileAvailable(): boolean {
  const response = fetch("chrome://noraneko-newtab/content/index.html");
  response.then(() => {
    return true;
  })
  return false;
}

function checkNewtabUserPreference(): boolean {
  const result = Services.prefs.getStringPref("floorp.design.configs");

  if (!result) {
    return true;
  }

  const data = JSON.parse(result);

  return !data.uiCustomization.disableFloorpStart
}

/* Register Custom About Pages
 *
 * Credits: angelbruni/Geckium on GitHub
 * This code is the TypeScript version of the original JavaScript code.
 *
 * File referred: https://github.com/angelbruni/Geckium/blob/main/Profile%20Folder/chrome/JS/Geckium_aboutPageRegisterer.uc.js
 */

const customAboutPages: Record<string, string> = {
  "hub" : "chrome://noraneko-settings/content/index.html",
  "welcome" : "chrome://noraneko-welcome/content/index.html",
  ...(checkNewtabUserPreference() && {
    "newtab" : isNewTabFileAvailable() ? "chrome://noraneko-newtab/content/index.html" : "http://localhost:5186",
    "home" : isNewTabFileAvailable() ? "chrome://noraneko-newtab/content/index.html" : "http://localhost:5186",
  }),
};

class CustomAboutPage {
  private _uri: nsIURI;

  constructor(urlString: string) {
    this._uri = Services.io.newURI(urlString);
  }

  get uri(): nsIURI {
    return this._uri;
  }

  newChannel(_uri: nsIURI, loadInfo: nsILoadInfo): nsIChannel {
    const new_ch = Services.io.newChannelFromURIWithLoadInfo(this.uri, loadInfo);
    new_ch.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return new_ch;
  }

  getURIFlags(_uri: nsIURI): number {
    return Ci.nsIAboutModule.ALLOW_SCRIPT | Ci.nsIAboutModule.IS_SECURE_CHROME_UI;
  }

  getChromeURI(_uri: nsIURI): nsIURI {
    return this.uri;
  }

  QueryInterface = ChromeUtils.generateQI(["nsIAboutModule"]);
}

function registerCustomAboutPages(): void {
  for (const aboutKey in customAboutPages) {
    const AboutModuleFactory: nsIFactory = {
      createInstance(aIID: nsIID): MozQueryInterface {
        return new CustomAboutPage(customAboutPages[aboutKey]).QueryInterface(aIID);
      }
    };

    Components.manager.QueryInterface(Ci.nsIComponentRegistrar).registerFactory(
      Components.ID(Services.uuid.generateUUID().toString()),
      `about:${aboutKey}`,
      `@mozilla.org/network/protocol/about;1?what=${aboutKey}`,
      AboutModuleFactory
    );
  }
}

initializeVersionInfo();
setupNoranekoNewTab();
registerCustomAboutPages();

if (isMainBrowser) {
  Services.obs.addObserver(onFinalUIStartup, "final-ui-startup");
}
