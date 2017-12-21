/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const BIN_DIR = (IS_WIN ? "test_bug473417-ó" : "test_bug473417");
const BIN_EXE = "TestAUSReadStrings" + BIN_SUFFIX;
const tempdir = do_get_tempdir();

function run_test() {
  let workdir = tempdir.clone();
  workdir.append(BIN_DIR);

  let paths = [
    BIN_EXE,
    "TestAUSReadStrings1.ini",
    "TestAUSReadStrings2.ini",
    "TestAUSReadStrings3.ini",
  ];
  for (let i = 0; i < paths.length; i++) {
    let file = do_get_file("../data/" + paths[i]);
    file.copyTo(workdir, null);
  }

  let readStrings = workdir.clone();
  readStrings.append(BIN_EXE);

  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(readStrings);
  process.run(true, [], 0);
  Assert.equal(process.exitValue, 0);
}
