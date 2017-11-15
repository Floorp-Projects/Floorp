
function run_test() {
  // Test that minidump-analyzer gracefully handles an invalid pointer to the
  // exception unwind information.
  let exe = do_get_file("test_crash_win64cfi_invalid_exception_rva.exe");
  ok(exe);

  // Perform a crash. We won't get unwind info, but make sure the stack scan
  // still works.
  do_x64CFITest("CRASH_X64CFI_ALLOC_SMALL",
    [
      { symbol: "CRASH_X64CFI_ALLOC_SMALL", trust: "context" },
      { symbol: "CRASH_X64CFI_NO_MANS_LAND", trust: null }
    ],
    ["--force-use-module", exe.path]);
}
