/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This script contains the minimum, skeleton content process code that we need
 * in order to lazily load other extension modules when they are first
 * necessary. Anything which is not likely to be needed immediately, or shortly
 * after startup, in *every* browser process live outside of this file.
 */

var EXPORTED_SYMBOLS = ["ExtensionProcessScript", "ExtensionAPIRequestHandler"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ExtensionChild: "resource://gre/modules/ExtensionChild.jsm",
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.jsm",
  ExtensionContent: "resource://gre/modules/ExtensionContent.jsm",
  ExtensionPageChild: "resource://gre/modules/ExtensionPageChild.jsm",
  ExtensionWorkerChild: "resource://gre/modules/ExtensionWorkerChild.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
});

const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "console", () =>
  ExtensionCommon.getConsole()
);

const { DefaultWeakMap } = ExtensionUtils;

const { sharedData } = Services.cpmm;

function getData(extension, key = "") {
  return sharedData.get(`extension/${extension.id}/${key}`);
}

// We need to avoid touching Services.appinfo here in order to prevent
// the wrong version from being cached during xpcshell test startup.
// eslint-disable-next-line mozilla/use-services
XPCOMUtils.defineLazyGetter(this, "isContentProcess", () => {
  return Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;
});

XPCOMUtils.defineLazyGetter(this, "isContentScriptProcess", () => {
  return (
    isContentProcess ||
    !WebExtensionPolicy.useRemoteWebExtensions ||
    // Thunderbird still loads some content in the parent process.
    AppConstants.MOZ_APP_NAME == "thunderbird"
  );
});

var extensions = new DefaultWeakMap(policy => {
  return new ExtensionChild.BrowserExtensionContent(policy);
});

var pendingExtensions = new Map();

var ExtensionManager;

