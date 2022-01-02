add_task(async function run_test() {
  // Test that minidump-analyzer gracefully handles an invalid pointer to the
  // exception unwind information.
  let exe = do_get_file("test_crash_win64cfi_invalid_exception_rva.exe");
  ok(exe);

  // Perform a crash. The PE used for unwind info should fail, resulting in
  // fallback behavior, calculating the first frame from thread context.
  // Further frames would be calculated with either frame_pointer or scan trust,
  // but should not be calculated via CFI. If we see CFI here that would be an
  // indication that either our alternative EXE was not used, or we failed to
  // abandon unwind info parsing.
  await do_x64CFITest(
    "CRASH_X64CFI_ALLOC_SMALL",
    [
      { symbol: "CRASH_X64CFI_ALLOC_SMALL", trust: "context" },
      { symbol: null, trust: "!cfi" },
    ],
    ["--force-use-module", exe.path]
  );
});
