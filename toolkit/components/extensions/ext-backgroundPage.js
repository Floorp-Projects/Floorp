var { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

// WeakMap[Extension -> BackgroundPage]
var backgroundPagesMap = new WeakMap();

// Responsible for the background_page section of the manifest.
function BackgroundPage(options, extension)
{
  this.extension = extension;
  this.scripts = options.scripts || [];
  this.page = options.page || null;
  this.contentWindow = null;
  this.webNav = null;
  this.context = null;
}

BackgroundPage.prototype = {
  build() {
    let webNav = Services.appShell.createWindowlessBrowser(false);
    this.webNav = webNav;

    let principal = Services.scriptSecurityManager.createCodebasePrincipal(this.extension.baseURI,
                                                                           {addonId: this.extension.id});

    let interfaceRequestor = webNav.QueryInterface(Ci.nsIInterfaceRequestor);
    let docShell = interfaceRequestor.getInterface(Ci.nsIDocShell);

    this.context = new ExtensionPage(this.extension, {type: "background", docShell});
    GlobalManager.injectInDocShell(docShell, this.extension, this.context);

    docShell.createAboutBlankContentViewer(principal);

    let window = webNav.document.defaultView;
    this.contentWindow = window;
    this.context.contentWindow = window;

    let url;
    if (this.page) {
      url = this.extension.baseURI.resolve(this.page);
    } else {
      url = this.extension.baseURI.resolve("_blank.html");
    }
    webNav.loadURI(url, 0, null, null, null);

    // TODO: Right now we run onStartup after the background page
    // finishes. See if this is what Chrome does.
    window.windowRoot.addEventListener("load", () => {
      if (this.scripts) {
        let doc = window.document;
        for (let script of this.scripts) {
          let url = this.extension.baseURI.resolve(script);
          let tag = doc.createElement("script");
          tag.setAttribute("src", url);
          tag.async = false;
          doc.body.appendChild(tag);
        }
      }

      if (this.extension.onStartup) {
        this.extension.onStartup();
      }
    }, true);
  },

  shutdown() {
    // Navigate away from the background page to invalidate any
    // setTimeouts or other callbacks.
    this.webNav.loadURI("about:blank", 0, null, null, null);
    this.webNav = null;
  },
};

extensions.on("manifest_background", (type, directive, extension, manifest) => {
  let bgPage = new BackgroundPage(manifest.background, extension);
  bgPage.build();
  backgroundPagesMap.set(extension, bgPage);
});

extensions.on("shutdown", (type, extension) => {
  if (backgroundPagesMap.has(extension)) {
    backgroundPagesMap.get(extension).shutdown();
    backgroundPagesMap.delete(extension);
  }
});

extensions.registerAPI((extension, context) => {
  return {
    extension: {
      getBackgroundPage: function() {
        return backgroundPagesMap.get(extension).contentWindow;
      },
    },
  };
});
