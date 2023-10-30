/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Realm, WindowRealm } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/Realm.sys.mjs"
);

add_task(function test_id() {
  const realm1 = new Realm();
  const id1 = realm1.id;
  Assert.equal(typeof id1, "string");

  const realm2 = new Realm();
  const id2 = realm2.id;
  Assert.equal(typeof id2, "string");

  Assert.notEqual(id1, id2, "Ids for different realms are different");
});

add_task(function test_handleObjectMap() {
  const realm = new Realm();

  // Test an unknown handle.
  Assert.equal(
    realm.getObjectForHandle("unknown"),
    undefined,
    "Unknown handles return undefined"
  );

  // Test creating a simple handle.
  const object = {};
  const handle = realm.getHandleForObject(object);
  Assert.equal(typeof handle, "string", "Created a valid handle");
  Assert.equal(
    realm.getObjectForHandle(handle),
    object,
    "Using the handle returned the original object"
  );

  // Test another handle for the same object.
  const secondHandle = realm.getHandleForObject(object);
  Assert.equal(typeof secondHandle, "string", "Created a valid handle");
  Assert.notEqual(secondHandle, handle, "A different handle was generated");
  Assert.equal(
    realm.getObjectForHandle(secondHandle),
    object,
    "Using the second handle also returned the original object"
  );

  // Test using the handles in another realm.
  const otherRealm = new Realm();
  Assert.equal(
    otherRealm.getObjectForHandle(handle),
    undefined,
    "A realm returns undefined for handles from another realm"
  );

  // Removing an unknown handle should not throw or have any side effect on
  // existing handles.
  realm.removeObjectHandle("unknown");
  Assert.equal(realm.getObjectForHandle(handle), object);
  Assert.equal(realm.getObjectForHandle(secondHandle), object);

  // Remove the second handle
  realm.removeObjectHandle(secondHandle);
  Assert.equal(
    realm.getObjectForHandle(handle),
    object,
    "The first handle is still resolving the object"
  );
  Assert.equal(
    realm.getObjectForHandle(secondHandle),
    undefined,
    "The second handle returns undefined after calling removeObjectHandle"
  );

  // Remove the original handle
  realm.removeObjectHandle(handle);
  Assert.equal(
    realm.getObjectForHandle(handle),
    undefined,
    "The first handle returns undefined as well"
  );
});

add_task(async function test_windowRealm_isSandbox() {
  const windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  const contentWindow = windowlessBrowser.docShell.domWindow;

  const realm1 = new WindowRealm(contentWindow);
  Assert.equal(realm1.isSandbox, false);

  const realm2 = new WindowRealm(contentWindow, { sandboxName: "test" });
  Assert.equal(realm2.isSandbox, true);
});

add_task(async function test_windowRealm_userActivationEnabled() {
  const windowlessBrowser = Services.appShell.createWindowlessBrowser(false);
  const contentWindow = windowlessBrowser.docShell.domWindow;
  const userActivation = contentWindow.navigator.userActivation;

  const realm = new WindowRealm(contentWindow);

  Assert.equal(realm.userActivationEnabled, false);
  Assert.equal(userActivation.isActive && userActivation.hasBeenActive, false);

  realm.userActivationEnabled = true;
  Assert.equal(realm.userActivationEnabled, true);
  Assert.equal(userActivation.isActive && userActivation.hasBeenActive, true);

  realm.userActivationEnabled = false;
  Assert.equal(realm.userActivationEnabled, false);
  Assert.equal(userActivation.isActive && userActivation.hasBeenActive, false);
});
