const MANDATORY = ["id", "name", "headerURL"];
const OPTIONAL = ["footerURL", "textcolor", "accentcolor", "iconURL",
                  "previewURL", "author", "description", "homepageURL",
                  "updateURL", "version"];

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

function run_test() {
  var temp = {};
  Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm", temp);
  do_check_eq(typeof temp.LightweightThemeManager, "object");

  var ltm = temp.LightweightThemeManager;

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

  ltm.forgetUsedTheme("nonexisting");
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

  data = dummy();
  data.unknownProperty = "Foo";
  do_check_eq(typeof roundtrip(data).unknownProperty, "undefined");

  data = dummy();
  data.unknownURL = "http://lwttest.invalid/";
  do_check_eq(typeof roundtrip(data).unknownURL, "undefined");

  function roundtripSet(props, modify, test, secure) {
    props.forEach(function (prop) {
      var data = dummy();
      modify(data, prop);
      test(roundtrip(data, secure), prop, data);
    });
  }

  roundtripSet(MANDATORY, function (data, prop) {
    delete data[prop];
  }, function (after) {
    do_check_eq(after, null);
  });

  roundtripSet(OPTIONAL, function (data, prop) {
    delete data[prop];
  }, function (after) {
    do_check_neq(after, null);
  });

  roundtripSet(MANDATORY, function (data, prop) {
    data[prop] = "";
  }, function (after) {
    do_check_eq(after, null);
  });

  roundtripSet(OPTIONAL, function (data, prop) {
    data[prop] = "";
  }, function (after, prop) {
    do_check_eq(typeof after[prop], "undefined");
  });

  roundtripSet(MANDATORY, function (data, prop) {
    data[prop] = " ";
  }, function (after) {
    do_check_eq(after, null);
  });

  roundtripSet(OPTIONAL, function (data, prop) {
    data[prop] = " ";
  }, function (after, prop) {
    do_check_neq(after, null);
    do_check_eq(typeof after[prop], "undefined");
  });

  function non_urls(props) {
    return props.filter(function (prop) !/URL$/.test(prop));
  }

  function urls(props) {
    return props.filter(function (prop) /URL$/.test(prop));
  }

  roundtripSet(non_urls(MANDATORY.concat(OPTIONAL)), function (data, prop) {
    data[prop] = prop;
  }, function (after, prop, before) {
    do_check_eq(after[prop], before[prop]);
  });

  roundtripSet(non_urls(MANDATORY.concat(OPTIONAL)), function (data, prop) {
    data[prop] = " " + prop + "  ";
  }, function (after, prop, before) {
    do_check_eq(after[prop], before[prop].trim());
  });

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function (data, prop) {
    data[prop] = Math.random().toString();
  }, function (after, prop, before) {
    if (prop == "updateURL")
      do_check_eq(typeof after[prop], "undefined");
    else
      do_check_eq(after[prop], "http://lwttest.invalid/" + before[prop]);
  });

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function (data, prop) {
    data[prop] = Math.random().toString();
  }, function (after, prop, before) {
    do_check_eq(after[prop], "https://lwttest.invalid/" + before[prop]);
  }, true);

  roundtripSet(urls(MANDATORY.concat(OPTIONAL)), function (data, prop) {
    data[prop] = "https://sub.lwttest.invalid/" + Math.random().toString();
  }, function (after, prop, before) {
    do_check_eq(after[prop], before[prop]);
  });

  roundtripSet(urls(MANDATORY), function (data, prop) {
    data[prop] = "ftp://lwttest.invalid/" + Math.random().toString();
  }, function (after) {
    do_check_eq(after, null);
  });

  roundtripSet(urls(OPTIONAL), function (data, prop) {
    data[prop] = "ftp://lwttest.invalid/" + Math.random().toString();
  }, function (after, prop) {
    do_check_eq(typeof after[prop], "undefined");
  });
}
