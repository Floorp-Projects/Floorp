/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Shared functions for tests related to invoking external handler applications.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "HandlerServiceTestUtils",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gExternalProtocolService",
                                   "@mozilla.org/uriloader/external-protocol-service;1",
                                   "nsIExternalProtocolService");
XPCOMUtils.defineLazyServiceGetter(this, "gMIMEService",
                                   "@mozilla.org/mime;1",
                                   "nsIMIMEService");

// For now, we need consumers to provide a reference to Assert.jsm.
var Assert = null;

this.HandlerServiceTestUtils = {
  set Assert(assert) {
    Assert = assert; // eslint-disable-line no-native-reassign
  },

  /**
   * This has to be initialized to the nsIHandlerService instance under testing.
   *
   * When support for migration from the RDF to the JSON back-end is removed,
   * this can be replaced by a lazy getter for the default implementation.
   */
  handlerService: null,

  /**
   * Retrieves the names of all the MIME types and protocols configured in the
   * handler service instance currently under testing.
   *
   * @return Array of strings like "example/type" or "example-scheme", sorted
   *         alphabetically regardless of category.
   */
  getAllHandlerInfoTypes() {
    let handlerInfoTypes = [];
    let handlerInfoEnumerator = this.handlerService.enumerate();
    while (handlerInfoEnumerator.hasMoreElements()) {
      let handlerInfo = handlerInfoEnumerator.getNext()
                                             .QueryInterface(Ci.nsIHandlerInfo);
      handlerInfoTypes.push(handlerInfo.type);
    }
    return handlerInfoTypes.sort();
  },

  /**
   * Retrieves all the configured handlers for MIME types and protocols.
   *
   * @note The nsIHandlerInfo instances returned by the "enumerate" method
   *       cannot be used for testing because they incorporate information from
   *       the operating system and also from the default nsIHandlerService
   *       instance, independently from what instance is under testing.
   *
   * @return Array of nsIHandlerInfo instances sorted by their "type" property.
   */
  getAllHandlerInfos() {
    return this.getAllHandlerInfoTypes().map(type => this.getHandlerInfo(type));
  },

  /**
   * Retrieves an nsIHandlerInfo for the given MIME type or protocol, which
   * incorporates information from the operating system and also from the
   * handler service instance currently under testing.
   *
   * @note If the handler service instance currently under testing is not the
   *       default one and the requested type is a MIME type, the returned
   *       nsIHandlerInfo will include information from the default
   *       nsIHandlerService instance. This cannot be avoided easily because the
   *       getMIMEInfoFromOS method is not exposed to JavaScript.
   *
   * @param type
   *        MIME type or scheme name of the nsIHandlerInfo to retrieve.
   *
   * @return The populated nsIHandlerInfo instance.
   */
  getHandlerInfo(type) {
    if (type.includes("/")) {
      // We have to use the getFromTypeAndExtension method because we don't have
      // access to getMIMEInfoFromOS. This means that we have to reset the data
      // that may have been imported from the default nsIHandlerService instance
      // and is not overwritten by fillHandlerInfo later.
      let handlerInfo = gMIMEService.getFromTypeAndExtension(type, null);
      handlerInfo.setFileExtensions("");
      // Populate the object from the handler service instance under testing.
      if (this.handlerService.exists(handlerInfo)) {
        this.handlerService.fillHandlerInfo(handlerInfo, "");
      }
      return handlerInfo;
    }

    // Populate the protocol information from the handler service instance under
    // testing, like the nsIExternalProtocolService::GetProtocolHandlerInfo
    // method does on the default nsIHandlerService instance.
    let osDefaultHandlerFound = {};
    let handlerInfo = gExternalProtocolService.getProtocolHandlerInfoFromOS(
                                                  type, osDefaultHandlerFound);
    if (this.handlerService.exists(handlerInfo)) {
      this.handlerService.fillHandlerInfo(handlerInfo, "");
    } else {
      gExternalProtocolService.setProtocolHandlerDefaults(
                                  handlerInfo, osDefaultHandlerFound.value);
    }
    return handlerInfo;
  },

  /**
   * Creates an nsIHandlerInfo for the given MIME type or protocol, initialized
   * to the default values for the current platform.
   *
   * @note For this method to work, the specified MIME type or protocol must not
   *       be configured in the default handler service instance or the one
   *       under testing, and must not be registered in the operating system.
   *
   * @param type
   *        MIME type or scheme name of the nsIHandlerInfo to create.
   *
   * @return The blank nsIHandlerInfo instance.
   */
  getBlankHandlerInfo(type) {
    let handlerInfo = this.getHandlerInfo(type);

    let preferredAction, preferredApplicationHandler;
    if (AppConstants.platform == "android") {
      // On Android, the default preferredAction for MIME types is useHelperApp.
      // For protocols, we always behave as if an operating system provided
      // handler existed, and as such we initialize them to useSystemDefault.
      // This is because the AndroidBridge is not available in xpcshell tests.
      preferredAction = type.includes("/")
                        ? Ci.nsIHandlerInfo.useHelperApp
                        : Ci.nsIHandlerInfo.useSystemDefault;
      // On Android, the default handler application is always the internal one.
      preferredApplicationHandler = {
        name: "Android chooser",
      };
    } else {
      // On Desktop, the default preferredAction for MIME types is saveToDisk,
      // while for protocols it is alwaysAsk.
      preferredAction = type.includes("/")
                        ? Ci.nsIHandlerInfo.saveToDisk
                        : Ci.nsIHandlerInfo.alwaysAsk;
      preferredApplicationHandler = null;
    }

    this.assertHandlerInfoMatches(handlerInfo, {
      type,
      preferredAction,
      alwaysAskBeforeHandling: true,
      preferredApplicationHandler,
    });
    return handlerInfo;
  },

  /**
   * Checks whether an nsIHandlerInfo instance matches the provided object.
   */
  assertHandlerInfoMatches(handlerInfo, expected) {
    let expectedInterface = expected.type.includes("/") ? Ci.nsIMIMEInfo
                                                        : Ci.nsIHandlerInfo;
    Assert.ok(handlerInfo instanceof expectedInterface);
    Assert.equal(handlerInfo.type, expected.type);
    Assert.equal(handlerInfo.preferredAction, expected.preferredAction);
    Assert.equal(handlerInfo.alwaysAskBeforeHandling,
                 expected.alwaysAskBeforeHandling);

    if (expectedInterface == Ci.nsIMIMEInfo) {
      let fileExtensionsEnumerator = handlerInfo.getFileExtensions();
      for (let expectedFileExtension of expected.fileExtensions || []) {
        Assert.equal(fileExtensionsEnumerator.getNext(), expectedFileExtension);
      }
      Assert.ok(!fileExtensionsEnumerator.hasMore());
    }

    if (expected.preferredApplicationHandler) {
      this.assertHandlerAppMatches(handlerInfo.preferredApplicationHandler,
                                   expected.preferredApplicationHandler);
    } else {
      Assert.equal(handlerInfo.preferredApplicationHandler, null);
    }

    let handlerAppsArrayEnumerator =
        handlerInfo.possibleApplicationHandlers.enumerate();
    if (AppConstants.platform == "android") {
      // On Android, the first handler application is always the internal one.
      this.assertHandlerAppMatches(handlerAppsArrayEnumerator.getNext(), {
        name: "Android chooser",
      });
    }
    for (let expectedHandlerApp of expected.possibleApplicationHandlers || []) {
      this.assertHandlerAppMatches(handlerAppsArrayEnumerator.getNext(),
                                   expectedHandlerApp);
    }
    Assert.ok(!handlerAppsArrayEnumerator.hasMoreElements());
  },

  /**
   * Checks whether an nsIHandlerApp instance matches the provided object.
   */
  assertHandlerAppMatches(handlerApp, expected) {
    Assert.ok(handlerApp instanceof Ci.nsIHandlerApp);
    Assert.equal(handlerApp.name, expected.name);
    if (expected.executable) {
      Assert.ok(handlerApp instanceof Ci.nsILocalHandlerApp);
      Assert.ok(expected.executable.equals(handlerApp.executable));
    } else if (expected.uriTemplate) {
      Assert.ok(handlerApp instanceof Ci.nsIWebHandlerApp);
      Assert.equal(handlerApp.uriTemplate, expected.uriTemplate);
    } else if (expected.service) {
      Assert.ok(handlerApp instanceof Ci.nsIDBusHandlerApp);
      Assert.equal(handlerApp.service, expected.service);
      Assert.equal(handlerApp.method, expected.method);
      Assert.equal(handlerApp.dBusInterface, expected.dBusInterface);
      Assert.equal(handlerApp.objectPath, expected.objectPath);
    }
  },
};
