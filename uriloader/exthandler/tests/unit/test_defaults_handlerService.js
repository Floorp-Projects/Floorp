/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gExternalProtocolService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);
ChromeUtils.defineESModuleGetters(this, {
  kHandlerList: "resource://gre/modules/handlers/HandlerList.sys.mjs",
});

add_task(async function test_check_defaults_get_added() {
  let protocols = Object.keys(kHandlerList.default.schemes);
  for (let protocol of protocols) {
    let protocolHandlerCount =
      kHandlerList.default.schemes[protocol].handlers.length;
    Assert.ok(
      gHandlerService.wrappedJSObject._store.data.schemes[protocol].stubEntry,
      `Expect stub for ${protocol}`
    );
    let info = gExternalProtocolService.getProtocolHandlerInfo(protocol, {});
    Assert.ok(
      info,
      `Should be able to get protocol handler info for ${protocol}`
    );
    let handlers = Array.from(
      info.possibleApplicationHandlers.enumerate(Ci.nsIHandlerApp)
    );
    handlers = handlers.filter(h => h instanceof Ci.nsIWebHandlerApp);
    Assert.equal(
      handlers.length,
      protocolHandlerCount,
      `Default web handlers for ${protocol} should match`
    );
    let { alwaysAskBeforeHandling, preferredAction } = info;
    // Actually store something, pretending there was a change:
    let infoToWrite = gExternalProtocolService.getProtocolHandlerInfo(
      protocol,
      {}
    );
    gHandlerService.store(infoToWrite);
    ok(
      !gHandlerService.wrappedJSObject._store.data.schemes[protocol].stubEntry,
      "Expect stub entry info to go away"
    );

    let newInfo = gExternalProtocolService.getProtocolHandlerInfo(protocol, {});
    Assert.equal(
      alwaysAskBeforeHandling,
      newInfo.alwaysAskBeforeHandling,
      protocol + " - always ask shouldn't change"
    );
    Assert.equal(
      preferredAction,
      newInfo.preferredAction,
      protocol + " - preferred action shouldn't change"
    );
    await deleteHandlerStore();
  }
});

add_task(async function test_check_default_modification() {
  Assert.ok(
    true,
    JSON.stringify(gHandlerService.wrappedJSObject._store.data.schemes.mailto)
  );
  Assert.ok(
    gHandlerService.wrappedJSObject._store.data.schemes.mailto.stubEntry,
    "Expect stub for mailto"
  );
  let mailInfo = gExternalProtocolService.getProtocolHandlerInfo("mailto", {});
  mailInfo.alwaysAskBeforeHandling = false;
  mailInfo.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
  gHandlerService.store(mailInfo);
  Assert.ok(
    !gHandlerService.wrappedJSObject._store.data.schemes.mailto.stubEntry,
    "Stub entry should be removed immediately."
  );
  let newMail = gExternalProtocolService.getProtocolHandlerInfo("mailto", {});
  Assert.equal(newMail.preferredAction, Ci.nsIHandlerInfo.useSystemDefault);
  Assert.equal(newMail.alwaysAskBeforeHandling, false);
  await deleteHandlerStore();
});

add_task(async function test_migrations() {
  const kTestData = [
    ["A", "http://compose.mail.yahoo.co.jp/ym/Compose?To=%s"],
    ["B", "http://www.inbox.lv/rfc2368/?value=%s"],
    ["C", "http://poczta.interia.pl/mh/?mailto=%s"],
    ["D", "http://win.mail.ru/cgi-bin/sentmsg?mailto=%s"],
  ];
  // Set up the test handlers. This doesn't use prefs like the previous test,
  // because we now refuse to import insecure handler prefs. They can only
  // exist if they were added into the handler store before this restriction
  // was created (bug 1526890).
  gHandlerService.wrappedJSObject._injectDefaultProtocolHandlers();
  let handler = gExternalProtocolService.getProtocolHandlerInfo("mailto");
  while (handler.possibleApplicationHandlers.length) {
    handler.possibleApplicationHandlers.removeElementAt(0);
  }
  for (let [name, uriTemplate] of kTestData) {
    let app = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
      Ci.nsIWebHandlerApp
    );
    app.uriTemplate = uriTemplate;
    app.name = name;
    handler.possibleApplicationHandlers.appendElement(app);
  }
  gHandlerService.store(handler);

  // Now migrate them:
  Services.prefs.setCharPref("browser.handlers.migrations", "blah,secure-mail");
  gHandlerService.wrappedJSObject._migrateProtocolHandlersIfNeeded();

  // Now check the result:
  handler = gExternalProtocolService.getProtocolHandlerInfo("mailto");

  let expectedURIs = new Set([
    "https://mail.yahoo.co.jp/compose/?To=%s",
    "https://mail.inbox.lv/compose?to=%s",
    "https://poczta.interia.pl/mh/?mailto=%s",
    "https://e.mail.ru/cgi-bin/sentmsg?mailto=%s",
  ]);

  let possibleApplicationHandlers = Array.from(
    handler.possibleApplicationHandlers.enumerate(Ci.nsIWebHandlerApp)
  );
  // Set iterators are stable to deletion, so this works:
  for (let expected of expectedURIs) {
    for (let app of possibleApplicationHandlers) {
      if (app instanceof Ci.nsIWebHandlerApp && app.uriTemplate == expected) {
        Assert.ok(true, "Found handler with URI " + expected);
        // ... even when we remove items.
        expectedURIs.delete(expected);
        break;
      }
    }
  }
  Assert.equal(expectedURIs.size, 0, "Should have seen all the expected URIs.");

  for (let app of possibleApplicationHandlers) {
    if (app instanceof Ci.nsIWebHandlerApp) {
      Assert.ok(
        !kTestData.some(n => n[1] == app.uriTemplate),
        "Should not be any of the original handlers"
      );
    }
  }

  Assert.ok(
    !handler.preferredApplicationHandler,
    "Shouldn't have preferred handler initially."
  );
  await deleteHandlerStore();
});

/**
 * Check that non-https templates or ones without a '%s' are ignored.
 */
add_task(async function invalid_handlers_are_rejected() {
  let schemes = kHandlerList.default.schemes;
  schemes.myfancyinvalidstuff = {
    handlers: [
      {
        name: "No template at all",
      },
      {
        name: "Not secure",
        uriTemplate: "http://example.com/%s",
      },
      {
        name: "No replacement percent-s bit",
        uriTemplate: "https://example.com/",
      },
      {
        name: "Actually valid",
        uriTemplate: "https://example.com/%s",
      },
    ],
  };
  gHandlerService.wrappedJSObject._injectDefaultProtocolHandlers();
  // Now check the result:
  let handler = gExternalProtocolService.getProtocolHandlerInfo(
    "myfancyinvalidstuff"
  );

  let expectedURIs = ["https://example.com/%s"];

  Assert.deepEqual(
    Array.from(
      handler.possibleApplicationHandlers.enumerate(Ci.nsIWebHandlerApp),
      e => e.uriTemplate
    ),
    expectedURIs,
    "Should have seen only 1 handler added."
  );
  await deleteHandlerStore();
});
