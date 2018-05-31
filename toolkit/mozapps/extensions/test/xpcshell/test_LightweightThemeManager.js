const MANDATORY = ["id", "name"];
const OPTIONAL = ["headerURL", "footerURL", "textcolor", "accentcolor",
                  "iconURL", "previewURL", "author", "description",
                  "homepageURL", "updateURL", "version"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

const DEFAULT_THEME_ID = "default-theme@mozilla.org";

function dummy(id) {
  return {
    id: id || Math.random().toString(),
    name: Math.random().toString(),
    headerURL: "http://lwttest.invalid/a.png",
    footerURL: "http://lwttest.invalid/b.png",
    textcolor: Math.random().toString(),
    accentcolor: Math.random().toString()
  };
}

function hasPermission(aAddon, aPerm) {
  var perm = AddonManager["PERM_CAN_" + aPerm.toUpperCase()];
  return !!(aAddon.permissions & perm);
}

add_task(async function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  await promiseStartupManager();

  Services.prefs.setIntPref("lightweightThemes.maxUsedThemes", 8);

  let {LightweightThemeManager: ltm} = ChromeUtils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

  Assert.equal(typeof ltm, "object");
  Assert.equal(typeof ltm.usedThemes, "object");
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  ltm.previewTheme(dummy("preview0"));
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  ltm.previewTheme(dummy("preview1"));
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);
  ltm.resetPreview();

  ltm.currentTheme = dummy("x0");
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.id, "x0");
  Assert.equal(ltm.usedThemes[0].id, "x0");
  Assert.equal(ltm.getUsedTheme("x0").id, "x0");

  ltm.previewTheme(dummy("preview0"));
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.id, "x0");

  ltm.resetPreview();
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.id, "x0");

  ltm.currentTheme = dummy("x1");
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.currentTheme.id, "x1");
  Assert.equal(ltm.usedThemes[1].id, "x0");

  ltm.currentTheme = dummy("x2");
  Assert.equal(ltm.usedThemes.length, 4);
  Assert.equal(ltm.currentTheme.id, "x2");
  Assert.equal(ltm.usedThemes[1].id, "x1");
  Assert.equal(ltm.usedThemes[2].id, "x0");

  ltm.currentTheme = dummy("x3");
  ltm.currentTheme = dummy("x4");
  ltm.currentTheme = dummy("x5");
  ltm.currentTheme = dummy("x6");
  ltm.currentTheme = dummy("x7");
  Assert.equal(ltm.usedThemes.length, 9);
  Assert.equal(ltm.currentTheme.id, "x7");
  Assert.equal(ltm.usedThemes[1].id, "x6");
  Assert.equal(ltm.usedThemes[7].id, "x0");

  ltm.currentTheme = dummy("x8");
  Assert.equal(ltm.usedThemes.length, 9);
  Assert.equal(ltm.currentTheme.id, "x8");
  Assert.equal(ltm.usedThemes[1].id, "x7");
  Assert.equal(ltm.usedThemes[7].id, "x1");
  Assert.equal(ltm.getUsedTheme("x0"), null);

  ltm.forgetUsedTheme("nonexistent");
  Assert.equal(ltm.usedThemes.length, 9);
  Assert.notEqual(ltm.currentTheme.id, DEFAULT_THEME_ID);

  ltm.forgetUsedTheme("x8");
  Assert.equal(ltm.usedThemes.length, 8);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);
  Assert.equal(ltm.usedThemes[0].id, "x7");
  Assert.equal(ltm.usedThemes[6].id, "x1");

  ltm.forgetUsedTheme("x7");
  ltm.forgetUsedTheme("x6");
  ltm.forgetUsedTheme("x5");
  ltm.forgetUsedTheme("x4");
  ltm.forgetUsedTheme("x3");
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);
  Assert.equal(ltm.usedThemes[0].id, "x2");
  Assert.equal(ltm.usedThemes[1].id, "x1");

  ltm.currentTheme = dummy("x1");
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.currentTheme.id, "x1");
  Assert.equal(ltm.usedThemes[0].id, "x1");
  Assert.equal(ltm.usedThemes[1].id, "x2");

  ltm.currentTheme = dummy("x2");
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.currentTheme.id, "x2");
  Assert.equal(ltm.usedThemes[0].id, "x2");
  Assert.equal(ltm.usedThemes[1].id, "x1");

  ltm.currentTheme = ltm.getUsedTheme("x1");
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.currentTheme.id, "x1");
  Assert.equal(ltm.usedThemes[0].id, "x1");
  Assert.equal(ltm.usedThemes[1].id, "x2");

  ltm.forgetUsedTheme("x1");
  ltm.forgetUsedTheme("x2");
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  // Use chinese name to test utf-8, for bug #541943
  var chineseTheme = dummy("chinese0");
  chineseTheme.name = "笢恅0";
  chineseTheme.description = "笢恅1";
  ltm.currentTheme = chineseTheme;
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.name, "笢恅0");
  Assert.equal(ltm.currentTheme.description, "笢恅1");
  Assert.equal(ltm.usedThemes[0].name, "笢恅0");
  Assert.equal(ltm.usedThemes[0].description, "笢恅1");
  Assert.equal(ltm.getUsedTheme("chinese0").name, "笢恅0");
  Assert.equal(ltm.getUsedTheme("chinese0").description, "笢恅1");

  // This name used to break the usedTheme JSON causing all LWTs to be lost
  var chineseTheme1 = dummy("chinese1");
  chineseTheme1.name = "眵昜湮桵蔗坌~郔乾";
  chineseTheme1.description = "眵昜湮桵蔗坌~郔乾";
  ltm.currentTheme = chineseTheme1;
  Assert.notEqual(ltm.currentTheme.id, DEFAULT_THEME_ID);
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.currentTheme.name, "眵昜湮桵蔗坌~郔乾");
  Assert.equal(ltm.currentTheme.description, "眵昜湮桵蔗坌~郔乾");
  Assert.equal(ltm.usedThemes[1].name, "笢恅0");
  Assert.equal(ltm.usedThemes[1].description, "笢恅1");
  Assert.equal(ltm.usedThemes[0].name, "眵昜湮桵蔗坌~郔乾");
  Assert.equal(ltm.usedThemes[0].description, "眵昜湮桵蔗坌~郔乾");

  ltm.forgetUsedTheme("chinese0");
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.notEqual(ltm.currentTheme.id, DEFAULT_THEME_ID);

  ltm.forgetUsedTheme("chinese1");
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  Assert.equal(ltm.parseTheme("invalid json"), null);
  Assert.equal(ltm.parseTheme('"json string"'), null);

  function roundtrip(data, secure) {
    return ltm.parseTheme(JSON.stringify(data),
                          "http" + (secure ? "s" : "") + "://lwttest.invalid/");
  }

  var data = dummy();
  Assert.notEqual(roundtrip(data), null);
  data.id = null;
  Assert.equal(roundtrip(data), null);
  data.id = 1;
  Assert.equal(roundtrip(data), null);
  data.id = 1.5;
  Assert.equal(roundtrip(data), null);
  data.id = true;
  Assert.equal(roundtrip(data), null);
  data.id = {};
  Assert.equal(roundtrip(data), null);
  data.id = [];
  Assert.equal(roundtrip(data), null);

  // Check whether parseTheme handles international characters right
  var chineseTheme2 = dummy();
  chineseTheme2.name = "眵昜湮桵蔗坌~郔乾";
  chineseTheme2.description = "眵昜湮桵蔗坌~郔乾";
  Assert.notEqual(roundtrip(chineseTheme2), null);
  Assert.equal(roundtrip(chineseTheme2).name, "眵昜湮桵蔗坌~郔乾");
  Assert.equal(roundtrip(chineseTheme2).description, "眵昜湮桵蔗坌~郔乾");

  data = dummy();
  data.unknownProperty = "Foo";
  Assert.equal(typeof roundtrip(data).unknownProperty, "undefined");

  data = dummy();
  data.unknownURL = "http://lwttest.invalid/";
  Assert.equal(typeof roundtrip(data).unknownURL, "undefined");

  function roundtripSet(props, modify, test, secure) {
    props.forEach(function(prop) {
      var theme = dummy();
      modify(theme, prop);
      test(roundtrip(theme, secure), prop, theme);
    });
  }

  roundtripSet(MANDATORY, function(theme, prop) {
    delete theme[prop];
  }, function(after) {
    Assert.equal(after, null);
  });

  roundtripSet(OPTIONAL, function(theme, prop) {
    delete theme[prop];
  }, function(after) {
    Assert.notEqual(after, null);
  });

  roundtripSet(MANDATORY, function(theme, prop) {
    theme[prop] = "";
  }, function(after) {
    Assert.equal(after, null);
  });

  roundtripSet(OPTIONAL, function(theme, prop) {
    theme[prop] = "";
  }, function(after, prop) {
    Assert.equal(typeof after[prop], "undefined");
  });

  roundtripSet(MANDATORY, function(theme, prop) {
    theme[prop] = " ";
  }, function(after) {
    Assert.equal(after, null);
  });

  roundtripSet(OPTIONAL, function(theme, prop) {
    theme[prop] = " ";
  }, function(after, prop) {
    Assert.notEqual(after, null);
    Assert.equal(typeof after[prop], "undefined");
  });

  function non_urls(props) {
    return props.filter(prop => !/URL$/.test(prop));
  }

  function urls(props) {
    return props.filter(prop => /URL$/.test(prop));
  }

  roundtripSet(non_urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = prop;
  }, function(after, prop, before) {
    Assert.equal(after[prop], before[prop]);
  });

  roundtripSet(non_urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = " " + prop + "  ";
  }, function(after, prop, before) {
    Assert.equal(after[prop], before[prop].trim());
  });

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = Math.random().toString();
  }, function(after, prop, before) {
    if (prop == "updateURL")
      Assert.equal(typeof after[prop], "undefined");
    else
      Assert.equal(after[prop], "http://lwttest.invalid/" + before[prop]);
  });

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = Math.random().toString();
  }, function(after, prop, before) {
    Assert.equal(after[prop], "https://lwttest.invalid/" + before[prop]);
  }, true);

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = "https://sub.lwttest.invalid/" + Math.random().toString();
  }, function(after, prop, before) {
    Assert.equal(after[prop], before[prop]);
  });

  roundtripSet(urls(MANDATORY), function(theme, prop) {
    theme[prop] = "ftp://lwttest.invalid/" + Math.random().toString();
  }, function(after) {
    Assert.equal(after, null);
  });

  roundtripSet(urls(OPTIONAL), function(theme, prop) {
    theme[prop] = "ftp://lwttest.invalid/" + Math.random().toString();
  }, function(after, prop) {
    Assert.equal(typeof after[prop], "undefined");
  });

  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  data = dummy();
  delete data.name;
  try {
    ltm.currentTheme = data;
    do_throw("Should have rejected a theme with no name");
  } catch (e) {
    // Expected exception
  }

  // Sanitize themes with a bad headerURL
  data = dummy();
  data.headerURL = "foo";
  ltm.currentTheme = data;
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.headerURL, undefined);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  Assert.equal(ltm.usedThemes.length, 1);

  // Sanitize themes with a non-http(s) headerURL
  data = dummy();
  data.headerURL = "ftp://lwtest.invalid/test.png";
  ltm.currentTheme = data;
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.headerURL, undefined);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  Assert.equal(ltm.usedThemes.length, 1);

  // Sanitize themes with a non-http(s) headerURL
  data = dummy();
  data.headerURL = "file:///test.png";
  ltm.currentTheme = data;
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.headerURL, undefined);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  Assert.equal(ltm.usedThemes.length, 1);

  data = dummy();
  data.updateURL = "file:///test.json";
  ltm.setLocalTheme(data);
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.updateURL, undefined);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  Assert.equal(ltm.usedThemes.length, 1);

  data = dummy();
  data.headerURL = "file:///test.png";
  ltm.setLocalTheme(data);
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.headerURL, "file:///test.png");
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  Assert.equal(ltm.usedThemes.length, 1);

  data = dummy();
  data.headerURL = "ftp://lwtest.invalid/test.png";
  ltm.setLocalTheme(data);
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.currentTheme.updateURL, undefined);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  Assert.equal(ltm.usedThemes.length, 1);

  data = dummy();
  delete data.id;
  try {
    ltm.currentTheme = data;
    do_throw("Should have rejected a theme with no ID");
  } catch (e) {
    // Expected exception
  }

  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  // Force the theme into the prefs anyway
  let themes = [data];
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));
  Assert.equal(ltm.usedThemes.length, 2);

  // This should silently drop the bad theme.
  ltm.currentTheme = dummy();
  Assert.equal(ltm.usedThemes.length, 2);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  // Add one broken and some working.
  themes = [data, dummy("x1"), dummy("x2")];
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));
  Assert.equal(ltm.usedThemes.length, 4);

  // Switching to an existing theme should drop the bad theme.
  ltm.currentTheme = ltm.getUsedTheme("x1");
  Assert.equal(ltm.usedThemes.length, 3);
  ltm.forgetUsedTheme("x1");
  ltm.forgetUsedTheme("x2");
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));
  Assert.equal(ltm.usedThemes.length, 4);

  // Forgetting an existing theme should drop the bad theme.
  ltm.forgetUsedTheme("x1");
  Assert.equal(ltm.usedThemes.length, 2);
  ltm.forgetUsedTheme("x2");
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  // Test whether a JSON set with setCharPref can be retrieved with usedThemes
  ltm.currentTheme = dummy("x0");
  ltm.currentTheme = dummy("x1");
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(ltm.usedThemes));
  Assert.equal(ltm.usedThemes.length, 4);
  Assert.equal(ltm.currentTheme.id, "x1");
  Assert.equal(ltm.usedThemes[1].id, "x0");
  Assert.equal(ltm.usedThemes[0].id, "x1");

  ltm.forgetUsedTheme("x0");
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.notEqual(ltm.currentTheme.id, DEFAULT_THEME_ID);

  ltm.forgetUsedTheme("x1");
  Assert.equal(ltm.usedThemes.length, 1);
  Assert.equal(ltm.currentTheme.id, DEFAULT_THEME_ID);

  Services.prefs.clearUserPref("lightweightThemes.maxUsedThemes");

  ltm.currentTheme = dummy("x1");
  ltm.currentTheme = dummy("x2");
  ltm.currentTheme = dummy("x3");
  ltm.currentTheme = dummy("x4");
  ltm.currentTheme = dummy("x5");
  ltm.currentTheme = dummy("x6");
  ltm.currentTheme = dummy("x7");
  ltm.currentTheme = dummy("x8");
  ltm.currentTheme = dummy("x9");
  ltm.currentTheme = dummy("x10");
  ltm.currentTheme = dummy("x11");
  ltm.currentTheme = dummy("x12");
  ltm.currentTheme = dummy("x13");
  ltm.currentTheme = dummy("x14");
  ltm.currentTheme = dummy("x15");
  ltm.currentTheme = dummy("x16");
  ltm.currentTheme = dummy("x17");
  ltm.currentTheme = dummy("x18");
  ltm.currentTheme = dummy("x19");
  ltm.currentTheme = dummy("x20");
  ltm.currentTheme = dummy("x21");
  ltm.currentTheme = dummy("x22");
  ltm.currentTheme = dummy("x23");
  ltm.currentTheme = dummy("x24");
  ltm.currentTheme = dummy("x25");
  ltm.currentTheme = dummy("x26");
  ltm.currentTheme = dummy("x27");
  ltm.currentTheme = dummy("x28");
  ltm.currentTheme = dummy("x29");
  ltm.currentTheme = dummy("x30");

  Assert.equal(ltm.usedThemes.length, 31);

  ltm.currentTheme = dummy("x31");

  Assert.equal(ltm.usedThemes.length, 31);
  Assert.equal(ltm.getUsedTheme("x1"), null);

  Services.prefs.setIntPref("lightweightThemes.maxUsedThemes", 15);

  Assert.equal(ltm.usedThemes.length, 16);

  Services.prefs.setIntPref("lightweightThemes.maxUsedThemes", 32);

  ltm.currentTheme = dummy("x1");
  ltm.currentTheme = dummy("x2");
  ltm.currentTheme = dummy("x3");
  ltm.currentTheme = dummy("x4");
  ltm.currentTheme = dummy("x5");
  ltm.currentTheme = dummy("x6");
  ltm.currentTheme = dummy("x7");
  ltm.currentTheme = dummy("x8");
  ltm.currentTheme = dummy("x9");
  ltm.currentTheme = dummy("x10");
  ltm.currentTheme = dummy("x11");
  ltm.currentTheme = dummy("x12");
  ltm.currentTheme = dummy("x13");
  ltm.currentTheme = dummy("x14");
  ltm.currentTheme = dummy("x15");
  ltm.currentTheme = dummy("x16");

  ltm.currentTheme = dummy("x32");

  Assert.equal(ltm.usedThemes.length, 33);

  ltm.currentTheme = dummy("x33");

  Assert.equal(ltm.usedThemes.length, 33);

  Services.prefs.clearUserPref("lightweightThemes.maxUsedThemes");

  Assert.equal(ltm.usedThemes.length, 31);

  let usedThemes = ltm.usedThemes;
  for (let theme of usedThemes) {
    ltm.forgetUsedTheme(theme.id);
  }

  // Check builtInTheme functionality for Bug 1094821
  Assert.equal(ltm._builtInThemes.toString(), "[object Map]");
  Assert.equal([...ltm._builtInThemes.entries()].length, 1);
  Assert.equal(ltm.usedThemes.length, 1);

  ltm.addBuiltInTheme(dummy("builtInTheme0"));
  Assert.equal([...ltm._builtInThemes].length, 2);
  Assert.equal(ltm.usedThemes.length, 2);
  Assert.equal(ltm.usedThemes[1].id, "builtInTheme0");

  ltm.addBuiltInTheme(dummy("builtInTheme1"));
  Assert.equal([...ltm._builtInThemes].length, 3);
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.usedThemes[2].id, "builtInTheme1");

  // Clear all and then re-add
  ltm.clearBuiltInThemes();
  Assert.equal([...ltm._builtInThemes].length, 0);
  Assert.equal(ltm.usedThemes.length, 0);

  ltm.addBuiltInTheme(dummy("builtInTheme0"));
  ltm.addBuiltInTheme(dummy("builtInTheme1"));
  Assert.equal([...ltm._builtInThemes].length, 2);
  Assert.equal(ltm.usedThemes.length, 2);

  let builtInThemeAddon = await AddonManager.getAddonByID("builtInTheme0@personas.mozilla.org");
  Assert.equal(hasPermission(builtInThemeAddon, "uninstall"), false);
  Assert.equal(hasPermission(builtInThemeAddon, "disable"), false);
  Assert.equal(hasPermission(builtInThemeAddon, "enable"), true);

  ltm.currentTheme = dummy("x0");
  Assert.equal([...ltm._builtInThemes].length, 2);
  Assert.equal(ltm.usedThemes.length, 3);
  Assert.equal(ltm.usedThemes[0].id, "x0");
  Assert.equal(ltm.currentTheme.id, "x0");
  Assert.equal(ltm.usedThemes[1].id, "builtInTheme0");
  Assert.equal(ltm.usedThemes[2].id, "builtInTheme1");

  Assert.throws(() => { ltm.addBuiltInTheme(dummy("builtInTheme0")); },
    /Error: Trying to add invalid builtIn theme/,
    "Exception is thrown adding a duplicate theme");
  Assert.throws(() => { ltm.addBuiltInTheme("not a theme object"); },
    /Error: Trying to add invalid builtIn theme/,
    "Exception is thrown adding an invalid theme");

  let x0Addon = await AddonManager.getAddonByID("x0@personas.mozilla.org");
  Assert.equal(hasPermission(x0Addon, "uninstall"), true);
  Assert.equal(hasPermission(x0Addon, "disable"), true);
  Assert.equal(hasPermission(x0Addon, "enable"), false);

  ltm.forgetUsedTheme("x0");
  Assert.equal(ltm.currentTheme, null);

  // Removing the currently applied app specific theme should unapply it
  ltm.currentTheme = ltm.getUsedTheme("builtInTheme0");
  Assert.equal(ltm.currentTheme.id, "builtInTheme0");
  Assert.ok(ltm.forgetBuiltInTheme("builtInTheme0"));
  Assert.equal(ltm.currentTheme, null);

  Assert.equal([...ltm._builtInThemes].length, 1);
  Assert.equal(ltm.usedThemes.length, 1);

  Assert.ok(ltm.forgetBuiltInTheme("builtInTheme1"));
  Assert.ok(!ltm.forgetBuiltInTheme("not-an-existing-theme-id"));

  Assert.equal([...ltm._builtInThemes].length, 0);
  Assert.equal(ltm.usedThemes.length, 0);
  Assert.equal(ltm.currentTheme, null);
});
