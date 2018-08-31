
function run_test() {
  do_x64CFITest("CRASH_X64CFI_EPILOG", [
      { symbol: "CRASH_X64CFI_EPILOG", trust: "context" },
      { symbol: "CRASH_X64CFI_LAUNCHER", trust: "cfi" },
    ]);
}
