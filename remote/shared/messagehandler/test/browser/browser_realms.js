/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { RootMessageHandler } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/messagehandler/RootMessageHandler.sys.mjs"
);

add_task(async function test_tab_is_removed() {
  const tab = await addTab("https://example.com/document-builder.sjs?html=tab");
  const sessionId = "realms";
  const browsingContext = tab.linkedBrowser.browsingContext;
  const contextDescriptor = {
    type: ContextDescriptorType.TopBrowsingContext,
    id: browsingContext.browserId,
  };

  const rootMessageHandler = createRootMessageHandler(sessionId);

  const onRealmCreated = rootMessageHandler.once("realm-created");

  // Add a new session data item to get window global handlers created
  await rootMessageHandler.addSessionDataItem({
    moduleName: "command",
    category: "browser_realms",
    contextDescriptor,
    values: [true],
  });

  const realmCreatedEvent = await onRealmCreated;
  const createdRealmId = realmCreatedEvent.realmInfo.realm;

  is(rootMessageHandler.realms.size, 1, "Realm is added in the internal map");

  const onRealmDestroyed = rootMessageHandler.once("realm-destroyed");

  gBrowser.removeTab(tab);

  const realmDestroyedEvent = await onRealmDestroyed;

  is(
    realmDestroyedEvent.realm,
    createdRealmId,
    "Received a correct realm id in realm-destroyed event"
  );
  is(rootMessageHandler.realms.size, 0, "The realm map is cleaned up");

  rootMessageHandler.destroy();
});

add_task(async function test_same_origin_navigation() {
  const tab = await addTab("https://example.com/document-builder.sjs?html=tab");
  const sessionId = "realms";
  const browsingContext = tab.linkedBrowser.browsingContext;
  const contextDescriptor = {
    type: ContextDescriptorType.TopBrowsingContext,
    id: browsingContext.browserId,
  };

  const rootMessageHandler = createRootMessageHandler(sessionId);

  const onRealmCreated = rootMessageHandler.once("realm-created");

  // Add a new session data item to get window global handlers created
  await rootMessageHandler.addSessionDataItem({
    moduleName: "command",
    category: "browser_realms",
    contextDescriptor,
    values: [true],
  });

  const realmCreatedEvent = await onRealmCreated;
  const createdRealmId = realmCreatedEvent.realmInfo.realm;

  is(rootMessageHandler.realms.size, 1, "Realm is added in the internal map");

  const onRealmDestroyed = rootMessageHandler.once("realm-destroyed");
  const onNewRealmCreated = rootMessageHandler.once("realm-created");

  // Navigate to another page with the same origin
  await loadURL(
    tab.linkedBrowser,
    "https://example.com/document-builder.sjs?html=othertab"
  );

  const realmDestroyedEvent = await onRealmDestroyed;

  is(
    realmDestroyedEvent.realm,
    createdRealmId,
    "Received a correct realm id in realm-destroyed event"
  );

  await onNewRealmCreated;

  is(rootMessageHandler.realms.size, 1, "Realm is added in the internal map");

  gBrowser.removeTab(tab);
  rootMessageHandler.destroy();
});

add_task(async function test_cross_origin_navigation() {
  const tab = await addTab("https://example.com/document-builder.sjs?html=tab");
  const sessionId = "realms";
  const browsingContext = tab.linkedBrowser.browsingContext;
  const contextDescriptor = {
    type: ContextDescriptorType.TopBrowsingContext,
    id: browsingContext.browserId,
  };

  const rootMessageHandler = createRootMessageHandler(sessionId);

  const onRealmCreated = rootMessageHandler.once("realm-created");

  // Add a new session data item to get window global handlers created
  await rootMessageHandler.addSessionDataItem({
    moduleName: "command",
    category: "browser_realms",
    contextDescriptor,
    values: [true],
  });

  const realmCreatedEvent = await onRealmCreated;
  const createdRealmId = realmCreatedEvent.realmInfo.realm;

  is(rootMessageHandler.realms.size, 1, "Realm is added in the internal map");

  const onRealmDestroyed = rootMessageHandler.once("realm-destroyed");
  const onNewRealmCreated = rootMessageHandler.once("realm-created");

  // Navigate to another page with the different origin
  await loadURL(
    tab.linkedBrowser,
    "https://example.com/document-builder.sjs?html=otherorigin"
  );

  const realmDestroyedEvent = await onRealmDestroyed;

  is(
    realmDestroyedEvent.realm,
    createdRealmId,
    "Received a correct realm id in realm-destroyed event"
  );

  await onNewRealmCreated;

  is(rootMessageHandler.realms.size, 1, "Realm is added in the internal map");

  gBrowser.removeTab(tab);
  rootMessageHandler.destroy();
});
