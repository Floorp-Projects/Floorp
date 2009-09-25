const CWD = do_get_cwd();
function checkOS(os) {
  const nsILocalFile_ = "nsILocalFile" + os;
  return nsILocalFile_ in Components.interfaces &&
         CWD instanceof Components.interfaces[nsILocalFile_];
}

const DIR_TARGET     = "target";
const DIR_LINK       = "link";
const DIR_LINK_LINK  = "link_link";
const FILE_TARGET    = "target.txt";
const FILE_LINK      = "link.txt";
const FILE_LINK_LINK = "link_link.txt";

const DOES_NOT_EXIST = "doesnotexist";
const DANGLING_LINK  = "dangling_link";
const LOOP_LINK      = "loop_link";

const isWin = checkOS("Win");
const isOS2 = checkOS("OS2");
const isMac = checkOS("Mac");
const isUnix = !(isWin || isOS2 || isMac);

const nsIFile = Components.interfaces.nsIFile;

var process;
function createSymLink(from, to) {
  if (!process) {
    var ln = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
    ln.initWithPath("/bin/ln");

    process = Components.classes["@mozilla.org/process/util;1"]
                        .createInstance(Components.interfaces.nsIProcess);
    process.init(ln);
  }

  const args = ["-s", from, to];
  process.run(true, args, args.length);
  do_check_eq(process.exitValue, 0);
}

function makeSymLink(from, toName, relative) {
  var to = from.parent;
  to.append(toName);

  if (relative) {
    createSymLink(from.leafName, to.path);
  }
  else {
    createSymLink(from.path, to.path);
  }

  do_check_true(to.isSymlink());

  print("---");
  print(from.path);
  print(to.path);
  print(to.target);

  if (from.leafName != DOES_NOT_EXIST && from.isSymlink()) {
    // XXXjag wish I could set followLinks to false so we'd just get
    // the symlink's direct target instead of the final target.
    do_check_eq(from.target, to.target);
  }
  else {
    do_check_eq(from.path, to.target);
  }

  return to;
}

function setupTestDir(testDir, relative) {
  var targetDir = testDir.clone();
  targetDir.append(DIR_TARGET);

  if (testDir.exists()) {
    testDir.remove(true);
  }
  do_check_true(!testDir.exists());

  testDir.create(nsIFile.DIRECTORY_TYPE, 0777);

  targetDir.create(nsIFile.DIRECTORY_TYPE, 0777);

  var targetFile = testDir.clone();
  targetFile.append(FILE_TARGET);
  targetFile.create(nsIFile.NORMAL_FILE_TYPE, 0666);

  var imaginary = testDir.clone();
  imaginary.append(DOES_NOT_EXIST);

  var loop = testDir.clone();
  loop.append(LOOP_LINK);

  var dirLink  = makeSymLink(targetDir, DIR_LINK, relative);
  var fileLink = makeSymLink(targetFile, FILE_LINK, relative);

  makeSymLink(dirLink, DIR_LINK_LINK, relative);
  makeSymLink(fileLink, FILE_LINK_LINK, relative);

  makeSymLink(imaginary, DANGLING_LINK, relative);

  try {
    makeSymLink(loop, LOOP_LINK, relative);
    do_check_true(false);
  }
  catch (e) {
  }
}

function createSpaces(dirs, files, links) {
  function longest(a, b) a.length > b.length ? a : b;
  return dirs.concat(files, links).reduce(longest, "").replace(/./g, " ");
}

function testSymLinks(testDir, relative) {
  setupTestDir(testDir, relative);

  const dirLinks   = [DIR_LINK, DIR_LINK_LINK];
  const fileLinks  = [FILE_LINK, FILE_LINK_LINK];
  const otherLinks = [DANGLING_LINK, LOOP_LINK];
  const dirs  = [DIR_TARGET].concat(dirLinks);
  const files = [FILE_TARGET].concat(fileLinks);
  const links = otherLinks.concat(dirLinks, fileLinks);

  const spaces = createSpaces(dirs, files, links);
  const bools = {false: " false", true: " true "};
  print(spaces + " dir   file  symlink");
  var dirEntries = testDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    const file = dirEntries.getNext().QueryInterface(nsIFile);
    const name = file.leafName;
    print(name + spaces.substring(name.length) + bools[file.isDirectory()] +
          bools[file.isFile()] + bools[file.isSymlink()]);
    do_check_eq(file.isDirectory(), dirs.indexOf(name) != -1);
    do_check_eq(file.isFile(), files.indexOf(name) != -1);
    do_check_eq(file.isSymlink(), links.indexOf(name) != -1);
  }
}

function run_test() {
  // Skip this test on Windows
  if (isWin || isOS2)
    return;

  var testDir = CWD;
  testDir.append("test_symlinks");

  testSymLinks(testDir, false);
  testSymLinks(testDir, true);
}
