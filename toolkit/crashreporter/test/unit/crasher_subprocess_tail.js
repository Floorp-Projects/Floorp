// now actually crash
let crasher = Components.classes["@mozilla.org/testcrasher;1"].createInstance(Components.interfaces.nsITestCrasher);
crasher.crash(crashType);
