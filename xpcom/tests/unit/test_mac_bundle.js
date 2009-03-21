function run_test() { 
  // this is a hack to skip the rest of the code on non-Mac platforms, 
  // since #ifdef is not available to xpcshell tests...
  if (!("nsILocalFileMac" in Components.interfaces))
    return;
  
  // OK, here's the real part of the test:
  // make sure these two test bundles are recognized as bundles (or "packages") 
  var keynoteBundle = do_get_file("data/presentation.key");
  var appBundle = do_get_file("data/SmallApp.app");
  
  do_check_true(keynoteBundle instanceof Components.interfaces.nsILocalFileMac);
  do_check_true(appBundle instanceof Components.interfaces.nsILocalFileMac);
  
  do_check_true(keynoteBundle.isPackage());
  do_check_true(appBundle.isPackage());
}
