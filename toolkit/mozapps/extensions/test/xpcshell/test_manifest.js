/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests that all properties are read from the install manifests and that
// items are correctly enabled/disabled based on them (blocklist tests are
// elsewhere)

const ADDONS = [
  {
    "install.rdf": {
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      aboutURL: "chrome://test/content/about.xul",
      iconURL: "chrome://test/skin/icon.png",
      icon64URL: "chrome://test/skin/icon64.png",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 1",
      description: "Test Description",
      creator: "Test Creator",
      homepageURL: "http://www.example.com",
      developer: [
        "Test Developer 1",
        "Test Developer 2"
      ],
      translator: [
        "Test Translator 1",
        "Test Translator 2"
      ],
      contributor: [
        "Test Contributor 1",
        "Test Contributor 2"
      ]
    },

    expected: {
      id: "addon1@tests.mozilla.org",
      type: "extension",
      version: "1.0",
      optionsType: null,
      aboutURL: "chrome://test/content/about.xul",
      iconURL: "chrome://test/skin/icon.png",
      icon64URL: "chrome://test/skin/icon64.png",
      icons: {32: "chrome://test/skin/icon.png", 48: "chrome://test/skin/icon.png", 64: "chrome://test/skin/icon64.png"},
      name: "Test Addon 1",
      description: "Test Description",
      creator: "Test Creator",
      homepageURL: "http://www.example.com",
      developers: ["Test Developer 1", "Test Developer 2"],
      translators: ["Test Translator 1", "Test Translator 2"],
      contributors: ["Test Contributor 1", "Test Contributor 2"],
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
      providesUpdatesSecurely: true,
      blocklistState: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    },
  },

  {
    "install.rdf": {
      id: "addon2@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      updateURL: "https://www.foo.com",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 2"
    },

    expected: {
      id: "addon2@tests.mozilla.org",
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      providesUpdatesSecurely: true,
    },
  },

  {
    "install.rdf": {
      id: "addon3@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      updateURL: "http://www.foo.com",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 3"
    },

    expected: {
      id: "addon3@tests.mozilla.org",
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      providesUpdatesSecurely: false,
    },
  },

  {
    "install.rdf": {
      id: "addon4@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      updateURL: "http://www.foo.com",
      updateKey: "foo",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 4"
    },

    expected: {
      id: "addon4@tests.mozilla.org",
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      providesUpdatesSecurely: false,
    },
  },

  {
    "install.rdf": {
      id: "addon5@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "*"
      }],
      name: "Test Addon 5"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
    },
  },

  {
    "install.rdf": {
      id: "addon6@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0",
        maxVersion: "1"
      }],
      name: "Test Addon 6"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
    },
  },

  {
    "install.rdf": {
      id: "addon7@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0",
        maxVersion: "0"
      }],
      name: "Test Addon 7"
    },

    expected: {
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      isCompatible: false,
    },
  },

  {
    "install.rdf": {
      id: "addon8@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1.1",
        maxVersion: "*"
      }],
      name: "Test Addon 8"
    },

    expected: {
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      isCompatible: false,
    },
  },

  {
    "install.rdf": {
      id: "addon9@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1.9.2",
        maxVersion: "1.9.*"
      }],
      name: "Test Addon 9"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
    },
  },

  {
    "install.rdf": {
      id: "addon10@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1.9.2.1",
        maxVersion: "1.9.*"
      }],
      name: "Test Addon 10"
    },

    expected: {
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      isCompatible: false,
    },
  },

  {
    "install.rdf": {
      id: "addon11@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1.9",
        maxVersion: "1.9.2"
      }],
      name: "Test Addon 11"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
    },
  },

  {
    "install.rdf": {
      id: "addon12@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1.9",
        maxVersion: "1.9.1.*"
      }],
      name: "Test Addon 12"
    },

    expected: {
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      isCompatible: false,
    },
  },

  {
    "install.rdf": {
      id: "addon13@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1.9",
        maxVersion: "1.9.*"
      }, {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "0",
        maxVersion: "0.5"
      }],
      name: "Test Addon 13"
    },

    expected: {
      isActive: false,
      userDisabled: false,
      appDisabled: true,
      isCompatible: false,
    },
  },

  {
    "install.rdf": {
      id: "addon14@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "toolkit@mozilla.org",
        minVersion: "1.9",
        maxVersion: "1.9.1"
      }, {
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 14"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
    },
  },

  {
    "install.rdf": {
      id: "addon15@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      updateKey: "foo",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 15"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
      providesUpdatesSecurely: true,
    },
  },

  {
    "install.rdf": {
      id: "addon16@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      updateKey: "foo",
      updateURL: "https://www.foo.com",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 16"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
      providesUpdatesSecurely: true,
    },
  },

  {
    "install.rdf": {
      id: "addon17@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsURL: "chrome://test/content/options.xul",
      optionsType: "2",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 17"
    },

    // An obsolete optionsType means the add-on isn't registered.
    expected: null,
  },

  {
    "install.rdf": {
      id: "addon18@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 18"
    },
    extraFiles: {"options.xul": ""},

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
      optionsURL: null,
      optionsType: null,
    },
  },

  {
    "install.rdf": {
      id: "addon19@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsType: "99",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 19"
    },

    expected: null,
  },

  {
    "install.rdf": {
      id: "addon20@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsURL: "chrome://test/content/options.xul",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 20"
    },

    // Even with a defined optionsURL optionsType is null by default.
    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
      optionsURL: "chrome://test/content/options.xul",
      optionsType: null,
    },
  },

  {
    "install.rdf": {
      id: "addon21@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsType: "3",
      optionsURL: "chrome://test/content/options.xul",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 21"
    },

    expected: {
      isActive: true,
      userDisabled: false,
      appDisabled: false,
      isCompatible: true,
      optionsURL: "chrome://test/content/options.xul",
      optionsType: AddonManager.OPTIONS_TYPE_TAB,
    },
  },

  {
    "install.rdf": {
      id: "addon22@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsType: "2",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 22"
    },

    // An obsolete optionsType means the add-on isn't registered.
    expected: null,
  },

  {
    "install.rdf": {
      id: "addon23@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsType: "2",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 23"
    },
    extraFiles: {"options.xul": ""},

    // An obsolete optionsType means the add-on isn't registered.
    expected: null,
  },

  {
    "install.rdf": {
      id: "addon24@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 24"
    },
    extraFiles: {"options.xul": ""},

    expected: {
      optionsType: null,
      optionsURL: null,
    },
  },

  {
    "install.rdf": {
      id: "addon25@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsType: "3",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 25"
    },

    expected: {
      optionsType: null,
      optionsURL: null,
    },
  },

  {
    "install.rdf": {
      id: "addon26@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      optionsType: "4",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }],
      name: "Test Addon 26"
    },
    extraFiles: {"options.xul": ""},
    expected: null,
  },

  // Tests compatibility based on target platforms.

  // No targetPlatforms so should be compatible
  {
    "install.rdf": {
      id: "tp-addon1@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      name: "Test 1",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },

    expected: {
      appDisabled: false,
      isPlatformCompatible: true,
      isActive: true,
    },
  },

  // Matches the OS
  {
    "install.rdf": {
      id: "tp-addon2@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      name: "Test 2",
      targetPlatforms: [
        "XPCShell",
        "WINNT_x86",
        "XPCShell"
      ],
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },

    expected: {
      appDisabled: false,
      isPlatformCompatible: true,
      isActive: true,
    },
  },

  // Matches the OS and ABI
  {
    "install.rdf": {
      id: "tp-addon3@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      name: "Test 3",
      targetPlatforms: [
        "WINNT",
        "XPCShell_noarch-spidermonkey"
      ],
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },

    expected: {
      appDisabled: false,
      isPlatformCompatible: true,
      isActive: true,
    },
  },

  // Doesn't match
  {
    "install.rdf": {
      id: "tp-addon4@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      name: "Test 4",
      targetPlatforms: [
        "WINNT_noarch-spidermonkey",
        "Darwin",
        "WINNT_noarch-spidermonkey"
      ],
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },

    expected: {
      appDisabled: true,
      isPlatformCompatible: false,
      isActive: false,
    },
  },

  // Matches the OS but since a different entry specifies ABI this doesn't match.
  {
    "install.rdf": {
      id: "tp-addon5@tests.mozilla.org",
      version: "1.0",
      bootstrap: true,
      name: "Test 5",
      targetPlatforms: [
        "XPCShell",
        "XPCShell_foo"
      ],
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "1"
      }]
    },

    expected: {
      appDisabled: true,
      isPlatformCompatible: false,
      isActive: false,
    },
  },
];

const IDS = ADDONS.map(a => a["install.rdf"].id);

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  const profileDir = gProfD.clone();
  profileDir.append("extensions");

  for (let addon of ADDONS) {
    await promiseWriteInstallRDFForExtension(addon["install.rdf"], profileDir, undefined, addon.extraFiles);
  }
});

add_task(async function test_values() {
  await promiseStartupManager();

  let addons = await getAddons(IDS);

  for (let addon of ADDONS) {
    let {id} = addon["install.rdf"];
    checkAddon(id, addons.get(id), addon.expected);
  }

  await promiseShutdownManager();
});
