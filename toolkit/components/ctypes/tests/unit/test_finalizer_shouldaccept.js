try {
  // We might be running without privileges, in which case it's up to the
  // harness to give us the 'ctypes' object.
  Components.utils.import("resource://gre/modules/ctypes.jsm");
} catch(e) {
}

let acquire, dispose, reset_errno, dispose_errno;

function run_test()
{
  let library = open_ctypes_test_lib();

  let start = library.declare("test_finalizer_start", ctypes.default_abi,
                          ctypes.void_t,
                          ctypes.size_t);
  let stop = library.declare("test_finalizer_stop", ctypes.default_abi,
                             ctypes.void_t);
  let tester = new ResourceTester(start, stop);
  acquire = library.declare("test_finalizer_acq_size_t",
                            ctypes.default_abi,
                            ctypes.size_t,
                            ctypes.size_t);
  dispose = library.declare("test_finalizer_rel_size_t",
                            ctypes.default_abi,
                            ctypes.void_t,
                            ctypes.size_t);
  reset_errno = library.declare("reset_errno",
                                ctypes.default_abi,
                                ctypes.void_t);
  dispose_errno = library.declare("test_finalizer_rel_size_t_set_errno",
                                  ctypes.default_abi,
                                  ctypes.void_t,
                                  ctypes.size_t);
  tester.launch(10, test_to_string);
  tester.launch(10, test_to_source);
  tester.launch(10, test_to_int);
  tester.launch(10, test_errno);
}

/**
 * Check that toString succeeds before/after forget/dispose.
 */
function test_to_string()
{
  do_print("Starting test_to_string");
  let a = ctypes.CDataFinalizer(acquire(0), dispose);
  do_check_eq(a.toString(), "0");

  a.forget();
  do_check_eq(a.toString(), "[CDataFinalizer - empty]");

  a = ctypes.CDataFinalizer(acquire(0), dispose);
  a.dispose();
  do_check_eq(a.toString(), "[CDataFinalizer - empty]");
}

/**
 * Check that toSource succeeds before/after forget/dispose.
 */
function test_to_source()
{
  do_print("Starting test_to_source");
  let value = acquire(0);
  let a = ctypes.CDataFinalizer(value, dispose);
  do_check_eq(a.toSource(),
              "ctypes.CDataFinalizer("
              + ctypes.size_t(value).toSource()
              +", "
              +dispose.toSource()
              +")");
  value = null;

  a.forget();
  do_check_eq(a.toSource(), "ctypes.CDataFinalizer()");

  a = ctypes.CDataFinalizer(acquire(0), dispose);
  a.dispose();
  do_check_eq(a.toSource(), "ctypes.CDataFinalizer()");
}

/**
 * Test conversion to int32
 */
function test_to_int()
{
  let value = 2;
  let wrapped, converted, finalizable;
  wrapped   = ctypes.int32_t(value);
  finalizable= ctypes.CDataFinalizer(acquire(value), dispose);
  converted = ctypes.int32_t(finalizable);

  structural_check_eq(converted, wrapped);
  structural_check_eq(converted, ctypes.int32_t(finalizable.forget()));

  wrapped   = ctypes.int64_t(value);
  converted = ctypes.int64_t(ctypes.CDataFinalizer(acquire(value),
                                                   dispose));
  structural_check_eq(converted, wrapped);
}

/**
 * Test that dispose can change errno but finalization cannot
 */
function test_errno(size)
{
  reset_errno();
  do_check_eq(ctypes.errno, 0);

  let finalizable = ctypes.CDataFinalizer(acquire(3), dispose_errno);
  finalizable.dispose();
  do_check_eq(ctypes.errno, 10);
  reset_errno();

  do_check_eq(ctypes.errno, 0);
  for (let i = 0; i < size; ++i) {
    finalizable = ctypes.CDataFinalizer(acquire(i), dispose_errno);
  }

  trigger_gc();
  do_check_eq(ctypes.errno, 0);
}