ExtensionManager = {
  // WeakMap<WebExtensionPolicy, Map<string, WebExtensionContentScript>>
  registeredContentScripts: new DefaultWeakMap(policy => new Map()),

  init() {
    Services.cpmm.addMessageListener("Extension:Startup", this);
    Services.cpmm.addMessageListener("Extension:Shutdown", this);
    Services.cpmm.addMessageListener("Extension:FlushJarCache", this);
    Services.cpmm.addMessageListener("Extension:RegisterContentScript", this);
    Services.cpmm.addMessageListener(
      "Extension:UnregisterContentScripts",
      this
    );

    this.updateStubExtensions();

    for (let id of sharedData.get("extensions/activeIDs") || []) {
      this.initExtension(getData({ id }));
    }
  },

  initStubPolicy(id, data) {
    let resolveReadyPromise;
    let readyPromise = new Promise(resolve => {
      resolveReadyPromise = resolve;
    });

    let policy = new WebExtensionPolicy({
      id,
      localizeCallback() {},
      readyPromise,
      allowedOrigins: new MatchPatternSet([]),
      ...data,
    });

    try {
      policy.active = true;

      pendingExtensions.set(id, { policy, resolveReadyPromise });
    } catch (e) {
      Cu.reportError(e);
    }
  },

  updateStubExtensions() {
    for (let [id, data] of sharedData.get("extensions/pending") || []) {
      if (!pendingExtensions.has(id)) {
        this.initStubPolicy(id, data);
      }
    }
  },

  initExtensionPolicy(extension) {
    let policy = WebExtensionPolicy.getByID(extension.id);
    if (!policy || pendingExtensions.has(extension.id)) {
      let localizeCallback;
      if (extension.localize) {
        // We have a real Extension object.
        localizeCallback = extension.localize.bind(extension);
      } else {
        // We have serialized extension data;
        localizeCallback = str => extensions.get(policy).localize(str);
      }

      let { backgroundScripts } = extension;
      if (!backgroundScripts && WebExtensionPolicy.isExtensionProcess) {
        ({ backgroundScripts } = getData(extension, "extendedData") || {});
      }

      let { backgroundWorkerScript } = extension;
      if (!backgroundWorkerScript && WebExtensionPolicy.isExtensionProcess) {
        ({ backgroundWorkerScript } = getData(extension, "extendedData") || {});
      }

      policy = new WebExtensionPolicy({
        id: extension.id,
        mozExtensionHostname: extension.uuid,
        name: extension.name,
        baseURL: extension.resourceURL,

        isPrivileged: extension.isPrivileged,
        temporarilyInstalled: extension.temporarilyInstalled,
        permissions: extension.permissions,
        allowedOrigins: extension.allowedOrigins,
        webAccessibleResources: extension.webAccessibleResources,

        manifestVersion: extension.manifestVersion,
        extensionPageCSP: extension.extensionPageCSP,

        localizeCallback,

        backgroundScripts,
        backgroundWorkerScript,

        contentScripts: extension.contentScripts,
      });

      policy.debugName = `${JSON.stringify(policy.name)} (ID: ${
        policy.id
      }, ${policy.getURL()})`;

      // Register any existent dynamically registered content script for the extension
      // when a content process is started for the first time (which also cover
      // a content process that crashed and it has been recreated).
      const registeredContentScripts = this.registeredContentScripts.get(
        policy
      );

      for (let [scriptId, options] of getData(extension, "contentScripts") ||
        []) {
        const script = new WebExtensionContentScript(policy, options);

        // If the script is a userScript, add the additional userScriptOptions
        // property to the WebExtensionContentScript instance.
        if ("userScriptOptions" in options) {
          script.userScriptOptions = options.userScriptOptions;
        }

        policy.registerContentScript(script);
        registeredContentScripts.set(scriptId, script);
      }

      let stub = pendingExtensions.get(extension.id);
      if (stub) {
        pendingExtensions.delete(extension.id);
        stub.policy.active = false;
        stub.resolveReadyPromise(policy);
      }

      policy.active = true;
      policy.instanceId = extension.instanceId;
      policy.optionalPermissions = extension.optionalPermissions;
    }
    return policy;
  },

  initExtension(data) {
    if (typeof data === "string") {
      data = getData({ id: data });
    }
    let policy = this.initExtensionPolicy(data);

    policy.injectContentScripts();
  },

  handleEvent(event) {
    if (
      event.type === "change" &&
      event.changedKeys.includes("extensions/pending")
    ) {
      this.updateStubExtensions();
    }
  },

  receiveMessage({ name, data }) {
    try {
      switch (name) {
        case "Extension:Startup":
          this.initExtension(data);
          break;

        case "Extension:Shutdown": {
          let policy = WebExtensionPolicy.getByID(data.id);
          if (policy) {
            if (extensions.has(policy)) {
              extensions.get(policy).shutdown();
            }

            if (isContentProcess) {
              policy.active = false;
            }
          }
          break;
        }

        case "Extension:FlushJarCache":
          ExtensionUtils.flushJarCache(data.path);
          break;

        case "Extension:RegisterContentScript": {
          let policy = WebExtensionPolicy.getByID(data.id);

          if (policy) {
            const registeredContentScripts = this.registeredContentScripts.get(
              policy
            );
            const type =
              "userScriptOptions" in data.options
                ? "userScript"
                : "contentScript";

            if (registeredContentScripts.has(data.scriptId)) {
              Cu.reportError(
                new Error(
                  `Registering ${type} ${data.scriptId} on ${data.id} more than once`
                )
              );
            } else {
              const script = new WebExtensionContentScript(
                policy,
                data.options
              );

              // If the script is a userScript, add the additional userScriptOptions
              // property to the WebExtensionContentScript instance.
              if (type === "userScript") {
                script.userScriptOptions = data.options.userScriptOptions;
              }

              policy.registerContentScript(script);
              registeredContentScripts.set(data.scriptId, script);
            }
          }
          break;
        }

        case "Extension:UnregisterContentScripts": {
          let policy = WebExtensionPolicy.getByID(data.id);

          if (policy) {
            const registeredContentScripts = this.registeredContentScripts.get(
              policy
            );

            for (const scriptId of data.scriptIds) {
              const script = registeredContentScripts.get(scriptId);
              if (script) {
                policy.unregisterContentScript(script);
                registeredContentScripts.delete(scriptId);
              }
            }
          }
          break;
        }
      }
    } catch (e) {
      Cu.reportError(e);
    }
    Services.cpmm.sendAsyncMessage(`${name}Complete`);
  },
};

