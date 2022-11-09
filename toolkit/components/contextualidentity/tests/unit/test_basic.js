"use strict";

const profileDir = do_get_profile();

const { ContextualIdentityService } = ChromeUtils.import(
  "resource://gre/modules/ContextualIdentityService.jsm"
);

const TEST_STORE_FILE_PATH = PathUtils.join(
  profileDir.path,
  "test-containers.json"
);

let cis;

// Basic tests
add_task(function() {
  ok(!!ContextualIdentityService, "ContextualIdentityService exists");

  cis = ContextualIdentityService.createNewInstanceForTesting(
    TEST_STORE_FILE_PATH
  );
  ok(!!cis, "We have our instance of ContextualIdentityService");

  equal(cis.getPublicIdentities().length, 4, "By default, 4 containers.");
  equal(cis.getPublicIdentityFromId(0), null, "No identity with id 0");

  ok(!!cis.getPublicIdentityFromId(1), "Identity 1 exists");
  ok(!!cis.getPublicIdentityFromId(2), "Identity 2 exists");
  ok(!!cis.getPublicIdentityFromId(3), "Identity 3 exists");
  ok(!!cis.getPublicIdentityFromId(4), "Identity 4 exists");
});

// Create a new identity
add_task(function() {
  equal(cis.getPublicIdentities().length, 4, "By default, 4 containers.");

  let identity = cis.create("New Container", "Icon", "Color");
  ok(!!identity, "New container created");
  equal(identity.name, "New Container", "Name matches");
  equal(identity.icon, "Icon", "Icon matches");
  equal(identity.color, "Color", "Color matches");

  equal(cis.getPublicIdentities().length, 5, "Expected 5 containers.");

  ok(!!cis.getPublicIdentityFromId(identity.userContextId), "Identity exists");
  equal(
    cis.getPublicIdentityFromId(identity.userContextId).name,
    "New Container",
    "Identity name is OK"
  );
  equal(
    cis.getPublicIdentityFromId(identity.userContextId).icon,
    "Icon",
    "Identity icon is OK"
  );
  equal(
    cis.getPublicIdentityFromId(identity.userContextId).color,
    "Color",
    "Identity color is OK"
  );
  equal(
    cis.getUserContextLabel(identity.userContextId),
    "New Container",
    "Identity label is OK"
  );

  // Remove an identity
  equal(
    cis.remove(-1),
    false,
    "cis.remove() returns false if identity doesn't exist."
  );
  equal(cis.remove(1), true, "cis.remove() returns true if identity exists.");

  equal(cis.getPublicIdentities().length, 4, "Expected 4 containers.");
});

// Update an identity
add_task(function() {
  ok(!!cis.getPublicIdentityFromId(2), "Identity 2 exists");

  equal(
    cis.update(-1, "Container", "Icon", "Color"),
    false,
    "Update returns false if the identity doesn't exist"
  );

  equal(
    cis.update(2, "Container", "Icon", "Color"),
    true,
    "Update returns true if everything is OK"
  );

  ok(!!cis.getPublicIdentityFromId(2), "Identity exists");
  equal(
    cis.getPublicIdentityFromId(2).name,
    "Container",
    "Identity name is OK"
  );
  equal(cis.getPublicIdentityFromId(2).icon, "Icon", "Identity icon is OK");
  equal(cis.getPublicIdentityFromId(2).color, "Color", "Identity color is OK");
  equal(cis.getUserContextLabel(2), "Container", "Identity label is OK");
});
