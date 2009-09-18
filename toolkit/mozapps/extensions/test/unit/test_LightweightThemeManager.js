function dummy(id) {
  return {
    id: id,
    name: Math.random(),
    headerURL: Math.random(),
    footerURL: Math.random(),
    textColor: Math.random(),
    dominantColor: Math.random()
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
}
