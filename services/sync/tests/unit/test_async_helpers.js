Cu.import("resource://services-sync/async.js");

function chain(fs) {
  fs.reduce(function (prev, next) next.bind(this, prev),
            run_next_test)();
}

// barrieredCallbacks.
add_test(function test_barrieredCallbacks() {
  let s1called = false;
  let s2called = false;

  function reset() {
    _(" > reset.");
    s1called = s2called = false;
  }
  function succeed1(err, result) {
    _(" > succeed1.");
    s1called = true;
  }
  function succeed2(err, result) {
    _(" > succeed2.");
    s2called = true;
  }
  function fail1(err, result) {
    _(" > fail1.");
    return "failed";
  }
  function throw1(err, result) {
    _(" > throw1.");
    throw "Aieeee!";
  }

  function doneSequential(next, err) {
    _(" > doneSequential.");
    do_check_eq(err, "failed");
    do_check_true(s1called);
    do_check_true(s2called);
    next();
  }
  function doneFailFirst(next, err) {
    _(" > doneFailFirst.");
    do_check_eq(err, "failed");
    do_check_false(s1called);
    do_check_false(s2called);
    next();
  }
  function doneOnlySucceed(next, err) {
    _(" > doneOnlySucceed.");
    do_check_true(!err);
    do_check_true(s1called);
    do_check_true(s2called);
    next();
  }
  function doneThrow(next, err) {
    _(" > doneThrow.");
    do_check_eq(err, "Aieeee!");
    do_check_true(s1called);
    do_check_false(s2called);
    next();
  }

  function sequence_test(label, parts, end) {
    return function (next) {
      _("Sequence test '" + label + "':");
      reset();
      for (let cb in Async.barrieredCallbacks(parts, end.bind(this, next)))
        cb();
    };
  }

  chain(
    [sequence_test("failFirst",
                   [fail1, succeed1, succeed2],
                   doneFailFirst),

     sequence_test("sequentially",
                   [succeed1, succeed2, fail1],
                   doneSequential),

     sequence_test("onlySucceed",
                   [succeed1, succeed2],
                   doneOnlySucceed),

     sequence_test("throw",
                   [succeed1, throw1, succeed2],
                   doneThrow)]);

});

add_test(function test_empty_barrieredCallbacks() {
  let err;
  try {
    Async.barrieredCallbacks([], function (err) { }).next();
  } catch (ex) {
    err = ex;
  }
  _("err is " + err);
  do_check_true(err instanceof StopIteration);
  run_next_test();
});

add_test(function test_no_output_barrieredCallbacks() {
  let err;
  try {
    Async.barrieredCallbacks([function (x) {}], null);
  } catch (ex) {
    err = ex;
  }
  do_check_eq(err, "No output callback provided to barrieredCallbacks.");
  run_next_test();
});

add_test(function test_serially() {
  let called = {};
  let i      = 1;
  function reset() {
    called = {};
    i      = 0;
  }

  function f(x, cb) {
    called[x] = ++i;
    cb(null, x);
  }

  function err_on(expected) {
    return function (err, result, context) {
      if (err) {
        return err;
      }
      if (result == expected) {
        return expected;
      }
      _("Got " + result + ", passing.");
    };
  }

  // Fail in the middle.
  reset();
  Async.serially(["a", "b", "d"], f, err_on("b"), function (err) {
    do_check_eq(1, called["a"]);
    do_check_eq(2, called["b"]);
    do_check_false(!!called["d"]);
    do_check_eq(err, "b");

  // Don't fail.
  reset();
  Async.serially(["a", "d", "b"], f, err_on("x"), function (err) {
    do_check_eq(1, called["a"]);
    do_check_eq(3, called["b"]);
    do_check_eq(2, called["d"]);
    do_check_false(!!err);

  // Empty inputs.
  reset();
  Async.serially([], f, err_on("a"), function (err) {
    do_check_false(!!err);

  reset();
  Async.serially(undefined, f, err_on("a"), function (err) {
    do_check_false(!!err);
    run_next_test();
  });
  });
  });
  });
});

add_test(function test_countedCallback() {
  let error   = null;
  let output  = null;
  let context = null;
  let counter = 0;
  function cb(err, result, ctx) {
    counter++;
    output  = result;
    error   = err;
    context = ctx;
    if (err == "error!")
      return "Oh dear.";
  }

  let c1;

  c1 = Async.countedCallback(cb, 3, function (err) {
    do_check_eq(2, counter);
    do_check_eq("error!", error);
    do_check_eq(2, output);
    do_check_eq("b", context);
    do_check_eq(err, "Oh dear.");

    // If we call the counted callback again (once this output function is
    // done, that is), then the component callback is not invoked.
    Utils.nextTick(function () {
      _("Don't expect component callback.");
      c1("not", "running", "now");
      do_check_eq(2, counter);
      do_check_eq("error!", error);
      do_check_eq(2, output);
      do_check_eq("b", context);
      run_next_test();
    });
  });

  c1(1, "foo", "a");
  do_check_eq(1, counter);
  do_check_eq(1, error);
  do_check_eq("foo", output);
  do_check_eq("a", context);

  c1("error!", 2, "b");
  // Subsequent checks must now take place inside the 'done' callback... read
  // above!
});

add_test(function test_finallyCallback() {
  let fnCalled = false;
  let cbCalled = false;
  let error    = undefined;

  function reset() {
    fnCalled = cbCalled = false;
    error = undefined;
  }

  function fn(arg) {
    do_check_false(!!arg);
    fnCalled = true;
  }

  function fnThrow(arg) {
    do_check_false(!!arg);
    fnCalled = true;
    throw "Foo";
  }

  function cb(next, err) {
    _("Called with " + err);
    cbCalled = true;
    error  = err;
    next();
  }

  function allGood(next) {
    reset();
    let callback = cb.bind(this, function() {
      do_check_true(fnCalled);
      do_check_true(cbCalled);
      do_check_false(!!error);
      next();
    });
    Async.finallyCallback(callback, fn)(null);
  }

  function inboundErr(next) {
    reset();
    let callback = cb.bind(this, function() {
      do_check_true(fnCalled);
      do_check_true(cbCalled);
      do_check_eq(error, "Baz");
      next();
    });
    Async.finallyCallback(callback, fn)("Baz");
  }

  function throwsNoErr(next) {
    reset();
    let callback = cb.bind(this, function() {
      do_check_true(fnCalled);
      do_check_true(cbCalled);
      do_check_eq(error, "Foo");
      next();
    });
    Async.finallyCallback(callback, fnThrow)(null);
  }

  function throwsOverrulesErr(next) {
    reset();
    let callback = cb.bind(this, function() {
      do_check_true(fnCalled);
      do_check_true(cbCalled);
      do_check_eq(error, "Foo");
      next();
    });
    Async.finallyCallback(callback, fnThrow)("Bar");
  }

  chain([throwsOverrulesErr,
         throwsNoErr,
         inboundErr,
         allGood]);
});

function run_test() {
  run_next_test();
}
