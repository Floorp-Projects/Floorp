var Cc = Components.classes;
var Ci = Components.interfaces;

const MANDATORY = ["id", "name", "headerURL"];
const OPTIONAL = ["footerURL", "textcolor", "accentcolor", "iconURL",
                  "previewURL", "author", "description", "homepageURL",
                  "updateURL", "version"];

Components.utils.import("resource://gre/modules/Services.jsm");

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

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  Services.prefs.setIntPref("lightweightThemes.maxUsedThemes", 8);

  let {LightweightThemeManager: ltm} = Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", {});

  do_check_eq(typeof ltm, "object");
  do_check_eq(typeof ltm.usedThemes, "object");
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  ltm.previewTheme(dummy("preview0"));
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  ltm.previewTheme(dummy("preview1"));
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);
  ltm.resetPreview();

  ltm.currentTheme = dummy("x0");
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_eq(ltm.currentTheme.id, "x0");
  do_check_eq(ltm.usedThemes[0].id, "x0");
  do_check_eq(ltm.getUsedTheme("x0").id, "x0");

  ltm.previewTheme(dummy("preview0"));
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_eq(ltm.currentTheme.id, "x0");

  ltm.resetPreview();
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_eq(ltm.currentTheme.id, "x0");

  ltm.currentTheme = dummy("x1");
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.currentTheme.id, "x1");
  do_check_eq(ltm.usedThemes[1].id, "x0");

  ltm.currentTheme = dummy("x2");
  do_check_eq(ltm.usedThemes.length, 3);
  do_check_eq(ltm.currentTheme.id, "x2");
  do_check_eq(ltm.usedThemes[1].id, "x1");
  do_check_eq(ltm.usedThemes[2].id, "x0");

  ltm.currentTheme = dummy("x3");
  ltm.currentTheme = dummy("x4");
  ltm.currentTheme = dummy("x5");
  ltm.currentTheme = dummy("x6");
  ltm.currentTheme = dummy("x7");
  do_check_eq(ltm.usedThemes.length, 8);
  do_check_eq(ltm.currentTheme.id, "x7");
  do_check_eq(ltm.usedThemes[1].id, "x6");
  do_check_eq(ltm.usedThemes[7].id, "x0");

  ltm.currentTheme = dummy("x8");
  do_check_eq(ltm.usedThemes.length, 8);
  do_check_eq(ltm.currentTheme.id, "x8");
  do_check_eq(ltm.usedThemes[1].id, "x7");
  do_check_eq(ltm.usedThemes[7].id, "x1");
  do_check_eq(ltm.getUsedTheme("x0"), null);

  ltm.forgetUsedTheme("nonexistent");
  do_check_eq(ltm.usedThemes.length, 8);
  do_check_neq(ltm.currentTheme, null);

  ltm.forgetUsedTheme("x8");
  do_check_eq(ltm.usedThemes.length, 7);
  do_check_eq(ltm.currentTheme, null);
  do_check_eq(ltm.usedThemes[0].id, "x7");
  do_check_eq(ltm.usedThemes[6].id, "x1");

  ltm.forgetUsedTheme("x7");
  ltm.forgetUsedTheme("x6");
  ltm.forgetUsedTheme("x5");
  ltm.forgetUsedTheme("x4");
  ltm.forgetUsedTheme("x3");
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.currentTheme, null);
  do_check_eq(ltm.usedThemes[0].id, "x2");
  do_check_eq(ltm.usedThemes[1].id, "x1");

  ltm.currentTheme = dummy("x1");
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.currentTheme.id, "x1");
  do_check_eq(ltm.usedThemes[0].id, "x1");
  do_check_eq(ltm.usedThemes[1].id, "x2");

  ltm.currentTheme = dummy("x2");
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.currentTheme.id, "x2");
  do_check_eq(ltm.usedThemes[0].id, "x2");
  do_check_eq(ltm.usedThemes[1].id, "x1");

  ltm.currentTheme = ltm.getUsedTheme("x1");
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.currentTheme.id, "x1");
  do_check_eq(ltm.usedThemes[0].id, "x1");
  do_check_eq(ltm.usedThemes[1].id, "x2");

  ltm.forgetUsedTheme("x1");
  ltm.forgetUsedTheme("x2");
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  // Use chinese name to test utf-8, for bug #541943
  var chineseTheme = dummy("chinese0");
  chineseTheme.name = "笢恅0";
  chineseTheme.description = "笢恅1";
  ltm.currentTheme = chineseTheme;
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_eq(ltm.currentTheme.name, "笢恅0");
  do_check_eq(ltm.currentTheme.description, "笢恅1");
  do_check_eq(ltm.usedThemes[0].name, "笢恅0");
  do_check_eq(ltm.usedThemes[0].description, "笢恅1");
  do_check_eq(ltm.getUsedTheme("chinese0").name, "笢恅0");
  do_check_eq(ltm.getUsedTheme("chinese0").description, "笢恅1");

  // This name used to break the usedTheme JSON causing all LWTs to be lost
  var chineseTheme1 = dummy("chinese1");
  chineseTheme1.name = "眵昜湮桵蔗坌~郔乾";
  chineseTheme1.description = "眵昜湮桵蔗坌~郔乾";
  ltm.currentTheme = chineseTheme1;
  do_check_neq(ltm.currentTheme, null);
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.currentTheme.name, "眵昜湮桵蔗坌~郔乾");
  do_check_eq(ltm.currentTheme.description, "眵昜湮桵蔗坌~郔乾");
  do_check_eq(ltm.usedThemes[1].name, "笢恅0");
  do_check_eq(ltm.usedThemes[1].description, "笢恅1");
  do_check_eq(ltm.usedThemes[0].name, "眵昜湮桵蔗坌~郔乾");
  do_check_eq(ltm.usedThemes[0].description, "眵昜湮桵蔗坌~郔乾");

  ltm.forgetUsedTheme("chinese0");
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_neq(ltm.currentTheme, null);

  ltm.forgetUsedTheme("chinese1");
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  do_check_eq(ltm.parseTheme("invalid json"), null);
  do_check_eq(ltm.parseTheme('"json string"'), null);

  function roundtrip(data, secure) {
    return ltm.parseTheme(JSON.stringify(data),
                          "http" + (secure ? "s" : "") + "://lwttest.invalid/");
  }

  var data = dummy();
  do_check_neq(roundtrip(data), null);
  data.id = null;
  do_check_eq(roundtrip(data), null);
  data.id = 1;
  do_check_eq(roundtrip(data), null);
  data.id = 1.5;
  do_check_eq(roundtrip(data), null);
  data.id = true;
  do_check_eq(roundtrip(data), null);
  data.id = {};
  do_check_eq(roundtrip(data), null);
  data.id = [];
  do_check_eq(roundtrip(data), null);

  // Check whether parseTheme handles international characters right
  var chineseTheme2 = dummy();
  chineseTheme2.name = "眵昜湮桵蔗坌~郔乾";
  chineseTheme2.description = "眵昜湮桵蔗坌~郔乾";
  do_check_neq(roundtrip(chineseTheme2), null);
  do_check_eq(roundtrip(chineseTheme2).name, "眵昜湮桵蔗坌~郔乾");
  do_check_eq(roundtrip(chineseTheme2).description, "眵昜湮桵蔗坌~郔乾");

  data = dummy();
  data.unknownProperty = "Foo";
  do_check_eq(typeof roundtrip(data).unknownProperty, "undefined");

  data = dummy();
  data.unknownURL = "http://lwttest.invalid/";
  do_check_eq(typeof roundtrip(data).unknownURL, "undefined");

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
    do_check_eq(after, null);
  });

  roundtripSet(OPTIONAL, function(theme, prop) {
    delete theme[prop];
  }, function(after) {
    do_check_neq(after, null);
  });

  roundtripSet(MANDATORY, function(theme, prop) {
    theme[prop] = "";
  }, function(after) {
    do_check_eq(after, null);
  });

  roundtripSet(OPTIONAL, function(theme, prop) {
    theme[prop] = "";
  }, function(after, prop) {
    do_check_eq(typeof after[prop], "undefined");
  });

  roundtripSet(MANDATORY, function(theme, prop) {
    theme[prop] = " ";
  }, function(after) {
    do_check_eq(after, null);
  });

  roundtripSet(OPTIONAL, function(theme, prop) {
    theme[prop] = " ";
  }, function(after, prop) {
    do_check_neq(after, null);
    do_check_eq(typeof after[prop], "undefined");
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
    do_check_eq(after[prop], before[prop]);
  });

  roundtripSet(non_urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = " " + prop + "  ";
  }, function(after, prop, before) {
    do_check_eq(after[prop], before[prop].trim());
  });

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = Math.random().toString();
  }, function(after, prop, before) {
    if (prop == "updateURL")
      do_check_eq(typeof after[prop], "undefined");
    else
      do_check_eq(after[prop], "http://lwttest.invalid/" + before[prop]);
  });

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = Math.random().toString();
  }, function(after, prop, before) {
    do_check_eq(after[prop], "https://lwttest.invalid/" + before[prop]);
  }, true);

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function(theme, prop) {
    theme[prop] = "https://sub.lwttest.invalid/" + Math.random().toString();
  }, function(after, prop, before) {
    do_check_eq(after[prop], before[prop]);
  });

  roundtripSet(urls(MANDATORY), function(theme, prop) {
    theme[prop] = "ftp://lwttest.invalid/" + Math.random().toString();
  }, function(after) {
    do_check_eq(after, null);
  });

  roundtripSet(urls(OPTIONAL), function(theme, prop) {
    theme[prop] = "ftp://lwttest.invalid/" + Math.random().toString();
  }, function(after, prop) {
    do_check_eq(typeof after[prop], "undefined");
  });

  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  data = dummy();
  delete data.name;
  try {
    ltm.currentTheme = data;
    do_throw("Should have rejected a theme with no name");
  } catch (e) {
    // Expected exception
  }

  data = dummy();
  data.headerURL = "foo";
  try {
    ltm.currentTheme = data;
    do_throw("Should have rejected a theme with a bad headerURL");
  } catch (e) {
    // Expected exception
  }

  data = dummy();
  data.headerURL = "ftp://lwtest.invalid/test.png";
  try {
    ltm.currentTheme = data;
    do_throw("Should have rejected a theme with a non-http(s) headerURL");
  } catch (e) {
    // Expected exception
  }

  data = dummy();
  data.headerURL = "file:///test.png";
  try {
    ltm.currentTheme = data;
    do_throw("Should have rejected a theme with a non-http(s) headerURL");
  } catch (e) {
    // Expected exception
  }

  data = dummy();
  data.updateURL = "file:///test.json";
  ltm.setLocalTheme(data);
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_eq(ltm.currentTheme.updateURL, undefined);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  do_check_eq(ltm.usedThemes.length, 0);

  data = dummy();
  data.headerURL = "file:///test.png";
  ltm.setLocalTheme(data);
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_eq(ltm.currentTheme.headerURL, "file:///test.png");
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  do_check_eq(ltm.usedThemes.length, 0);

  data = dummy();
  data.headerURL = "ftp://lwtest.invalid/test.png";
  try {
    ltm.setLocalTheme(data);
    do_throw("Should have rejected a theme with a non-http(s), non-file headerURL");
  } catch (e) {
    // Expected exception
  }

  data = dummy();
  delete data.id;
  try {
    ltm.currentTheme = data;
    do_throw("Should have rejected a theme with no ID");
  } catch (e) {
    // Expected exception
  }

  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  // Force the theme into the prefs anyway
  let themes = [data];
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));
  do_check_eq(ltm.usedThemes.length, 1);

  // This should silently drop the bad theme.
  ltm.currentTheme = dummy();
  do_check_eq(ltm.usedThemes.length, 1);
  ltm.forgetUsedTheme(ltm.currentTheme.id);
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  // Add one broken and some working.
  themes = [data, dummy("x1"), dummy("x2")];
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));
  do_check_eq(ltm.usedThemes.length, 3);

  // Switching to an existing theme should drop the bad theme.
  ltm.currentTheme = ltm.getUsedTheme("x1");
  do_check_eq(ltm.usedThemes.length, 2);
  ltm.forgetUsedTheme("x1");
  ltm.forgetUsedTheme("x2");
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(themes));
  do_check_eq(ltm.usedThemes.length, 3);

  // Forgetting an existing theme should drop the bad theme.
  ltm.forgetUsedTheme("x1");
  do_check_eq(ltm.usedThemes.length, 1);
  ltm.forgetUsedTheme("x2");
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

  // Test whether a JSON set with setCharPref can be retrieved with usedThemes
  ltm.currentTheme = dummy("x0");
  ltm.currentTheme = dummy("x1");
  Services.prefs.setCharPref("lightweightThemes.usedThemes", JSON.stringify(ltm.usedThemes));
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.currentTheme.id, "x1");
  do_check_eq(ltm.usedThemes[1].id, "x0");
  do_check_eq(ltm.usedThemes[0].id, "x1");

  ltm.forgetUsedTheme("x0");
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_neq(ltm.currentTheme, null);

  ltm.forgetUsedTheme("x1");
  do_check_eq(ltm.usedThemes.length, 0);
  do_check_eq(ltm.currentTheme, null);

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

  do_check_eq(ltm.usedThemes.length, 30);

  ltm.currentTheme = dummy("x31");

  do_check_eq(ltm.usedThemes.length, 30);
  do_check_eq(ltm.getUsedTheme("x1"), null);

  Services.prefs.setIntPref("lightweightThemes.maxUsedThemes", 15);

  do_check_eq(ltm.usedThemes.length, 15);

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

  do_check_eq(ltm.usedThemes.length, 32);

  ltm.currentTheme = dummy("x33");

  do_check_eq(ltm.usedThemes.length, 32);

  Services.prefs.clearUserPref("lightweightThemes.maxUsedThemes");

  do_check_eq(ltm.usedThemes.length, 30);

  let usedThemes = ltm.usedThemes;
  for (let theme of usedThemes) {
    ltm.forgetUsedTheme(theme.id);
  }

  // Check builtInTheme functionality for Bug 1094821
  do_check_eq(ltm._builtInThemes.toString(), "[object Map]");
  do_check_eq([...ltm._builtInThemes.entries()].length, 0);
  do_check_eq(ltm.usedThemes.length, 0);

  ltm.addBuiltInTheme(dummy("builtInTheme0"));
  do_check_eq([...ltm._builtInThemes].length, 1);
  do_check_eq(ltm.usedThemes.length, 1);
  do_check_eq(ltm.usedThemes[0].id, "builtInTheme0");

  ltm.addBuiltInTheme(dummy("builtInTheme1"));
  do_check_eq([...ltm._builtInThemes].length, 2);
  do_check_eq(ltm.usedThemes.length, 2);
  do_check_eq(ltm.usedThemes[1].id, "builtInTheme1");

  // Clear all and then re-add
  ltm.clearBuiltInThemes();
  do_check_eq([...ltm._builtInThemes].length, 0);
  do_check_eq(ltm.usedThemes.length, 0);

  ltm.addBuiltInTheme(dummy("builtInTheme0"));
  ltm.addBuiltInTheme(dummy("builtInTheme1"));
  do_check_eq([...ltm._builtInThemes].length, 2);
  do_check_eq(ltm.usedThemes.length, 2);

  do_test_pending();

  AddonManager.getAddonByID("builtInTheme0@personas.mozilla.org", builtInThemeAddon => {
    // App specific theme can't be uninstalled or disabled,
    // but can be enabled (since it isn't already applied).
    do_check_eq(hasPermission(builtInThemeAddon, "uninstall"), false);
    do_check_eq(hasPermission(builtInThemeAddon, "disable"), false);
    do_check_eq(hasPermission(builtInThemeAddon, "enable"), true);

    ltm.currentTheme = dummy("x0");
    do_check_eq([...ltm._builtInThemes].length, 2);
    do_check_eq(ltm.usedThemes.length, 3);
    do_check_eq(ltm.usedThemes[0].id, "x0");
    do_check_eq(ltm.currentTheme.id, "x0");
    do_check_eq(ltm.usedThemes[1].id, "builtInTheme0");
    do_check_eq(ltm.usedThemes[2].id, "builtInTheme1");

    Assert.throws(() => { ltm.addBuiltInTheme(dummy("builtInTheme0")); },
                  "Exception is thrown adding a duplicate theme");
    Assert.throws(() => { ltm.addBuiltInTheme("not a theme object"); },
                  "Exception is thrown adding an invalid theme");

    AddonManager.getAddonByID("x0@personas.mozilla.org", x0Addon => {
      // Currently applied (non-app-specific) can be uninstalled or disabled,
      // but can't be enabled (since it's already applied).
      do_check_eq(hasPermission(x0Addon, "uninstall"), true);
      do_check_eq(hasPermission(x0Addon, "disable"), true);
      do_check_eq(hasPermission(x0Addon, "enable"), false);

      ltm.forgetUsedTheme("x0");
      do_check_eq(ltm.currentTheme, null);

      // Removing the currently applied app specific theme should unapply it
      ltm.currentTheme = ltm.getUsedTheme("builtInTheme0");
      do_check_eq(ltm.currentTheme.id, "builtInTheme0");
      do_check_true(ltm.forgetBuiltInTheme("builtInTheme0"));
      do_check_eq(ltm.currentTheme, null);

      do_check_eq([...ltm._builtInThemes].length, 1);
      do_check_eq(ltm.usedThemes.length, 1);

      do_check_true(ltm.forgetBuiltInTheme("builtInTheme1"));
      do_check_false(ltm.forgetBuiltInTheme("not-an-existing-theme-id"));

      do_check_eq([...ltm._builtInThemes].length, 0);
      do_check_eq(ltm.usedThemes.length, 0);
      do_check_eq(ltm.currentTheme, null);

      do_test_finished();
    });
  });
}
