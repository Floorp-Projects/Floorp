/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { getSingleton, getJsRefcount } = ChromeUtils.importESModule(
  "resource://gre/modules/RustRefcounts.sys.mjs"
);

// Test refcounts when we call methods.
//
// Each method call requires that we clone the Arc pointer on the JS side, then pass it to Rust
// which will consumer the reference.  Make sure we get this right

function createObjectAndCallMethods() {
  const obj = getSingleton();
  obj.method();
}

add_test(() => {
  // Create an object that we'll keep around.  If the ref count ends up being low, we don't want
  // to reduce it below 0, since the Rust code may catch that and clamp it
  const obj = getSingleton();
  createObjectAndCallMethods();
  Cu.forceGC();
  Cu.forceCC();
  do_test_pending();
  do_timeout(500, () => {
    Assert.equal(getJsRefcount(), 1);
    // Use `obj` to avoid unused warnings and try to ensure that JS doesn't destroy it early
    obj.method();
    do_test_finished();
    run_next_test();
  });
});

// Test refcounts when creating/destroying objects
function createAndDeleteObjects() {
  [getSingleton(), getSingleton(), getSingleton()];
}

add_test(() => {
  const obj = getSingleton();
  createAndDeleteObjects();
  Cu.forceGC();
  Cu.forceCC();
  do_timeout(500, () => {
    Assert.equal(getJsRefcount(), 1);
    obj.method();
    do_test_finished();
    run_next_test();
  });
});

// As we implement more UniFFI features we should probably add refcount tests for it.
// Some features that should probably have tests:
//   - Async methods
//   - UniFFI builtin trait methods like 'to_string'
//   - Rust trait objects
