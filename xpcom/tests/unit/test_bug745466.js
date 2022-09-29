const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);

function run_test() {
  Assert.ok(FileUtils.File("~").equals(FileUtils.getDir("Home", [])));
}
