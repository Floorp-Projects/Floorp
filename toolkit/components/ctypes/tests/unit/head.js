try {
  // We might be running without privileges, in which case it's up to the
  // harness to give us the 'ctypes' object.
  Components.utils.import("resource://gre/modules/ctypes.jsm");
} catch(e) {
}

function open_ctypes_test_lib()
{
  return ctypes.open(do_get_file(ctypes.libraryName("jsctypes-test")).path);
}

/**
 * Simple wrapper for tests that require cleanup.
 */
function ResourceTester(start, stop) {
  this._start = start;
  this._stop  = stop;
}
ResourceTester.prototype = {
  launch: function(size, test, args) {
    Components.utils.forceGC();
    this._start(size);
    try {
      test(size, args);
    } catch (x) {
      this._stop();
      throw x;
    }
    Components.utils.forceGC();
    this._stop();
  }
};

function structural_check_eq(a, b) {
  // 1. If objects can be "toSource()-ed", use this.

  let result;
  let finished = false;
  let asource, bsource;
  try {
    asource = a.toSource();
    bsource = b.toSource();
    finished = true;
  } catch (x) {
  }
  if (finished) {
    return do_check_eq(asource, bsource);
  }

  // 2. Otherwise, perform slower comparison

  try {
    structural_check_eq_aux(a, b);
    result = true;
  } catch (x) {
    dump(x);
    result = false;
  }
  do_check_true(result);
}
function structural_check_eq_aux(a, b) {
  let ak;
  try {
    ak = Object.keys(a);
  } catch (x) {
    if (a != b) {
      throw new Error("Distinct values "+a, b);
    }
    return;
  }
  ak.forEach(
    function(k) {
      let av = a[k];
      let bv = b[k];
      structural_check_eq_aux(av, bv);
    }
  );
}

function trigger_gc() {
  dump("Triggering garbage-collection");
  Components.utils.forceGC();
}

function must_throw(f) {
  let has_thrown = false;
  try {
    f();
  } catch (x) {
    has_thrown = true;
  }
  do_check_true(has_thrown);
}
