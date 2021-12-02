function run_test() {
  var cmdLine = Cu.createCommandLine(
    [],
    null,
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );
  try {
    cmdLine.getArgument(cmdLine.length);
  } catch (e) {}
}
