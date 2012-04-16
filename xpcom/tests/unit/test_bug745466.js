Components.utils.import("resource://gre/modules/FileUtils.jsm");

function run_test()
{
  do_check_true(FileUtils.File("~").equals(FileUtils.getDir("Home", [])));
}
