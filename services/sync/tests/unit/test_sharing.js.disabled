Cu.import("resource://weave/sharing.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/identity.js");

function runTestGenerator() {
  let self = yield;

  ID.set("blarg", new Identity("realm", "myusername", "mypass"));
  let fakeDav = {
    identity: "blarg",
    POST: function fakeDav_POST(url, data, callback) {
      do_check_true(data.indexOf("uid=myusername") != -1);
      do_check_true(data.indexOf("password=mypass") != -1);
      do_check_true(data.indexOf("/fake/dir") != -1);
      do_check_true(data.indexOf("johndoe") != -1);
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
