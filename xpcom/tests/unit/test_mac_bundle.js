function run_test() {
  // this is a hack to skip the rest of the code on non-Mac platforms,
  // since #ifdef is not available to xpcshell tests...
  if (mozinfo.os != "mac") {
    return;
  }

  // OK, here's the real part of the test:
  // make sure these two test bundles are recognized as bundles (or "packages")
  var keynoteBundle = do_get_file("data/presentation.key");
  var appBundle = do_get_file("data/SmallApp.app");

  Assert.ok(keynoteBundle instanceof Components.interfaces.nsILocalFileMac);
  Assert.ok(appBundle instanceof Components.interfaces.nsILocalFileMac);

  Assert.ok(keynoteBundle.isPackage());
  Assert.ok(appBundle.isPackage());
}
