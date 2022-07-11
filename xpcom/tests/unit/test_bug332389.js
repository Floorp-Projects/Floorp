function run_test() {
  var f = Services.dirsvc.get("CurProcD", Ci.nsIFile);

  var terminated = false;
  for (var i = 0; i < 100; i++) {
    if (f == null) {
      terminated = true;
      break;
    }
    f = f.parent;
  }

  Assert.ok(terminated);
}
