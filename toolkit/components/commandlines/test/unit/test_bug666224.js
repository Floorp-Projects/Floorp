function run_test() {
    var cmdLine=Components.classes["@mozilla.org/toolkit/command-line;1"].createInstance(Components.interfaces.nsICommandLine);
    try {
        cmdLine.getArgument(cmdLine.length);
    } catch(e) {}
}
