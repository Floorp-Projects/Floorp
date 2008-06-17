Cu.import("resource://weave/sharing.js");
Cu.import("resource://weave/util.js");

function runTestGenerator() {
  let self = yield;

  let fakeDav = {
    POST: function fakeDav_POST(url, data, callback) {
      let result = {status: 200, responseText: "OK"};
      Utils.makeTimerForCall(function() { callback(result); });
    }
  };

  let api = new Sharing.Api(fakeDav);
  api.shareWithUsers("/fake/dir", ["johndoe"], self.cb);
  let result = yield;

  do_check_eq(result.wasSuccessful, true);
  self.done();
}

var run_test = makeAsyncTestRunner(runTestGenerator);
