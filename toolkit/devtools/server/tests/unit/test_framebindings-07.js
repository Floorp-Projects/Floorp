/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gDebuggee;
var gClient;
var gThreadClient;

// Test that the EnvironmentClient's getBindings() method works as expected.
function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-bindings");

  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestTabAndResume(gClient, "test-bindings", function(aResponse, aTabClient, aThreadClient) {
      gThreadClient = aThreadClient;
      test_banana_environment();
    });
  });
  do_test_pending();
}

function test_banana_environment()
{

  gThreadClient.addOneTimeListener("paused", function(aEvent, aPacket) {
    let environment = aPacket.frame.environment;
    do_check_eq(environment.type, "function");

    let parent = environment.parent;
    do_check_eq(parent.type, "block");

    let grandpa = parent.parent;
    do_check_eq(grandpa.type, "function");

    let envClient = gThreadClient.environment(environment);
    envClient.getBindings(aResponse => {
      do_check_eq(aResponse.bindings.arguments[0].z.value, "z");

      let parentClient = gThreadClient.environment(parent);
      parentClient.getBindings(aResponse => {
        do_check_eq(aResponse.bindings.variables.banana3.value.class, "Function");

        let grandpaClient = gThreadClient.environment(grandpa);
        grandpaClient.getBindings(aResponse => {
          do_check_eq(aResponse.bindings.arguments[0].y.value, "y");
          gThreadClient.resume(() => finishClient(gClient));
        });
      });
    });
  });

  gDebuggee.eval("\
        function banana(x) {                                            \n\
          return function banana2(y) {                                  \n\
            return function banana3(z) {                                \n\
              debugger;                                                 \n\
            };                                                          \n\
          };                                                            \n\
        }                                                               \n\
        banana('x')('y')('z');                                          \n\
        ");
}
