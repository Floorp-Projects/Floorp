function run_test() {
  var cmdLine = Cu.createCommandLine();
  try {
    cmdLine.getArgument(cmdLine.length);
  } catch (e) {}
}
