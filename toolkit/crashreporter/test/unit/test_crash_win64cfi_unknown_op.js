add_task(async function run_test() {
  // In the case of an unknown unwind code or missing CFI,
  // make certain we can still walk the stack via stack scan. The crashing
  // function places NO_MANS_LAND on the stack so it will get picked up via
  // stack scan.
  await do_x64CFITest("CRASH_X64CFI_UNKNOWN_OPCODE", [
    { symbol: "CRASH_X64CFI_UNKNOWN_OPCODE", trust: "context" },
    // Trust may either be scan or frame_pointer; we don't really care as
    // long as the address is expected.
    { symbol: "CRASH_X64CFI_NO_MANS_LAND", trust: null },
  ]);
});
