/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that an actor will not be reused after a release or
 * releaseMany.
 */

var gDebuggee;
var gClient;
var gThreadClient;

function run_test()
{
  initTestDebuggerServer();
  gDebuggee = addTestGlobal("test-grips");
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect(function() {
    attachTestGlobalClientAndResume(gClient, "test-grips", function(aResponse, aThreadClient) {
      gThreadClient = aThreadClient;
      test_thread_lifetime();
    });
  });
  do_test_pending();
}

function arg_grips(aFrameArgs, aOnResponse) {
  let grips = [];
  let handler = function (aResponse) {
    grips.push(aResponse.threadGrip);
    if (grips.length == aFrameArgs.length) {
      aOnResponse(grips);
    }
  };
  for (let i = 0; i < aFrameArgs.length; i++) {
    gClient.request({ to: aFrameArgs[i].actor, type: "threadGrip" },
                    handler);
  }
}

function test_thread_lifetime()
{
  // Get three thread-lifetime grips.
  gThreadClient.addOneTimeListener("paused", function (aEvent, aPacket) {

    let frameArgs = aPacket.frame.arguments;
    arg_grips(frameArgs, function (aGrips) {
      release_grips(frameArgs, aGrips);
    });
  });

  gDebuggee.eval("(" + function() {
    function stopMe(arg1, arg2, arg3) {
      debugger;
    };
    stopMe({obj: 1}, {obj: 2}, {obj: 3});
    ")"
  } + ")()");
}


function release_grips(aFrameArgs, aThreadGrips)
{
  // Release the first grip with release, and the second two with releaseMany...

  gClient.request({ to: aThreadGrips[0].actor, type: "release" }, function (aResponse) {
    let release = [aThreadGrips[1].actor, aThreadGrips[2].actor];
    gClient.request({ to: gThreadClient.actor, type: "releaseMany", "actors": release }, function (aResponse) {
      // Now ask for thread grips again, they should be different.
      arg_grips(aFrameArgs, function (aNewGrips) {
        for (let i = 0; i < aNewGrips.length; i++) {
          do_check_neq(aThreadGrips[i].actor, aNewGrips[i].actor);
        }
        gThreadClient.resume(function () {
          finishClient(gClient);
        });
      });
    });
  });
}
