function run_test() {
  do_x64CFITest("CRASH_X64CFI_SAVE_XMM128_FAR", [
    { symbol: "CRASH_X64CFI_SAVE_XMM128_FAR", trust: "context" },
    { symbol: "CRASH_X64CFI_LAUNCHER", trust: "cfi" },
  ]);
}
