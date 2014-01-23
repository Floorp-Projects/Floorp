/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that reattaching to a previously detached thread works.
 */

var gClient, gDebuggee, gThreadClient, gTabClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = testGlobal("test-reattach");
  DebuggerServer.addTestGlobal(gDebuggee);

  let transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect(() => {
    attachTestTab(gClient, "test-reattach", (aReply, aTabClient) => {
      gTabClient = aTabClient;
      test_attach();
    });
  });
  do_test_pending();
}

function test_attach()
{
  gTabClient.attachThread({}, (aResponse, aThreadClient) => {
    do_check_eq(aThreadClient.state, "paused");
    gThreadClient = aThreadClient;
    aThreadClient.resume(test_detach);
  });
}

function test_detach()
{
  gThreadClient.detach(() => {
    do_check_eq(gThreadClient.state, "detached");
    do_check_eq(gTabClient.thread, null);
    test_reattach();
  });
}

function test_reattach()
{
  gTabClient.attachThread({}, (aResponse, aThreadClient) => {
    do_check_neq(gThreadClient, aThreadClient);
    do_check_eq(aThreadClient.state, "paused");
    do_check_eq(gTabClient.thread, aThreadClient);
    aThreadClient.resume(cleanup);
  });
}

function cleanup()
{
  gClient.close(do_test_finished);
}
