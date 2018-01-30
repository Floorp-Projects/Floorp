Components.utils.import("resource://gre/modules/FileUtils.jsm");

function run_test() {
  Assert.ok(FileUtils.File("~").equals(FileUtils.getDir("Home", [])));
}
