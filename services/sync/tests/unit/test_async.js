Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");

function run_test() {
  var fts = new FakeTimerService();

  Function.prototype.async = Async.sugar;

  var onCompleteCalled = false;

  function onComplete() {
    onCompleteCalled = true;
  }

  let timesYielded = 0;

  function testAsyncFunc(x) {
    let self = yield;
    timesYielded++;

    // Ensure that argument was passed in properly.
    do_check_eq(x, 5);

    // Ensure that 'this' is set properly.
    do_check_eq(this.sampleProperty, true);

    fts.makeTimerForCall(self.cb);
    yield;

    timesYielded++;
    self.done();
  }

  var thisArg = {sampleProperty: true};
  testAsyncFunc.async(thisArg, onComplete, 5);

  do_check_eq(timesYielded, 1);

  do_check_true(fts.processCallback());

  do_check_eq(timesYielded, 2);

  do_check_true(fts.processCallback());

  do_check_false(fts.processCallback());
}