var ExtensionProcessScript = {
  extensions,

  initExtension(extension) {
    return ExtensionManager.initExtensionPolicy(extension);
  },

  initExtensionDocument(policy, doc, privileged) {
    let extension = extensions.get(policy);
    if (privileged) {
      ExtensionPageChild.initExtensionContext(extension, doc.defaultView);
    } else {
      ExtensionContent.initExtensionContext(extension, doc.defaultView);
    }
  },

  getExtensionChild(id) {
    let policy = WebExtensionPolicy.getByID(id);
    if (policy) {
      return extensions.get(policy);
    }
  },

  preloadContentScript(contentScript) {
    if (isContentScriptProcess) {
      ExtensionContent.contentScripts.get(contentScript).preload();
    }
  },

  loadContentScript(contentScript, window) {
    return ExtensionContent.contentScripts
      .get(contentScript)
      .injectInto(window);
  },
};

var ExtensionAPIRequestHandler = {
  initExtensionWorker(policy, serviceWorkerInfo) {
    let extension = extensions.get(policy);

    if (!extension) {
      throw new Error(`Extension instance not found for addon ${policy.id}`);
    }

    ExtensionWorkerChild.initExtensionWorkerContext(
      extension,
      serviceWorkerInfo
    );
  },

  onExtensionWorkerLoaded(policy, serviceWorkerDescriptorId) {
    ExtensionWorkerChild.notifyExtensionWorkerContextLoaded(
      serviceWorkerDescriptorId,
      policy
    );
  },

  onExtensionWorkerDestroyed(policy, serviceWorkerDescriptorId) {
    ExtensionWorkerChild.destroyExtensionWorkerContext(
      serviceWorkerDescriptorId
    );
  },

  handleAPIRequest(policy, request) {
    try {
      let extension = extensions.get(policy);

      if (!extension) {
        throw new Error(`Extension instance not found for addon ${policy.id}`);
      }

      let context = this.getExtensionContextForAPIRequest({
        extension,
        request,
      });

      if (!context) {
        throw new Error(
          `Extension context not found for API request: ${request}`
        );
      }

      // Add a property to the request object for the normalizedArgs.
      request.normalizedArgs = this.validateAndNormalizeRequestArgs({
        context,
        request,
      });

      return context.childManager.handleWebIDLAPIRequest(request);
    } catch (error) {
      // Do not propagate errors that are not meant to be accessible to the
      // extension, report it to the console and just throw the generic
      // "An unexpected error occurred".
      Cu.reportError(error);
      return {
        type: Ci.mozIExtensionAPIRequestResult.EXTENSION_ERROR,
        value: new Error("An unexpected error occurred"),
      };
    }
  },

  getExtensionContextForAPIRequest({ extension, request }) {
    if (request.serviceWorkerInfo) {
      return ExtensionWorkerChild.getExtensionWorkerContext(
        extension,
        request.serviceWorkerInfo
      );
    }

    return null;
  },

  validateAndNormalizeRequestArgs({ context, request }) {
    if (!Schemas.checkPermissions(request.apiNamespace, context.extension)) {
      throw new context.Error(
        `Not enough privileges to access ${request.apiNamespace}`
      );
    }
    if (request.requestType === "getProperty") {
      return [];
    }

    if (request.apiObjectType) {
      // skip parameter validation on request targeting an api object,
      // even the JS-based implementation of the API objects are not
      // going through the same kind of Schema based validation that
      // the API namespaces methods and events go through.
      //
      // TODO(Bug 1728535): validate and normalize also this request arguments
      // as a low priority follow up.
      return request.args;
    }

    const { apiNamespace, apiName, args } = request;
    // Validate and normalize parameters, set the normalized args on the
    // mozIExtensionAPIRequest normalizedArgs property.
    return Schemas.checkParameters(context, apiNamespace, apiName, args);
  },
};

ExtensionManager.init();
