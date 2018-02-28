function run_test() {
    var cmdLine = Cc["@mozilla.org/toolkit/command-line;1"].createInstance(Ci.nsICommandLine);
    try {
        cmdLine.getArgument(cmdLine.length);
    } catch (e) {}
}
