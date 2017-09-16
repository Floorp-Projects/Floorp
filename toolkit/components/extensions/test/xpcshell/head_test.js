"use strict";

/* globals testEnv */
Assert.equal(WebExtensionPolicy.useRemoteWebExtensions, testEnv.expectRemote, "useRemoteWebExtensions matches");
Assert.equal(WebExtensionPolicy.isExtensionProcess, !testEnv.expectRemote, "testing from extension process");
