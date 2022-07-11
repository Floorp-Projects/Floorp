"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const UTILS_MODULE = "resource://services-settings/Utils.jsm";

const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

function clear_state() {
  env.set("MOZ_REMOTE_SETTINGS_DEVTOOLS", "0");
  Services.prefs.clearUserPref("services.settings.server");
  Services.prefs.clearUserPref("services.settings.preview_enabled");
  Cu.unload(UTILS_MODULE);
  Assert.ok(!Cu.isModuleLoaded(UTILS_MODULE), "Utils was unloaded before test");
}

add_setup(async function() {
  // Set this env vars in order to test the code path where the
  // server URL can only be overridden from Dev Tools.
  // See `isRunningTests` in `services/settings/Utils.jsm`.
  const before = env.get("MOZ_DISABLE_NONLOCAL_CONNECTIONS");
  env.set("MOZ_DISABLE_NONLOCAL_CONNECTIONS", "0");

  registerCleanupFunction(() => {
    clear_state();
    env.set("MOZ_DISABLE_NONLOCAL_CONNECTIONS", before);
  });
});

add_task(clear_state);

add_task(
  {
    skip_if: () => !AppConstants.RELEASE_OR_BETA,
  },
  async function test_server_url_cannot_be_toggled_in_release() {
    Services.prefs.setCharPref(
      "services.settings.server",
      "http://localhost:8888/v1"
    );

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.equal(
      Utils.SERVER_URL,
      AppConstants.REMOTE_SETTINGS_SERVER_URL,
      "Server url pref was not read in release"
    );
  }
);

add_task(
  {
    skip_if: () => AppConstants.RELEASE_OR_BETA,
  },
  async function test_server_url_cannot_be_toggled_in_dev_nightly() {
    Services.prefs.setCharPref(
      "services.settings.server",
      "http://localhost:8888/v1"
    );

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.notEqual(
      Utils.SERVER_URL,
      AppConstants.REMOTE_SETTINGS_SERVER_URL,
      "Server url pref was read in nightly/dev"
    );
  }
);
add_task(clear_state);

add_task(
  {
    skip_if: () => !AppConstants.RELEASE_OR_BETA,
  },
  async function test_preview_mode_cannot_be_toggled_in_release() {
    Services.prefs.setBoolPref("services.settings.preview_enabled", true);

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.ok(!Utils.PREVIEW_MODE, "Preview mode pref was not read in release");
  }
);
add_task(clear_state);

add_task(
  {
    skip_if: () => AppConstants.RELEASE_OR_BETA,
  },
  async function test_preview_mode_cannot_be_toggled_in_dev_nightly() {
    Services.prefs.setBoolPref("services.settings.preview_enabled", true);

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.ok(Utils.PREVIEW_MODE, "Preview mode pref is read in dev/nightly");
  }
);
add_task(clear_state);

add_task(
  {
    skip_if: () => !AppConstants.RELEASE_OR_BETA,
  },
  async function test_load_dumps_will_always_be_loaded_in_release() {
    Services.prefs.setCharPref(
      "services.settings.server",
      "http://localhost:8888/v1"
    );

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.equal(
      Utils.SERVER_URL,
      AppConstants.REMOTE_SETTINGS_SERVER_URL,
      "Server url pref was not read"
    );
    Assert.ok(Utils.LOAD_DUMPS, "Dumps will always be loaded");
  }
);

add_task(
  {
    skip_if: () => AppConstants.RELEASE_OR_BETA,
  },
  async function test_load_dumps_can_be_disabled_in_dev_nightly() {
    Services.prefs.setCharPref(
      "services.settings.server",
      "http://localhost:8888/v1"
    );

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.notEqual(
      Utils.SERVER_URL,
      AppConstants.REMOTE_SETTINGS_SERVER_URL,
      "Server url pref was read"
    );
    Assert.ok(!Utils.LOAD_DUMPS, "Dumps are not loaded if server is not prod");
  }
);
add_task(clear_state);

add_task(
  async function test_server_url_can_be_changed_in_all_versions_if_running_for_devtools() {
    env.set("MOZ_REMOTE_SETTINGS_DEVTOOLS", "1");
    Services.prefs.setCharPref(
      "services.settings.server",
      "http://localhost:8888/v1"
    );

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.notEqual(
      Utils.SERVER_URL,
      AppConstants.REMOTE_SETTINGS_SERVER_URL,
      "Server url pref was read"
    );
  }
);
add_task(clear_state);

add_task(
  async function test_preview_mode_can_be_changed_in_all_versions_if_running_for_devtools() {
    env.set("MOZ_REMOTE_SETTINGS_DEVTOOLS", "1");
    Services.prefs.setBoolPref("services.settings.preview_enabled", true);

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.ok(Utils.PREVIEW_MODE, "Preview mode pref was read");
  }
);
add_task(clear_state);

add_task(
  async function test_dumps_are_not_loaded_if_server_is_not_prod_if_running_for_devtools() {
    env.set("MOZ_REMOTE_SETTINGS_DEVTOOLS", "1");
    Services.prefs.setCharPref(
      "services.settings.server",
      "http://localhost:8888/v1"
    );

    const { Utils } = ChromeUtils.import(UTILS_MODULE);
    Assert.ok(!Utils.LOAD_DUMPS, "Dumps won't be loaded");
  }
);
add_task(clear_state);

add_task(
  async function test_dumps_are_loaded_if_server_is_prod_if_running_for_devtools() {
    env.set("MOZ_REMOTE_SETTINGS_DEVTOOLS", "1");
    Services.prefs.setCharPref(
      "services.settings.server",
      AppConstants.REMOTE_SETTINGS_SERVER_URL
    );

    const { Utils } = ChromeUtils.import(UTILS_MODULE);

    Assert.ok(Utils.LOAD_DUMPS, "dumps are loaded if prod");
  }
);
add_task(clear_state);
