"use strict";

ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm");

Services.prefs.setBoolPref(PREF_EM_CHECK_UPDATE_SECURITY, false);

add_task(async function test_theme_updates() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  const server = createHttpServer({hosts: ["example.com"]});

  let updates = {};
  server.registerPrefixHandler("/", (request, response) => {
    let update = updates[request.path];
    if (update) {
      response.setHeader("content-type", "application/json");
      response.write(JSON.stringify(update));
    } else {
      response.setStatusLine("1.1", 404, "Not Found");
    }
  });

  // Themes 1 and 2 will be updated to xpi themes, theme 3 will
  // (attempt to) be updated to a regular webextension.
  let theme1XPI = createTempWebExtensionFile({
    manifest: {
      name: "Theme 1",
      version: "2.0",
      theme: {
        images: { headerURL: "webextension.png" },
      },
    }
  });
  server.registerFile("/theme1.xpi", theme1XPI);

  let theme2XPI = createTempWebExtensionFile({
    manifest: {
      name: "Theme 2",
      version: "2.0",
      theme: {
        images: { headerURL: "webextension.png" },
      },
    }
  });
  server.registerFile("/theme2.xpi", theme2XPI);

  let theme3XPI = createTempWebExtensionFile({
    manifest: {
      name: "Theme 3",
      version: "2.0",
    }
  });
  server.registerFile("/theme3.xpi", theme3XPI);

  await promiseStartupManager();

  function makeTheme(n, {image = "orig.png", version = "1.0"} = {}) {
    return {
      id: `theme${n}`,
      name: `Theme ${n}`,
      version,
      updateURL: `http://example.com/${n}`,
      headerURL: `http://example.com/${image}`,
      footerURL: `http://example.com/${image}`,
      textcolor: "#000000",
      accentcolor: "#ffffff",
    };
  }

  // "Install" some LWTs.  Leave theme 2 as current
  LightweightThemeManager.currentTheme = makeTheme(1);
  LightweightThemeManager.currentTheme = makeTheme(3);
  LightweightThemeManager.currentTheme = makeTheme(2);

  function filterThemes(themes) {
    return themes.filter(t => t.id != "default-theme@mozilla.org");
  }

  equal(filterThemes(LightweightThemeManager.usedThemes).length, 3,
        "Have 3 lightweight themes");

  // Update URLs are all 404, nothing should change
  await AddonManagerPrivate.backgroundUpdateCheck();

  equal(filterThemes(LightweightThemeManager.usedThemes).length, 3,
        "Have 3 lightweight themes");

  // Make updates to all LWTs available, only the active on should be updated
  updates["/1"] = makeTheme(1, {image: "new.png", version: "1.1"});
  updates["/2"] = makeTheme(2, {image: "new.png", version: "1.1"});
  updates["/3"] = makeTheme(3, {image: "new.png", version: "1.1"});
  await AddonManagerPrivate.backgroundUpdateCheck();

  equal(LightweightThemeManager.currentTheme.id, "theme2",
        "Theme 2 is currently selected");

  let lwThemes = filterThemes(LightweightThemeManager.usedThemes);
  let theme1 = lwThemes.find(t => t.id == "theme1");
  notEqual(theme1, undefined, "Found theme1");
  ok(theme1.headerURL.endsWith("orig.png"), "LWT 1 was not updated");

  let theme2 = lwThemes.find(t => t.id == "theme2");
  notEqual(theme2, undefined, "Found theme2");
  dump(`theme2 ${JSON.stringify(theme2)}\n`);
  ok(theme2.headerURL.endsWith("new.png"), "LWT 2 was updated");

  let theme3 = lwThemes.find(t => t.id == "theme3");
  notEqual(theme3, undefined, "Found theme3");
  ok(theme3.headerURL.endsWith("orig.png"), "LWT 3 was not updated");

  // Update an inactive LWT to an XPI packaged theme
  updates["/1"] = {
    converted_theme: {
      url: "http://example.com/theme1.xpi",
      hash: do_get_file_hash(theme1XPI),
    },
  };

  await AddonManagerPrivate.backgroundUpdateCheck();

  let themes = filterThemes(await AddonManager.getAddonsByTypes(["theme"]));
  equal(themes.length, 3, "Still have a total of 3 themes");
  equal(filterThemes(LightweightThemeManager.usedThemes).length, 2,
        "Have 2 lightweight themes");
  theme1 = themes.find(t => t.name == "Theme 1");
  notEqual(theme1, undefined, "Found theme1");
  ok(!theme1.id.endsWith("@personas.mozilla.org"),
     "Theme 1 has been updated to an XPI packaged theme");
  equal(theme1.userDisabled, true, "Theme 1 is not active");

  // Update the current LWT to an XPI
  updates["/2"] = {
    converted_theme: {
      url: "http://example.com/theme2.xpi",
      hash: do_get_file_hash(theme2XPI),
    },
  };

  await AddonManagerPrivate.backgroundUpdateCheck();
  themes = filterThemes(await AddonManager.getAddonsByTypes(["theme"]));
  equal(themes.length, 3, "Still have a total of 3 themes");
  equal(filterThemes(LightweightThemeManager.usedThemes).length, 1,
        "Have 1 lightweight theme");
  theme2 = themes.find(t => t.name == "Theme 2");
  notEqual(theme2, undefined, "Found theme2");
  ok(!theme2.id.endsWith("@personas.mozilla.org"),
     "Theme 2 has been updated to an XPI packaged theme");
  equal(theme2.userDisabled, false, "Theme 2 is active");

  // Serve an update with a bad hash, the LWT should remain in place.
  updates["/3"] = {
    converted_theme: {
      url: "http://example.com/theme3.xpi",
      hash: "sha256:abcd",
    },
  };

  await AddonManagerPrivate.backgroundUpdateCheck();
  equal(filterThemes(LightweightThemeManager.usedThemes).length, 1,
        "Still have 1 lightweight theme");

  // Try updating to a regular (non-theme) webextension, the
  // LWT should remain in place.
  updates["/3"] = {
    converted_theme: {
      url: "http://example.com/theme3.xpi",
      hash: do_get_file_hash(theme3XPI),
    },
  };
  await AddonManagerPrivate.backgroundUpdateCheck();
  equal(filterThemes(LightweightThemeManager.usedThemes).length, 1,
        "Still have 1 lightweight theme");

  await promiseShutdownManager();
});
