/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests retrieving remote LoginRecipes in the parent process.
 * See https://firefox-source-docs.mozilla.org/services/settings/#unit-tests for explanation of db.importChanges({}, Date.now());
 */

"use strict";

const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const REMOTE_SETTINGS_COLLECTION = "password-recipes";

add_task(async function test_init_remote_recipe() {
  const db = RemoteSettings(REMOTE_SETTINGS_COLLECTION).db;
  await db.clear();
  const record1 = {
    id: "some-fake-ID",
    hosts: ["www.testDomain.com"],
    description: "Some description here",
    usernameSelector: "#username",
  };
  await db.importChanges({}, Date.now(), [record1], { clear: true });
  let parent = new LoginRecipesParent({ defaults: true });

  let recipesParent = await parent.initializationPromise;
  Assert.ok(
    recipesParent instanceof LoginRecipesParent,
    "Check initialization promise value which should be an instance of LoginRecipesParent"
  );
  Assert.strictEqual(
    recipesParent._recipesByHost.size,
    1,
    "Initially 1 recipe based on our test record"
  );
  let rsClient = recipesParent._rsClient;

  recipesParent.reset();
  await recipesParent.initializationPromise;
  Assert.ok(
    recipesParent instanceof LoginRecipesParent,
    "Ensure that the instance of LoginRecipesParent has not changed after resetting"
  );
  Assert.strictEqual(
    rsClient,
    recipesParent._rsClient,
    "Resetting recipes should not modify the rs client"
  );
  Assert.strictEqual(
    recipesParent._recipesByHost.size,
    1,
    "Initially 1 recipe based on our test record"
  );
  await db.clear();
  await db.importChanges({}, 42);
});

add_task(async function test_add_recipe_sync() {
  const db = RemoteSettings(REMOTE_SETTINGS_COLLECTION).db;
  const record1 = {
    id: "some-fake-ID",
    hosts: ["www.testDomain.com"],
    description: "Some description here",
    usernameSelector: "#username",
  };
  await db.importChanges({}, Date.now(), [record1], { clear: true });
  let parent = new LoginRecipesParent({ defaults: true });
  let recipesParent = await parent.initializationPromise;

  const record2 = {
    id: "some-fake-ID-2",
    hosts: ["www.testDomain2.com"],
    description: "Some description here. Wow it changed!",
    usernameSelector: "#username",
  };
  const payload = {
    current: [record1, record2],
    created: [record2],
    updated: [],
    deleted: [],
  };
  await RemoteSettings(REMOTE_SETTINGS_COLLECTION).emit("sync", {
    data: payload,
  });
  Assert.strictEqual(
    recipesParent._recipesByHost.size,
    2,
    "New recipe from sync event added successfully"
  );
  await db.clear();
  await db.importChanges({}, 42);
});

add_task(async function test_remove_recipe_sync() {
  const db = RemoteSettings(REMOTE_SETTINGS_COLLECTION).db;
  const record1 = {
    id: "some-fake-ID",
    hosts: ["www.testDomain.com"],
    description: "Some description here",
    usernameSelector: "#username",
  };
  await db.importChanges({}, Date.now(), [record1], { clear: true });
  let parent = new LoginRecipesParent({ defaults: true });
  let recipesParent = await parent.initializationPromise;

  const deletePayload = {
    current: [],
    created: [],
    updated: [],
    deleted: [record1],
  };
  await RemoteSettings(REMOTE_SETTINGS_COLLECTION).emit("sync", {
    data: deletePayload,
  });
  Assert.strictEqual(
    recipesParent._recipesByHost.size,
    0,
    "Recipes successfully deleted on sync event"
  );
  await db.clear();
});

add_task(async function test_malformed_recipes_in_db() {
  const db = RemoteSettings(REMOTE_SETTINGS_COLLECTION).db;
  const malformedRecord = {
    id: "some-ID",
    hosts: ["www.testDomain.com"],
    description: "Some description here",
    usernameSelector: "#username",
    fieldThatDoesNotExist: "value",
  };
  await db.importChanges({}, Date.now(), [malformedRecord], { clear: true });
  let parent = new LoginRecipesParent({ defaults: true });
  try {
    await parent.initializationPromise;
  } catch (e) {
    Assert.ok(
      e == "There were 1 recipe error(s)",
      "It should throw an error because of field that does not match the schema"
    );
  }

  await db.clear();
  const missingHostsRecord = {
    id: "some-ID",
    description: "Some description here",
    usernameSelector: "#username",
  };
  await db.importChanges({}, Date.now(), [missingHostsRecord], { clear: true });
  parent = new LoginRecipesParent({ defaults: true });
  try {
    await parent.initializationPromise;
  } catch (e) {
    Assert.ok(
      e == "There were 1 recipe error(s)",
      "It should throw an error because of missing hosts field"
    );
  }
});
