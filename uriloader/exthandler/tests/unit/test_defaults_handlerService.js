/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gExternalProtocolService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);

const kDefaultHandlerList = Services.prefs
  .getChildList("gecko.handlerService.schemes")
  .filter(p => {
    try {
      let val = Services.prefs.getComplexValue(p, Ci.nsIPrefLocalizedString)
        .data;
      return !!val;
    } catch (ex) {
      return false;
    }
  });

add_task(async function test_check_defaults_get_added() {
  let protocols = new Set(
    kDefaultHandlerList.map(p => p.match(/schemes\.(\w+)/)[1])
  );
  for (let protocol of protocols) {
    const kPrefStr = `schemes.${protocol}.`;
    let matchingPrefs = kDefaultHandlerList.filter(p => p.includes(kPrefStr));
    let protocolHandlerCount = matchingPrefs.length / 2;
    Assert.ok(
      protocolHandlerCount,
      `Prefs for ${protocol} have at least 1 protocol handler`
    );
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
  let mailtoHandlerCount =
    kDefaultHandlerList.filter(p => p.includes("mailto")).length / 2;
  Assert.ok(mailtoHandlerCount, "Prefs have at least 1 mailto handler");
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

/**
 * Check that we don't add bogus handlers.
 */
add_task(async function test_check_restrictions() {
  const kTestData = {
    testdeleteme: [
      ["Delete me", ""],
      ["Delete me insecure", "http://example.com/%s"],
      ["Delete me no substitution", "https://example.com/"],
      ["Keep me", "https://example.com/%s"],
    ],
    testreallydeleteme: [
      // used to check we remove the entire entry.
      ["Delete me", "http://example.com/%s"],
    ],
  };
  for (let [scheme, handlers] of Object.entries(kTestData)) {
    let count = 1;
    for (let [name, uriTemplate] of handlers) {
      let pref = `gecko.handlerService.schemes.${scheme}.${count}.`;
      let obj = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(
        Ci.nsIPrefLocalizedString
      );
      obj.data = name;
      Services.prefs.setComplexValue(
        pref + "name",
        Ci.nsIPrefLocalizedString,
        obj
      );
      obj.data = uriTemplate;
      Services.prefs.setComplexValue(
        pref + "uriTemplate",
        Ci.nsIPrefLocalizedString,
        obj
      );
      count++;
    }
  }

  gHandlerService.wrappedJSObject._injectDefaultProtocolHandlers();
  let schemeData = gHandlerService.wrappedJSObject._store.data.schemes;

  Assert.ok(schemeData.testdeleteme, "Expect an entry for testdeleteme");
  Assert.ok(
    schemeData.testdeleteme.stubEntry,
    "Expect a stub entry for testdeleteme"
  );

  Assert.deepEqual(
    schemeData.testdeleteme.handlers,
    [null, { name: "Keep me", uriTemplate: "https://example.com/%s" }],
    "Expect only one handler is kept."
  );

  Assert.ok(!schemeData.testreallydeleteme, "No entry for reallydeleteme");
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
