add_task(async function run_test() {
  await do_x64CFITest("CRASH_X64CFI_SAVE_XMM128", [
    { symbol: "CRASH_X64CFI_SAVE_XMM128", trust: "context" },
    { symbol: "CRASH_X64CFI_LAUNCHER", trust: "cfi" },
  ]);
});
