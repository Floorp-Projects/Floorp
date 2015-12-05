"use strict";

var { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

// WeakMap[Extension -> BackgroundPage]
var backgroundPagesMap = new WeakMap();

// Responsible for the background_page section of the manifest.
function BackgroundPage(options, extension) {
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

    let url;
    if (this.page) {
      url = this.extension.baseURI.resolve(this.page);
    } else {
      // TODO: Chrome uses "_generated_background_page.html" for this.
      url = this.extension.baseURI.resolve("_blank.html");
    }

    if (!this.extension.isExtensionURL(url)) {
      this.extension.manifestError("Background page must be a file within the extension");
      url = this.extension.baseURI.resolve("_blank.html");
    }

    let uri = Services.io.newURI(url, null, null);
    let principal = this.extension.createPrincipal(uri);

    let interfaceRequestor = webNav.QueryInterface(Ci.nsIInterfaceRequestor);
    let docShell = interfaceRequestor.getInterface(Ci.nsIDocShell);

    this.context = new ExtensionPage(this.extension, {type: "background", docShell, uri});
    GlobalManager.injectInDocShell(docShell, this.extension, this.context);

    docShell.createAboutBlankContentViewer(principal);

    let window = webNav.document.defaultView;
    this.contentWindow = window;
    this.context.contentWindow = window;

    webNav.loadURI(url, 0, null, null, null);

    // TODO: Right now we run onStartup after the background page
    // finishes. See if this is what Chrome does.
    let loadListener = event => {
      if (event.target != window.document) {
        return;
      }
      event.currentTarget.removeEventListener("load", loadListener, true);
      if (this.scripts) {
        let doc = window.document;
        for (let script of this.scripts) {
          let url = this.extension.baseURI.resolve(script);

          if (!this.extension.isExtensionURL(url)) {
            this.extension.manifestError("Background scripts must be files within the extension");
            continue;
          }

          let tag = doc.createElement("script");
          tag.setAttribute("src", url);
          tag.async = false;
          doc.body.appendChild(tag);
        }
      }

      if (this.extension.onStartup) {
        this.extension.onStartup();
      }
    };
    window.windowRoot.addEventListener("load", loadListener, true);
  },

  shutdown() {
    // Navigate away from the background page to invalidate any
    // setTimeouts or other callbacks.
    this.webNav.loadURI("about:blank", 0, null, null, null);
    this.webNav = null;
  },
};

/* eslint-disable mozilla/balanced-listeners */
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
/* eslint-enable mozilla/balanced-listeners */

extensions.registerAPI((extension, context) => {
  return {
    extension: {
      getBackgroundPage: function() {
        return backgroundPagesMap.get(extension).contentWindow;
      },
    },
  };
});
