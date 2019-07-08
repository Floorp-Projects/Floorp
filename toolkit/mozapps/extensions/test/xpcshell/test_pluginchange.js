/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const LIST_UPDATED_TOPIC = "plugins-list-updated";

class PluginTag extends MockPluginTag {
  constructor(name, description) {
    super({ name, description, version: "1.0" });
    this.description = description;
  }
}

const PLUGINS = [
  // A standalone plugin
  new PluginTag("Java", "A mock Java plugin"),

  // A plugin made up of two plugin files
  new PluginTag("Flash", "A mock Flash plugin"),
  new PluginTag("Flash", "A mock Flash plugin"),
];

mockPluginHost(PLUGINS);

// This verifies that when the list of plugins changes the add-ons manager
// correctly updates
async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref("media.gmp-provider.enabled", false);

  await promiseStartupManager();

  run_test_1();
}

function end_test() {
  executeSoon(do_test_finished);
}

function sortAddons(addons) {
  addons.sort(function(a, b) {
    return a.name.localeCompare(b.name);
  });
}

// Basic check that the mock object works
async function run_test_1() {
  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  sortAddons(addons);

  Assert.equal(addons.length, 2);

  Assert.equal(addons[0].name, "Flash");
  Assert.ok(!addons[0].userDisabled);
  Assert.equal(addons[1].name, "Java");
  Assert.ok(!addons[1].userDisabled);

  run_test_2();
}

// No change to the list should not trigger any events or changes in the API
async function run_test_2() {
  // Reorder the list a bit
  let tag = PLUGINS[0];
  PLUGINS[0] = PLUGINS[2];
  PLUGINS[2] = PLUGINS[1];
  PLUGINS[1] = tag;

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC);

  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  sortAddons(addons);

  Assert.equal(addons.length, 2);

  Assert.equal(addons[0].name, "Flash");
  Assert.ok(!addons[0].userDisabled);
  Assert.equal(addons[1].name, "Java");
  Assert.ok(!addons[1].userDisabled);

  run_test_3();
}

// Tests that a newly detected plugin shows up in the API and sends out events
async function run_test_3() {
  let tag = new PluginTag("Quicktime", "A mock Quicktime plugin");
  PLUGINS.push(tag);
  let id = tag.name + tag.description;

  await expectEvents(
    {
      addonEvents: {
        [id]: [{ event: "onInstalling" }, { event: "onInstalled" }],
      },
      installEvents: [{ event: "onExternalInstall" }],
    },
    async () => {
      Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC);
    }
  );

  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  sortAddons(addons);

  Assert.equal(addons.length, 3);

  Assert.equal(addons[0].name, "Flash");
  Assert.ok(!addons[0].userDisabled);
  Assert.equal(addons[1].name, "Java");
  Assert.ok(!addons[1].userDisabled);
  Assert.equal(addons[2].name, "Quicktime");
  Assert.ok(!addons[2].userDisabled);

  run_test_4();
}

// Tests that a removed plugin disappears from in the API and sends out events
async function run_test_4() {
  let tag = PLUGINS.splice(1, 1)[0];
  let id = tag.name + tag.description;

  await expectEvents(
    {
      addonEvents: {
        [id]: [{ event: "onUninstalling" }, { event: "onUninstalled" }],
      },
    },
    async () => {
      Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC);
    }
  );

  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  sortAddons(addons);

  Assert.equal(addons.length, 2);

  Assert.equal(addons[0].name, "Flash");
  Assert.ok(!addons[0].userDisabled);
  Assert.equal(addons[1].name, "Quicktime");
  Assert.ok(!addons[1].userDisabled);

  run_test_5();
}

// Removing part of the flash plugin should have no effect
async function run_test_5() {
  PLUGINS.splice(0, 1);

  Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC);

  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  sortAddons(addons);

  Assert.equal(addons.length, 2);

  Assert.equal(addons[0].name, "Flash");
  Assert.ok(!addons[0].userDisabled);
  Assert.equal(addons[1].name, "Quicktime");
  Assert.ok(!addons[1].userDisabled);

  run_test_6();
}

// Replacing flash should be detected
async function run_test_6() {
  let oldTag = PLUGINS.splice(0, 1)[0];
  let newTag = new PluginTag("Flash 2", "A new crash-free Flash!");
  newTag.disabled = true;
  PLUGINS.push(newTag);

  await expectEvents(
    {
      addonEvents: {
        [oldTag.name + oldTag.description]: [
          { event: "onUninstalling" },
          { event: "onUninstalled" },
        ],
        [newTag.name + newTag.description]: [
          { event: "onInstalling" },
          { event: "onInstalled" },
        ],
      },
      installEvents: [{ event: "onExternalInstall" }],
    },
    async () => {
      Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC);
    }
  );

  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  sortAddons(addons);

  Assert.equal(addons.length, 2);

  Assert.equal(addons[0].name, "Flash 2");
  Assert.ok(addons[0].userDisabled);
  Assert.equal(addons[1].name, "Quicktime");
  Assert.ok(!addons[1].userDisabled);

  run_test_7();
}

// If new tags are detected and the disabled state changes then we should send
// out appropriate notifications
async function run_test_7() {
  PLUGINS[0] = new PluginTag("Quicktime", "A mock Quicktime plugin");
  PLUGINS[0].disabled = true;
  PLUGINS[1] = new PluginTag("Flash 2", "A new crash-free Flash!");

  await expectEvents(
    {
      addonEvents: {
        [PLUGINS[0].name + PLUGINS[0].description]: [
          { event: "onDisabling" },
          { event: "onDisabled" },
        ],
        [PLUGINS[1].name + PLUGINS[1].description]: [
          { event: "onEnabling" },
          { event: "onEnabled" },
        ],
      },
    },
    async () => {
      Services.obs.notifyObservers(null, LIST_UPDATED_TOPIC);
    }
  );

  let addons = await AddonManager.getAddonsByTypes(["plugin"]);
  sortAddons(addons);

  Assert.equal(addons.length, 2);

  Assert.equal(addons[0].name, "Flash 2");
  Assert.ok(!addons[0].userDisabled);
  Assert.equal(addons[1].name, "Quicktime");
  Assert.ok(addons[1].userDisabled);

  end_test();
}
