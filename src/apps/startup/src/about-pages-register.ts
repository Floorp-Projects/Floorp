/* Credits:
 * Thanks to angelbruni/Geckium on GitHub for the original code.
 * This code is the TypeScript version of the original JavaScript code.
 *
 * File referred: https://github.com/angelbruni/Geckium/blob/main/Profile%20Folder/chrome/JS/Geckium_aboutPageRegisterer.uc.js
 *
 */

const customAboutPages: Record<string, string> = {
  "hub" : "chrome://noraneko-settings/content/index.html",
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