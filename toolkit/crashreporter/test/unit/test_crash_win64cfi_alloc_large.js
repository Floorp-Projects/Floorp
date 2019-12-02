add_task(async function run_test() {
  await do_x64CFITest("CRASH_X64CFI_ALLOC_LARGE", [
    { symbol: "CRASH_X64CFI_ALLOC_LARGE", trust: "context" },
    { symbol: "CRASH_X64CFI_LAUNCHER", trust: "cfi" },
  ]);
});
