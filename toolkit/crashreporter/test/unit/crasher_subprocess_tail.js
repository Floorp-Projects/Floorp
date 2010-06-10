// now actually crash
let cd = cwd.clone();
cd.append("components");
Components.manager instanceof Components.interfaces.nsIComponentRegistrar;
Components.manager.autoRegister(cd);
let crasher = Components.classes["@mozilla.org/testcrasher;1"].createInstance(Components.interfaces.nsITestCrasher);
crasher.crash(crashType);
