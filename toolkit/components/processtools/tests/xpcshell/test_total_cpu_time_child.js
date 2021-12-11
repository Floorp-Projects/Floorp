const MESSAGE_CHILD_TEST_DONE = "ChildTest:Done";

function run_test() {
  const kBusyWaitForMs = 50;
  let startTime = Date.now();
  while (Date.now() - startTime < kBusyWaitForMs) {
    // Burn CPU time...
  }
  do_send_remote_message(MESSAGE_CHILD_TEST_DONE, kBusyWaitForMs);
}
