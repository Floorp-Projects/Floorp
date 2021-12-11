/* -*-  indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable no-new-wrappers */

try {
  Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
  });
} catch (e) {}

const CTYPES_TEST_LIB = ctypes.libraryName("jsctypes-test");
const CTYPES_UNICODE_LIB = ctypes.libraryName("jsctyp\u00E8s-t\u00EB\u00DFt");

function do_check_throws(f, type, stack) {
  if (!stack) {
    try {
      // We might not have a 'Components' object.
      stack = Components.stack.caller;
    } catch (e) {}
  }

  try {
    f();
  } catch (exc) {
    if (exc.constructor.name === type.name) {
      Assert.ok(true);
      return;
    }
    do_throw("expected " + type.name + " exception, caught " + exc, stack);
  }
  do_throw("expected " + type.name + " exception, none thrown", stack);
}

function run_test() {
  // Test ctypes.CType and ctypes.CData are set up correctly.
  run_abstract_class_tests();

  // open the library
  let libfile = do_get_file(CTYPES_TEST_LIB);
  let library = ctypes.open(libfile.path);

  // Make sure we can call a function in the library.
  run_void_tests(library);

  // Test Int64 and UInt64.
  run_Int64_tests();
  run_UInt64_tests();

  // Test the basic bool, integer, and float types.
  run_bool_tests(library);

  run_integer_tests(library, ctypes.int8_t, "int8_t", 1, true, [-0x80, 0x7f]);
  run_integer_tests(library, ctypes.int16_t, "int16_t", 2, true, [
    -0x8000,
    0x7fff,
  ]);
  run_integer_tests(library, ctypes.int32_t, "int32_t", 4, true, [
    -0x80000000,
    0x7fffffff,
  ]);
  run_integer_tests(library, ctypes.uint8_t, "uint8_t", 1, false, [0, 0xff]);
  run_integer_tests(library, ctypes.uint16_t, "uint16_t", 2, false, [
    0,
    0xffff,
  ]);
  run_integer_tests(library, ctypes.uint32_t, "uint32_t", 4, false, [
    0,
    0xffffffff,
  ]);
  run_integer_tests(library, ctypes.short, "short", 2, true, [-0x8000, 0x7fff]);
  run_integer_tests(
    library,
    ctypes.unsigned_short,
    "unsigned_short",
    2,
    false,
    [0, 0xffff]
  );
  run_integer_tests(library, ctypes.int, "int", 4, true, [
    -0x80000000,
    0x7fffffff,
  ]);
  run_integer_tests(library, ctypes.unsigned_int, "unsigned_int", 4, false, [
    0,
    0xffffffff,
  ]);
  run_integer_tests(library, ctypes.unsigned, "unsigned_int", 4, false, [
    0,
    0xffffffff,
  ]);

  run_float_tests(library, ctypes.float32_t, "float32_t", 4);
  run_float_tests(library, ctypes.float64_t, "float64_t", 8);
  run_float_tests(library, ctypes.float, "float", 4);
  run_float_tests(library, ctypes.double, "double", 8);

  // Test the wrapped integer types.
  const s64limits = [
    "-9223372036854775808",
    "9223372036854775807",
    "-9223372036854775809",
    "9223372036854775808",
  ];
  const u64limits = ["0", "18446744073709551615", "-1", "18446744073709551616"];

  run_wrapped_integer_tests(
    library,
    ctypes.int64_t,
    "int64_t",
    8,
    true,
    ctypes.Int64,
    "ctypes.Int64",
    s64limits
  );
  run_wrapped_integer_tests(
    library,
    ctypes.uint64_t,
    "uint64_t",
    8,
    false,
    ctypes.UInt64,
    "ctypes.UInt64",
    u64limits
  );
  run_wrapped_integer_tests(
    library,
    ctypes.long_long,
    "long_long",
    8,
    true,
    ctypes.Int64,
    "ctypes.Int64",
    s64limits
  );
  run_wrapped_integer_tests(
    library,
    ctypes.unsigned_long_long,
    "unsigned_long_long",
    8,
    false,
    ctypes.UInt64,
    "ctypes.UInt64",
    u64limits
  );

  const s32limits = [-0x80000000, 0x7fffffff, -0x80000001, 0x80000000];
  const u32limits = [0, 0xffffffff, -1, 0x100000000];

  let slimits, ulimits;
  if (ctypes.long.size == 8) {
    slimits = s64limits;
    ulimits = u64limits;
  } else if (ctypes.long.size == 4) {
    slimits = s32limits;
    ulimits = u32limits;
  } else {
    do_throw("ctypes.long is not 4 or 8 bytes");
  }

  run_wrapped_integer_tests(
    library,
    ctypes.long,
    "long",
    ctypes.long.size,
    true,
    ctypes.Int64,
    "ctypes.Int64",
    slimits
  );
  run_wrapped_integer_tests(
    library,
    ctypes.unsigned_long,
    "unsigned_long",
    ctypes.long.size,
    false,
    ctypes.UInt64,
    "ctypes.UInt64",
    ulimits
  );

  if (ctypes.size_t.size == 8) {
    slimits = s64limits;
    ulimits = u64limits;
  } else if (ctypes.size_t.size == 4) {
    slimits = s32limits;
    ulimits = u32limits;
  } else {
    do_throw("ctypes.size_t is not 4 or 8 bytes");
  }

  run_wrapped_integer_tests(
    library,
    ctypes.size_t,
    "size_t",
    ctypes.size_t.size,
    false,
    ctypes.UInt64,
    "ctypes.UInt64",
    ulimits
  );
  run_wrapped_integer_tests(
    library,
    ctypes.ssize_t,
    "ssize_t",
    ctypes.size_t.size,
    true,
    ctypes.Int64,
    "ctypes.Int64",
    slimits
  );
  run_wrapped_integer_tests(
    library,
    ctypes.uintptr_t,
    "uintptr_t",
    ctypes.size_t.size,
    false,
    ctypes.UInt64,
    "ctypes.UInt64",
    ulimits
  );
  run_wrapped_integer_tests(
    library,
    ctypes.intptr_t,
    "intptr_t",
    ctypes.size_t.size,
    true,
    ctypes.Int64,
    "ctypes.Int64",
    slimits
  );

  if (ctypes.off_t.size == 8) {
    slimits = s64limits;
    ulimits = u64limits;
  } else if (ctypes.off_t.size == 4) {
    slimits = s32limits;
    ulimits = u32limits;
  } else {
    do_throw("ctypes.off_t is not 4 or 8 bytes");
  }
  run_wrapped_integer_tests(
    library,
    ctypes.off_t,
    "off_t",
    ctypes.off_t.size,
    true,
    ctypes.Int64,
    "ctypes.Int64",
    slimits
  );

  // Test the character types.
  run_char_tests(library, ctypes.char, "char", 1, true, [-0x80, 0x7f]);
  run_char_tests(library, ctypes.signed_char, "signed_char", 1, true, [
    -0x80,
    0x7f,
  ]);
  run_char_tests(library, ctypes.unsigned_char, "unsigned_char", 1, false, [
    0,
    0xff,
  ]);
  run_char16_tests(library, ctypes.char16_t, "char16_t", [0, 0xffff]);

  // Test the special types.
  run_StructType_tests();
  run_PointerType_tests();
  run_FunctionType_tests();
  run_ArrayType_tests();

  // Check that types print properly.
  run_type_toString_tests();

  // Test the 'name' and 'toSource' of a long typename.
  let ptrTo_ptrTo_arrayOf4_ptrTo_int32s = new ctypes.PointerType(
    new ctypes.PointerType(
      new ctypes.ArrayType(new ctypes.PointerType(ctypes.int32_t), 4)
    )
  );
  Assert.equal(ptrTo_ptrTo_arrayOf4_ptrTo_int32s.name, "int32_t*(**)[4]");

  let source_t = new ctypes.StructType("source", [
    { a: ptrTo_ptrTo_arrayOf4_ptrTo_int32s },
    { b: ctypes.int64_t },
  ]);
  Assert.equal(
    source_t.toSource(),
    'ctypes.StructType("source", [{ "a": ctypes.int32_t.ptr.array(4).ptr.ptr }, ' +
      '{ "b": ctypes.int64_t }])'
  );

  // Test ctypes.cast.
  run_cast_tests();

  run_string_tests(library);
  run_readstring_tests(library);
  run_struct_tests(library);
  run_function_tests(library);
  run_closure_tests(library);
  run_variadic_tests(library);
  run_static_data_tests(library);
  run_cpp_class_tests(library);

  // test library.close
  let test_void_t = library.declare(
    "test_void_t_cdecl",
    ctypes.default_abi,
    ctypes.void_t
  );
  library.close();
  do_check_throws(function() {
    test_void_t();
  }, Error);
  do_check_throws(function() {
    library.declare("test_void_t_cdecl", ctypes.default_abi, ctypes.void_t);
  }, Error);

  // test that library functions throw when bound to other objects
  library = ctypes.open(libfile.path);
  let obj = {};
  obj.declare = library.declare;
  do_check_throws(function() {
    run_void_tests(obj);
  }, Error);
  obj.close = library.close;
  do_check_throws(function() {
    obj.close();
  }, Error);

  // test that functions work as properties of other objects
  let getter = library.declare(
    "get_int8_t_cdecl",
    ctypes.default_abi,
    ctypes.int8_t
  );
  Assert.equal(getter(), 109);
  obj.t = getter;
  Assert.equal(obj.t(), 109);

  // bug 521937
  do_check_throws(function() {
    let nolib = ctypes.open("notfoundlibrary.dll");
    nolib.close();
  }, Error);

  // bug 522360
  Assert.equal(run_load_system_library(), true);

  // Test loading a library with a unicode name (bug 589413). Note that nsIFile
  // implementations are not available in some harnesses; if not, the harness
  // should take care of the copy for us.
  let unicodefile = do_get_file(CTYPES_UNICODE_LIB, true);
  let copy = libfile.copyTo instanceof Function;
  if (copy) {
    libfile.copyTo(null, unicodefile.leafName);
  }
  library = ctypes.open(unicodefile.path);
  run_void_tests(library);
  library.close();
  if (copy) {
    // Tolerate remove() failure because Firefox may have the DLL open
    // for examination.
    try {
      unicodefile.remove(false);
    } catch (e) {}
  }
}

function run_abstract_class_tests() {
  // Test that ctypes.CType is an abstract constructor that throws.
  do_check_throws(function() {
    ctypes.CType();
  }, TypeError);
  do_check_throws(function() {
    new ctypes.CType();
  }, TypeError);

  Assert.ok(ctypes.CType.hasOwnProperty("prototype"));
  do_check_throws(function() {
    ctypes.CType.prototype();
  }, TypeError);
  do_check_throws(function() {
    new ctypes.CType.prototype();
  }, TypeError);

  Assert.ok(ctypes.CType.prototype.hasOwnProperty("constructor"));
  Assert.ok(ctypes.CType.prototype.constructor === ctypes.CType);

  // Check that ctypes.CType.prototype has the correct properties and functions.
  Assert.ok(ctypes.CType.prototype.hasOwnProperty("name"));
  Assert.ok(ctypes.CType.prototype.hasOwnProperty("size"));
  Assert.ok(ctypes.CType.prototype.hasOwnProperty("ptr"));
  Assert.ok(ctypes.CType.prototype.hasOwnProperty("array"));
  Assert.ok(ctypes.CType.prototype.hasOwnProperty("toString"));
  Assert.ok(ctypes.CType.prototype.hasOwnProperty("toSource"));

  // Make sure we can access 'prototype' on a CTypeProto.
  Assert.ok(ctypes.CType.prototype.prototype === ctypes.CData.prototype);

  // Check that the shared properties and functions on ctypes.CType.prototype throw.
  do_check_throws(function() {
    ctypes.CType.prototype.name;
  }, TypeError);
  do_check_throws(function() {
    ctypes.CType.prototype.size;
  }, TypeError);
  do_check_throws(function() {
    ctypes.CType.prototype.ptr;
  }, TypeError);
  do_check_throws(function() {
    ctypes.CType.prototype.array();
  }, TypeError);

  // toString and toSource are called by the web console during inspection,
  // so we don't want them to throw.
  Assert.equal(typeof ctypes.CType.prototype.toString(), "string");
  Assert.equal(typeof ctypes.CType.prototype.toSource(), "string");

  // Test that ctypes.CData is an abstract constructor that throws.
  do_check_throws(function() {
    ctypes.CData();
  }, TypeError);
  do_check_throws(function() {
    new ctypes.CData();
  }, TypeError);

  Assert.ok(ctypes.CData.__proto__ === ctypes.CType.prototype);
  Assert.ok(ctypes.CData instanceof ctypes.CType);

  Assert.ok(ctypes.CData.hasOwnProperty("prototype"));
  Assert.ok(ctypes.CData.prototype.hasOwnProperty("constructor"));
  Assert.ok(ctypes.CData.prototype.constructor === ctypes.CData);

  // Check that ctypes.CData.prototype has the correct properties and functions.
  Assert.ok(ctypes.CData.prototype.hasOwnProperty("value"));
  Assert.ok(ctypes.CData.prototype.hasOwnProperty("address"));
  Assert.ok(ctypes.CData.prototype.hasOwnProperty("readString"));
  Assert.ok(ctypes.CData.prototype.hasOwnProperty("toString"));
  Assert.ok(ctypes.CData.prototype.hasOwnProperty("toSource"));

  // Check that the shared properties and functions on ctypes.CData.prototype throw.
  do_check_throws(function() {
    ctypes.CData.prototype.value;
  }, TypeError);
  do_check_throws(function() {
    ctypes.CData.prototype.value = null;
  }, TypeError);
  do_check_throws(function() {
    ctypes.CData.prototype.address();
  }, TypeError);
  do_check_throws(function() {
    ctypes.CData.prototype.readString();
  }, TypeError);

  // toString and toSource are called by the web console during inspection,
  // so we don't want them to throw.
  Assert.equal(ctypes.CData.prototype.toString(), "[CData proto object]");
  Assert.equal(ctypes.CData.prototype.toSource(), "[CData proto object]");
}

function run_Int64_tests() {
  do_check_throws(function() {
    ctypes.Int64();
  }, TypeError);

  Assert.ok(ctypes.Int64.hasOwnProperty("prototype"));
  Assert.ok(ctypes.Int64.prototype.hasOwnProperty("constructor"));
  Assert.ok(ctypes.Int64.prototype.constructor === ctypes.Int64);

  // Check that ctypes.Int64 and ctypes.Int64.prototype have the correct
  // properties and functions.
  Assert.ok(ctypes.Int64.hasOwnProperty("compare"));
  Assert.ok(ctypes.Int64.hasOwnProperty("lo"));
  Assert.ok(ctypes.Int64.hasOwnProperty("hi"));
  Assert.ok(ctypes.Int64.hasOwnProperty("join"));
  Assert.ok(ctypes.Int64.prototype.hasOwnProperty("toString"));
  Assert.ok(ctypes.Int64.prototype.hasOwnProperty("toSource"));

  // Check that the shared functions on ctypes.Int64.prototype throw.
  do_check_throws(function() {
    ctypes.Int64.prototype.toString();
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.prototype.toSource();
  }, TypeError);

  let int64 = ctypes.Int64(0);
  Assert.ok(int64.__proto__ === ctypes.Int64.prototype);
  Assert.ok(int64 instanceof ctypes.Int64);

  // Test Int64.toString([radix]).
  Assert.equal(int64.toString(), "0");
  for (let radix = 2; radix <= 36; ++radix) {
    Assert.equal(int64.toString(radix), "0");
  }
  do_check_throws(function() {
    int64.toString(0);
  }, RangeError);
  do_check_throws(function() {
    int64.toString(1);
  }, RangeError);
  do_check_throws(function() {
    int64.toString(37);
  }, RangeError);
  do_check_throws(function() {
    int64.toString(10, 2);
  }, TypeError);

  // Test Int64.toSource().
  Assert.equal(int64.toSource(), 'ctypes.Int64("0")');
  do_check_throws(function() {
    int64.toSource(10);
  }, TypeError);

  int64 = ctypes.Int64("0x28590a1c921def71");
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "2907366152271163249");
  Assert.equal(int64.toString(16), "28590a1c921def71");
  Assert.equal(
    int64.toString(2),
    "10100001011001000010100001110010010010000111011110111101110001"
  );
  Assert.equal(int64.toSource(), 'ctypes.Int64("' + int64.toString(10) + '")');

  int64 = ctypes.Int64("-0x28590a1c921def71");
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "-2907366152271163249");
  Assert.equal(int64.toString(16), "-28590a1c921def71");
  Assert.equal(
    int64.toString(2),
    "-10100001011001000010100001110010010010000111011110111101110001"
  );
  Assert.equal(int64.toSource(), 'ctypes.Int64("' + int64.toString(10) + '")');

  int64 = ctypes.Int64("-0X28590A1c921DEf71");
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "-2907366152271163249");
  Assert.equal(int64.toString(16), "-28590a1c921def71");
  Assert.equal(
    int64.toString(2),
    "-10100001011001000010100001110010010010000111011110111101110001"
  );
  Assert.equal(int64.toSource(), 'ctypes.Int64("' + int64.toString(10) + '")');

  // Test Int64(primitive double) constructor.
  int64 = ctypes.Int64(-0);
  Assert.equal(int64.toString(), "0");

  int64 = ctypes.Int64(0x7ffffffffffff000);
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "9223372036854771712");
  Assert.equal(int64.toString(16), "7ffffffffffff000");
  Assert.equal(
    int64.toString(2),
    "111111111111111111111111111111111111111111111111111000000000000"
  );

  int64 = ctypes.Int64(-0x8000000000000000);
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "-9223372036854775808");
  Assert.equal(int64.toString(16), "-8000000000000000");
  Assert.equal(
    int64.toString(2),
    "-1000000000000000000000000000000000000000000000000000000000000000"
  );

  // Test Int64(string) constructor.
  int64 = ctypes.Int64("0x7fffffffffffffff");
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "9223372036854775807");
  Assert.equal(int64.toString(16), "7fffffffffffffff");
  Assert.equal(
    int64.toString(2),
    "111111111111111111111111111111111111111111111111111111111111111"
  );

  int64 = ctypes.Int64("-0x8000000000000000");
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "-9223372036854775808");
  Assert.equal(int64.toString(16), "-8000000000000000");
  Assert.equal(
    int64.toString(2),
    "-1000000000000000000000000000000000000000000000000000000000000000"
  );

  int64 = ctypes.Int64("9223372036854775807");
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "9223372036854775807");
  Assert.equal(int64.toString(16), "7fffffffffffffff");
  Assert.equal(
    int64.toString(2),
    "111111111111111111111111111111111111111111111111111111111111111"
  );

  int64 = ctypes.Int64("-9223372036854775808");
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "-9223372036854775808");
  Assert.equal(int64.toString(16), "-8000000000000000");
  Assert.equal(
    int64.toString(2),
    "-1000000000000000000000000000000000000000000000000000000000000000"
  );

  // Test Int64(other Int64) constructor.
  int64 = ctypes.Int64(ctypes.Int64(0));
  Assert.equal(int64.toString(), "0");

  int64 = ctypes.Int64(ctypes.Int64("0x7fffffffffffffff"));
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "9223372036854775807");
  Assert.equal(int64.toString(16), "7fffffffffffffff");
  Assert.equal(
    int64.toString(2),
    "111111111111111111111111111111111111111111111111111111111111111"
  );

  int64 = ctypes.Int64(ctypes.Int64("-0x8000000000000000"));
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "-9223372036854775808");
  Assert.equal(int64.toString(16), "-8000000000000000");
  Assert.equal(
    int64.toString(2),
    "-1000000000000000000000000000000000000000000000000000000000000000"
  );

  // Test Int64(other UInt64) constructor.
  int64 = ctypes.Int64(ctypes.UInt64(0));
  Assert.equal(int64.toString(), "0");

  int64 = ctypes.Int64(ctypes.UInt64("0x7fffffffffffffff"));
  Assert.equal(int64.toString(), int64.toString(10));
  Assert.equal(int64.toString(10), "9223372036854775807");
  Assert.equal(int64.toString(16), "7fffffffffffffff");
  Assert.equal(
    int64.toString(2),
    "111111111111111111111111111111111111111111111111111111111111111"
  );

  let vals = [
    -0x8000000000001000,
    0x8000000000000000,
    ctypes.UInt64("0x8000000000000000"),
    Infinity,
    -Infinity,
    NaN,
    0.1,
    5.68e21,
    null,
    undefined,
    "",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      ctypes.Int64(vals[i]);
    }, TypeError);
  }

  vals = ["-0x8000000000000001", "0x8000000000000000"];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      ctypes.Int64(vals[i]);
    }, RangeError);
  }

  // Test ctypes.Int64.compare.
  Assert.equal(ctypes.Int64.compare(ctypes.Int64(5), ctypes.Int64(5)), 0);
  Assert.equal(ctypes.Int64.compare(ctypes.Int64(5), ctypes.Int64(4)), 1);
  Assert.equal(ctypes.Int64.compare(ctypes.Int64(4), ctypes.Int64(5)), -1);
  Assert.equal(ctypes.Int64.compare(ctypes.Int64(-5), ctypes.Int64(-5)), 0);
  Assert.equal(ctypes.Int64.compare(ctypes.Int64(-5), ctypes.Int64(-4)), -1);
  Assert.equal(ctypes.Int64.compare(ctypes.Int64(-4), ctypes.Int64(-5)), 1);
  do_check_throws(function() {
    ctypes.Int64.compare(ctypes.Int64(4), ctypes.UInt64(4));
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.compare(4, 5);
  }, TypeError);

  // Test ctypes.Int64.{lo,hi}.
  Assert.equal(ctypes.Int64.lo(ctypes.Int64(0x28590a1c921de000)), 0x921de000);
  Assert.equal(ctypes.Int64.hi(ctypes.Int64(0x28590a1c921de000)), 0x28590a1c);
  Assert.equal(ctypes.Int64.lo(ctypes.Int64(-0x28590a1c921de000)), 0x6de22000);
  Assert.equal(ctypes.Int64.hi(ctypes.Int64(-0x28590a1c921de000)), -0x28590a1d);
  do_check_throws(function() {
    ctypes.Int64.lo(ctypes.UInt64(0));
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.hi(ctypes.UInt64(0));
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.lo(0);
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.hi(0);
  }, TypeError);

  // Test ctypes.Int64.join.
  Assert.equal(ctypes.Int64.join(0, 0).toString(), "0");
  Assert.equal(
    ctypes.Int64.join(0x28590a1c, 0x921de000).toString(16),
    "28590a1c921de000"
  );
  Assert.equal(
    ctypes.Int64.join(-0x28590a1d, 0x6de22000).toString(16),
    "-28590a1c921de000"
  );
  Assert.equal(
    ctypes.Int64.join(0x7fffffff, 0xffffffff).toString(16),
    "7fffffffffffffff"
  );
  Assert.equal(
    ctypes.Int64.join(-0x80000000, 0x00000000).toString(16),
    "-8000000000000000"
  );
  /* eslint-disable mozilla/use-returnValue */
  do_check_throws(function() {
    ctypes.Int64.join(-0x80000001, 0);
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.join(0x80000000, 0);
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.join(0, -0x1);
  }, TypeError);
  do_check_throws(function() {
    ctypes.Int64.join(0, 0x800000000);
  }, TypeError);
  /* eslint-enable mozilla/use-returnValue */
}

function run_UInt64_tests() {
  do_check_throws(function() {
    ctypes.UInt64();
  }, TypeError);

  Assert.ok(ctypes.UInt64.hasOwnProperty("prototype"));
  Assert.ok(ctypes.UInt64.prototype.hasOwnProperty("constructor"));
  Assert.ok(ctypes.UInt64.prototype.constructor === ctypes.UInt64);

  // Check that ctypes.UInt64 and ctypes.UInt64.prototype have the correct
  // properties and functions.
  Assert.ok(ctypes.UInt64.hasOwnProperty("compare"));
  Assert.ok(ctypes.UInt64.hasOwnProperty("lo"));
  Assert.ok(ctypes.UInt64.hasOwnProperty("hi"));
  Assert.ok(ctypes.UInt64.hasOwnProperty("join"));
  Assert.ok(ctypes.UInt64.prototype.hasOwnProperty("toString"));
  Assert.ok(ctypes.UInt64.prototype.hasOwnProperty("toSource"));

  // Check that the shared functions on ctypes.UInt64.prototype throw.
  do_check_throws(function() {
    ctypes.UInt64.prototype.toString();
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.prototype.toSource();
  }, TypeError);

  let uint64 = ctypes.UInt64(0);
  Assert.ok(uint64.__proto__ === ctypes.UInt64.prototype);
  Assert.ok(uint64 instanceof ctypes.UInt64);

  // Test UInt64.toString([radix]).
  Assert.equal(uint64.toString(), "0");
  for (let radix = 2; radix <= 36; ++radix) {
    Assert.equal(uint64.toString(radix), "0");
  }
  do_check_throws(function() {
    uint64.toString(0);
  }, RangeError);
  do_check_throws(function() {
    uint64.toString(1);
  }, RangeError);
  do_check_throws(function() {
    uint64.toString(37);
  }, RangeError);
  do_check_throws(function() {
    uint64.toString(10, 2);
  }, TypeError);

  // Test UInt64.toSource().
  Assert.equal(uint64.toSource(), 'ctypes.UInt64("0")');
  do_check_throws(function() {
    uint64.toSource(10);
  }, TypeError);

  uint64 = ctypes.UInt64("0x28590a1c921def71");
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "2907366152271163249");
  Assert.equal(uint64.toString(16), "28590a1c921def71");
  Assert.equal(
    uint64.toString(2),
    "10100001011001000010100001110010010010000111011110111101110001"
  );
  Assert.equal(
    uint64.toSource(),
    'ctypes.UInt64("' + uint64.toString(10) + '")'
  );

  uint64 = ctypes.UInt64("0X28590A1c921DEf71");
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "2907366152271163249");
  Assert.equal(uint64.toString(16), "28590a1c921def71");
  Assert.equal(
    uint64.toString(2),
    "10100001011001000010100001110010010010000111011110111101110001"
  );
  Assert.equal(
    uint64.toSource(),
    'ctypes.UInt64("' + uint64.toString(10) + '")'
  );

  // Test UInt64(primitive double) constructor.
  uint64 = ctypes.UInt64(-0);
  Assert.equal(uint64.toString(), "0");

  uint64 = ctypes.UInt64(0xfffffffffffff000);
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "18446744073709547520");
  Assert.equal(uint64.toString(16), "fffffffffffff000");
  Assert.equal(
    uint64.toString(2),
    "1111111111111111111111111111111111111111111111111111000000000000"
  );

  // Test UInt64(string) constructor.
  uint64 = ctypes.UInt64("0xffffffffffffffff");
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "18446744073709551615");
  Assert.equal(uint64.toString(16), "ffffffffffffffff");
  Assert.equal(
    uint64.toString(2),
    "1111111111111111111111111111111111111111111111111111111111111111"
  );

  uint64 = ctypes.UInt64("0x0");
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "0");
  Assert.equal(uint64.toString(16), "0");
  Assert.equal(uint64.toString(2), "0");

  uint64 = ctypes.UInt64("18446744073709551615");
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "18446744073709551615");
  Assert.equal(uint64.toString(16), "ffffffffffffffff");
  Assert.equal(
    uint64.toString(2),
    "1111111111111111111111111111111111111111111111111111111111111111"
  );

  uint64 = ctypes.UInt64("0");
  Assert.equal(uint64.toString(), "0");

  // Test UInt64(other UInt64) constructor.
  uint64 = ctypes.UInt64(ctypes.UInt64(0));
  Assert.equal(uint64.toString(), "0");

  uint64 = ctypes.UInt64(ctypes.UInt64("0xffffffffffffffff"));
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "18446744073709551615");
  Assert.equal(uint64.toString(16), "ffffffffffffffff");
  Assert.equal(
    uint64.toString(2),
    "1111111111111111111111111111111111111111111111111111111111111111"
  );

  uint64 = ctypes.UInt64(ctypes.UInt64("0x0"));
  Assert.equal(uint64.toString(), "0");

  // Test UInt64(other Int64) constructor.
  uint64 = ctypes.UInt64(ctypes.Int64(0));
  Assert.equal(uint64.toString(), "0");

  uint64 = ctypes.UInt64(ctypes.Int64("0x7fffffffffffffff"));
  Assert.equal(uint64.toString(), uint64.toString(10));
  Assert.equal(uint64.toString(10), "9223372036854775807");
  Assert.equal(uint64.toString(16), "7fffffffffffffff");
  Assert.equal(
    uint64.toString(2),
    "111111111111111111111111111111111111111111111111111111111111111"
  );

  let vals = [
    -1,
    0x10000000000000000,
    "-1",
    "-0x1",
    ctypes.Int64("-1"),
    Infinity,
    -Infinity,
    NaN,
    0.1,
    5.68e21,
    null,
    undefined,
    "",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      ctypes.UInt64(vals[i]);
    }, TypeError);
  }

  vals = ["0x10000000000000000"];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      ctypes.UInt64(vals[i]);
    }, RangeError);
  }

  // Test ctypes.UInt64.compare.
  Assert.equal(ctypes.UInt64.compare(ctypes.UInt64(5), ctypes.UInt64(5)), 0);
  Assert.equal(ctypes.UInt64.compare(ctypes.UInt64(5), ctypes.UInt64(4)), 1);
  Assert.equal(ctypes.UInt64.compare(ctypes.UInt64(4), ctypes.UInt64(5)), -1);
  do_check_throws(function() {
    ctypes.UInt64.compare(ctypes.UInt64(4), ctypes.Int64(4));
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.compare(4, 5);
  }, TypeError);

  // Test ctypes.UInt64.{lo,hi}.
  Assert.equal(ctypes.UInt64.lo(ctypes.UInt64(0x28590a1c921de000)), 0x921de000);
  Assert.equal(ctypes.UInt64.hi(ctypes.UInt64(0x28590a1c921de000)), 0x28590a1c);
  Assert.equal(ctypes.UInt64.lo(ctypes.UInt64(0xa8590a1c921de000)), 0x921de000);
  Assert.equal(ctypes.UInt64.hi(ctypes.UInt64(0xa8590a1c921de000)), 0xa8590a1c);
  do_check_throws(function() {
    ctypes.UInt64.lo(ctypes.Int64(0));
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.hi(ctypes.Int64(0));
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.lo(0);
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.hi(0);
  }, TypeError);

  // Test ctypes.UInt64.join.
  Assert.equal(ctypes.UInt64.join(0, 0).toString(), "0");
  Assert.equal(
    ctypes.UInt64.join(0x28590a1c, 0x921de000).toString(16),
    "28590a1c921de000"
  );
  Assert.equal(
    ctypes.UInt64.join(0xa8590a1c, 0x921de000).toString(16),
    "a8590a1c921de000"
  );
  Assert.equal(
    ctypes.UInt64.join(0xffffffff, 0xffffffff).toString(16),
    "ffffffffffffffff"
  );
  Assert.equal(ctypes.UInt64.join(0, 0).toString(16), "0");
  /* eslint-disable mozilla/use-returnValue */
  do_check_throws(function() {
    ctypes.UInt64.join(-0x1, 0);
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.join(0x100000000, 0);
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.join(0, -0x1);
  }, TypeError);
  do_check_throws(function() {
    ctypes.UInt64.join(0, 0x1000000000);
  }, TypeError);
  /* eslint-enable mozilla/use-returnValue */
}

function run_basic_abi_tests(
  library,
  t,
  name,
  toprimitive,
  get_test,
  set_tests,
  sum_tests,
  sum_many_tests
) {
  // Test the function call ABI for calls involving the type.
  function declare_fn_cdecl(fn_t, prefix) {
    return library.declare(prefix + name + "_cdecl", fn_t);
  }
  run_single_abi_tests(
    declare_fn_cdecl,
    ctypes.default_abi,
    t,
    toprimitive,
    get_test,
    set_tests,
    sum_tests,
    sum_many_tests
  );

  if ("winLastError" in ctypes) {
    function declare_fn_stdcall(fn_t, prefix) {
      return library.declare(prefix + name + "_stdcall", fn_t);
    }
    run_single_abi_tests(
      declare_fn_stdcall,
      ctypes.stdcall_abi,
      t,
      toprimitive,
      get_test,
      set_tests,
      sum_tests,
      sum_many_tests
    );

    // Check that declaring a WINAPI function gets the right symbol name.
    let libuser32 = ctypes.open("user32.dll");
    let charupper = libuser32.declare(
      "CharUpperA",
      ctypes.winapi_abi,
      ctypes.char.ptr,
      ctypes.char.ptr
    );
    let hello = ctypes.char.array()("hello!");
    Assert.equal(charupper(hello).readString(), "HELLO!");
  }

  // Check the alignment of the type, and its behavior in a struct,
  // against what C says.
  check_struct_stats(library, t);

  // Check the ToSource functions defined in the namespace ABI
  Assert.equal(ctypes.default_abi.toSource(), "ctypes.default_abi");

  let exn;
  try {
    ctypes.default_abi.toSource.call(null);
  } catch (x) {
    exn = x;
  }
  Assert.ok(!!exn); // Check that some exception was raised
}

function run_single_abi_tests(
  decl,
  abi,
  t,
  toprimitive,
  get_test,
  set_tests,
  sum_tests,
  sum_many_tests
) {
  let getter_t = ctypes.FunctionType(abi, t).ptr;
  let getter = decl(getter_t, "get_");
  Assert.equal(toprimitive(getter()), get_test);

  let setter_t = ctypes.FunctionType(abi, t, [t]).ptr;
  let setter = decl(setter_t, "set_");
  for (let i of set_tests) {
    Assert.equal(toprimitive(setter(i)), i);
  }

  let sum_t = ctypes.FunctionType(abi, t, [t, t]).ptr;
  let sum = decl(sum_t, "sum_");
  for (let a of sum_tests) {
    Assert.equal(toprimitive(sum(a[0], a[1])), a[2]);
  }

  let sum_alignb_t = ctypes.FunctionType(abi, t, [
    ctypes.char,
    t,
    ctypes.char,
    t,
    ctypes.char,
  ]).ptr;
  let sum_alignb = decl(sum_alignb_t, "sum_alignb_");
  let sum_alignf_t = ctypes.FunctionType(abi, t, [
    ctypes.float,
    t,
    ctypes.float,
    t,
    ctypes.float,
  ]).ptr;
  let sum_alignf = decl(sum_alignf_t, "sum_alignf_");
  for (let a of sum_tests) {
    Assert.equal(toprimitive(sum_alignb(0, a[0], 0, a[1], 0)), a[2]);
    Assert.equal(toprimitive(sum_alignb(1, a[0], 1, a[1], 1)), a[2]);
    Assert.equal(toprimitive(sum_alignf(0, a[0], 0, a[1], 0)), a[2]);
    Assert.equal(toprimitive(sum_alignf(1, a[0], 1, a[1], 1)), a[2]);
  }

  let sum_many_t = ctypes.FunctionType(abi, t, [
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
    t,
  ]).ptr;
  let sum_many = decl(sum_many_t, "sum_many_");
  for (let a of sum_many_tests) {
    Assert.equal(
      toprimitive(
        sum_many(
          a[0],
          a[1],
          a[2],
          a[3],
          a[4],
          a[5],
          a[6],
          a[7],
          a[8],
          a[9],
          a[10],
          a[11],
          a[12],
          a[13],
          a[14],
          a[15],
          a[16],
          a[17]
        )
      ),
      a[18]
    );
  }
}

function check_struct_stats(library, t) {
  let s_t = ctypes.StructType("s_t", [{ x: ctypes.char }, { y: t }]);
  let n_t = ctypes.StructType("n_t", [
    { a: ctypes.char },
    { b: s_t },
    { c: ctypes.char },
  ]);
  let get_stats = library.declare(
    "get_" + t.name + "_stats",
    ctypes.default_abi,
    ctypes.void_t,
    ctypes.size_t.ptr,
    ctypes.size_t.ptr,
    ctypes.size_t.ptr,
    ctypes.size_t.ptr,
    ctypes.size_t.array()
  );

  let align = ctypes.size_t();
  let size = ctypes.size_t();
  let nalign = ctypes.size_t();
  let nsize = ctypes.size_t();
  let offsets = ctypes.size_t.array(3)();
  get_stats(
    align.address(),
    size.address(),
    nalign.address(),
    nsize.address(),
    offsets
  );

  Assert.equal(size.value, s_t.size);
  Assert.equal(align.value, s_t.size - t.size);
  Assert.equal(align.value, offsetof(s_t, "y"));
  Assert.equal(nsize.value, n_t.size);
  Assert.equal(nalign.value, offsetof(n_t, "b"));
  Assert.equal(offsets[0], offsetof(s_t, "y"));
  Assert.equal(offsets[1], offsetof(n_t, "b"));
  Assert.equal(offsets[2], offsetof(n_t, "c"));
}

// Determine the offset, in bytes, of 'member' within 'struct'.
function offsetof(struct, member) {
  let instance = struct();
  let memberptr = ptrValue(instance.addressOfField(member));
  let chararray = ctypes.cast(instance, ctypes.char.array(struct.size));
  let offset = 0;
  while (memberptr != ptrValue(chararray.addressOfElement(offset))) {
    ++offset;
  }
  return offset;
}

// Test the class and prototype hierarchy for a given basic type 't'.
function run_basic_class_tests(t) {
  Assert.ok(t.__proto__ === ctypes.CType.prototype);
  Assert.ok(t instanceof ctypes.CType);

  Assert.ok(t.prototype.__proto__ === ctypes.CData.prototype);
  Assert.ok(t.prototype instanceof ctypes.CData);
  Assert.ok(t.prototype.constructor === t);

  // Check that the shared properties and functions on 't.prototype' throw.
  do_check_throws(function() {
    t.prototype.value;
  }, TypeError);
  do_check_throws(function() {
    t.prototype.value = null;
  }, TypeError);
  do_check_throws(function() {
    t.prototype.address();
  }, TypeError);
  do_check_throws(function() {
    t.prototype.readString();
  }, TypeError);

  // toString and toSource are called by the web console during inspection,
  // so we don't want them to throw.
  Assert.equal(t.prototype.toString(), "[CData proto object]");
  Assert.equal(t.prototype.toSource(), "[CData proto object]");

  // Test that an instance 'd' of 't' is a CData.
  let d = t();
  Assert.ok(d.__proto__ === t.prototype);
  Assert.ok(d instanceof t);
  Assert.ok(d.constructor === t);
}

function run_bool_tests(library) {
  let t = ctypes.bool;
  run_basic_class_tests(t);

  let name = "bool";
  Assert.equal(t.name, name);
  Assert.ok(t.size == 1 || t.size == 4);

  Assert.equal(t.toString(), "type " + name);
  Assert.equal(t.toSource(), "ctypes." + name);
  Assert.ok(t.ptr === ctypes.PointerType(t));
  Assert.equal(t.array().name, name + "[]");
  Assert.equal(t.array(5).name, name + "[5]");

  let d = t();
  Assert.equal(d.value, 0);
  d.value = 1;
  Assert.equal(d.value, 1);
  d.value = -0;
  Assert.equal(1 / d.value, 1 / 0);
  d.value = false;
  Assert.equal(d.value, 0);
  d.value = true;
  Assert.equal(d.value, 1);
  d = new t(1);
  Assert.equal(d.value, 1);

  // don't convert anything else
  let vals = [
    -1,
    2,
    Infinity,
    -Infinity,
    NaN,
    0.1,
    ctypes.Int64(0),
    ctypes.UInt64(0),
    null,
    undefined,
    "",
    "0",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      d.value = vals[i];
    }, TypeError);
  }

  Assert.ok(d.address().constructor === t.ptr);
  Assert.equal(d.address().contents, d.value);
  Assert.equal(d.toSource(), "ctypes." + name + "(" + d.value + ")");
  Assert.equal(d.toSource(), d.toString());

  // Test the function call ABI for calls involving the type,
  // and check the alignment of the type against what C says.
  function toprimitive(a) {
    return a;
  }
  run_basic_abi_tests(
    library,
    t,
    name,
    toprimitive,
    true,
    [false, true],
    [
      [false, false, false],
      [false, true, true],
      [true, false, true],
      [true, true, true],
    ],
    [
      [
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
      ],
      [
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        true,
      ],
    ]
  );
}

function run_integer_tests(library, t, name, size, signed, limits) {
  run_basic_class_tests(t);

  Assert.equal(t.name, name);
  Assert.equal(t.size, size);

  Assert.equal(t.toString(), "type " + name);
  Assert.equal(t.toSource(), "ctypes." + name);
  Assert.ok(t.ptr === ctypes.PointerType(t));
  Assert.equal(t.array().name, name + "[]");
  Assert.equal(t.array(5).name, name + "[5]");

  // Check the alignment of the type, and its behavior in a struct,
  // against what C says.
  check_struct_stats(library, t);

  let d = t();
  Assert.equal(d.value, 0);
  d.value = 5;
  Assert.equal(d.value, 5);
  d = t(10);
  Assert.equal(d.value, 10);
  if (signed) {
    d.value = -10;
    Assert.equal(d.value, -10);
  }
  d = new t(20);
  Assert.equal(d.value, 20);

  d.value = ctypes.Int64(5);
  Assert.equal(d.value, 5);
  if (signed) {
    d.value = ctypes.Int64(-5);
    Assert.equal(d.value, -5);
  }
  d.value = ctypes.UInt64(5);
  Assert.equal(d.value, 5);

  d.value = limits[0];
  Assert.equal(d.value, limits[0]);
  d.value = limits[1];
  Assert.equal(d.value, limits[1]);
  d.value = 0;
  Assert.equal(d.value, 0);
  d.value = -0;
  Assert.equal(1 / d.value, 1 / 0);
  d.value = false;
  Assert.equal(d.value, 0);
  d.value = true;
  Assert.equal(d.value, 1);

  // don't convert anything else
  let vals = [
    limits[0] - 1,
    limits[1] + 1,
    Infinity,
    -Infinity,
    NaN,
    0.1,
    null,
    undefined,
    "",
    "0",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      d.value = vals[i];
    }, TypeError);
  }

  Assert.ok(d.address().constructor === t.ptr);
  Assert.equal(d.address().contents, d.value);
  Assert.equal(d.toSource(), "ctypes." + name + "(" + d.value + ")");
  Assert.equal(d.toSource(), d.toString());

  // Test the function call ABI for calls involving the type,
  // and check the alignment of the type against what C says.
  function toprimitive(a) {
    return a;
  }
  run_basic_abi_tests(
    library,
    t,
    name,
    toprimitive,
    109,
    [0, limits[0], limits[1]],
    [
      [0, 0, 0],
      [0, 1, 1],
      [1, 0, 1],
      [1, 1, 2],
      [limits[0], 1, limits[0] + 1],
      [limits[1], 1, limits[0]],
    ],
    [
      [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 18],
    ]
  );
}

function run_float_tests(library, t, name, size) {
  run_basic_class_tests(t);

  Assert.equal(t.name, name);
  Assert.equal(t.size, size);

  Assert.equal(t.toString(), "type " + name);
  Assert.equal(t.toSource(), "ctypes." + name);
  Assert.ok(t.ptr === ctypes.PointerType(t));
  Assert.equal(t.array().name, name + "[]");
  Assert.equal(t.array(5).name, name + "[5]");

  let d = t();
  Assert.equal(d.value, 0);
  d.value = 5;
  Assert.equal(d.value, 5);
  d.value = 5.25;
  Assert.equal(d.value, 5.25);
  d = t(10);
  Assert.equal(d.value, 10);
  d.value = -10;
  Assert.equal(d.value, -10);
  d = new t(20);
  Assert.equal(d.value, 20);

  do_check_throws(function() {
    d.value = ctypes.Int64(5);
  }, TypeError);
  do_check_throws(function() {
    d.value = ctypes.Int64(-5);
  }, TypeError);
  do_check_throws(function() {
    d.value = ctypes.UInt64(5);
  }, TypeError);

  if (size == 4) {
    d.value = 0x7fffff;
    Assert.equal(d.value, 0x7fffff);

    // allow values that can't be represented precisely as a float
    d.value = 0xffffffff;
    let delta = 1 - d.value / 0xffffffff;
    Assert.ok(delta != 0);
    Assert.ok(delta > -0.01 && delta < 0.01);
    d.value = 1 + 1 / 0x80000000;
    Assert.equal(d.value, 1);
  } else {
    d.value = 0xfffffffffffff000;
    Assert.equal(d.value, 0xfffffffffffff000);

    do_check_throws(function() {
      d.value = ctypes.Int64("0x7fffffffffffffff");
    }, TypeError);
  }

  d.value = Infinity;
  Assert.equal(d.value, Infinity);
  d.value = -Infinity;
  Assert.equal(d.value, -Infinity);
  d.value = NaN;
  Assert.ok(isNaN(d.value));
  d.value = 0;
  Assert.equal(d.value, 0);
  d.value = -0;
  Assert.equal(1 / d.value, 1 / -0);

  // don't convert anything else
  let vals = [
    true,
    false,
    null,
    undefined,
    "",
    "0",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      d.value = vals[i];
    }, TypeError);
  }

  // Check that values roundtrip through toSource() correctly.
  function test_roundtrip(tFn, val) {
    let f1 = tFn(val);
    // eslint-disable-next-line no-eval
    var f2 = eval(f1.toSource());
    Assert.equal(f1.value, f2.value);
  }
  vals = [
    Infinity,
    -Infinity,
    -0,
    0,
    1,
    -1,
    1 / 3,
    -1 / 3,
    1 / 4,
    -1 / 4,
    1e-14,
    -1e-14,
    0xfffffffffffff000,
    -0xfffffffffffff000,
  ];
  for (let i = 0; i < vals.length; i++) {
    test_roundtrip(t, vals[i]);
  }
  Assert.equal(t(NaN).toSource(), t.toSource() + "(NaN)");

  Assert.ok(d.address().constructor === t.ptr);
  Assert.equal(d.address().contents, d.value);
  Assert.equal(d.toSource(), "ctypes." + name + "(" + d.value + ")");
  Assert.equal(d.toSource(), d.toString());

  // Test the function call ABI for calls involving the type,
  // and check the alignment of the type against what C says.
  let operand = [];
  if (size == 4) {
    operand[0] = 503859.75;
    operand[1] = 1012385.25;
    operand[2] = 1516245;
  } else {
    operand[0] = 501823873859.75;
    operand[1] = 171290577385.25;
    operand[2] = 673114451245;
  }
  function toprimitive(a) {
    return a;
  }
  run_basic_abi_tests(
    library,
    t,
    name,
    toprimitive,
    109.25,
    [0, operand[0], operand[1]],
    [
      [0, 0, 0],
      [0, 1, 1],
      [1, 0, 1],
      [1, 1, 2],
      [operand[0], operand[1], operand[2]],
    ],
    [
      [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 18],
    ]
  );
}

function run_wrapped_integer_tests(
  library,
  t,
  name,
  size,
  signed,
  w,
  wname,
  limits
) {
  run_basic_class_tests(t);

  Assert.equal(t.name, name);
  Assert.equal(t.size, size);

  Assert.equal(t.toString(), "type " + name);
  Assert.equal(t.toSource(), "ctypes." + name);
  Assert.ok(t.ptr === ctypes.PointerType(t));
  Assert.equal(t.array().name, name + "[]");
  Assert.equal(t.array(5).name, name + "[5]");

  let d = t();
  Assert.ok(d.value instanceof w);
  Assert.equal(d.value, 0);
  d.value = 5;
  Assert.equal(d.value, 5);
  d = t(10);
  Assert.equal(d.value, 10);
  if (signed) {
    d.value = -10;
    Assert.equal(d.value, -10);
  }
  d = new t(20);
  Assert.equal(d.value, 20);

  d.value = ctypes.Int64(5);
  Assert.equal(d.value, 5);
  if (signed) {
    d.value = ctypes.Int64(-5);
    Assert.equal(d.value, -5);
  }
  d.value = ctypes.UInt64(5);
  Assert.equal(d.value, 5);

  d.value = w(limits[0]);
  Assert.equal(d.value, limits[0]);
  d.value = w(limits[1]);
  Assert.equal(d.value, limits[1]);
  d.value = 0;
  Assert.equal(d.value, 0);
  d.value = -0;
  Assert.equal(1 / d.value, 1 / 0);
  d.value = false;
  Assert.equal(d.value, 0);
  d.value = true;
  Assert.equal(d.value, 1);

  // don't convert anything else
  let vals = [
    limits[2],
    limits[3],
    Infinity,
    -Infinity,
    NaN,
    0.1,
    null,
    undefined,
    "",
    "0",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      d.value = vals[i];
    }, TypeError);
  }

  Assert.ok(d.address().constructor === t.ptr);
  Assert.equal(d.address().contents.toString(), d.value.toString());
  Assert.equal(
    d.toSource(),
    "ctypes." + name + "(" + wname + '("' + d.value + '"))'
  );
  Assert.equal(d.toSource(), d.toString());

  // Test the function call ABI for calls involving the type,
  // and check the alignment of the type against what C says.
  function toprimitive(a) {
    return a.toString();
  }
  run_basic_abi_tests(
    library,
    t,
    name,
    toprimitive,
    109,
    [0, w(limits[0]), w(limits[1])],
    [
      [0, 0, 0],
      [0, 1, 1],
      [1, 0, 1],
      [1, 1, 2],
      signed
        ? [w(limits[0]), -1, w(limits[1])]
        : [w(limits[1]), 1, w(limits[0])],
    ],
    [
      [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 18],
    ]
  );
}

function run_char_tests(library, t, name, size, signed, limits) {
  run_basic_class_tests(t);

  Assert.equal(t.name, name);
  Assert.equal(t.size, size);

  Assert.equal(t.toString(), "type " + name);
  Assert.equal(t.toSource(), "ctypes." + name);
  Assert.ok(t.ptr === ctypes.PointerType(t));
  Assert.equal(t.array().name, name + "[]");
  Assert.equal(t.array(5).name, name + "[5]");

  let d = t();
  Assert.equal(d.value, 0);
  d.value = 5;
  Assert.equal(d.value, 5);
  d = t(10);
  Assert.equal(d.value, 10);
  if (signed) {
    d.value = -10;
    Assert.equal(d.value, -10);
  } else {
    do_check_throws(function() {
      d.value = -10;
    }, TypeError);
  }
  d = new t(20);
  Assert.equal(d.value, 20);

  function toprimitive(a) {
    return a;
  }

  d.value = ctypes.Int64(5);
  Assert.equal(d.value, 5);
  if (signed) {
    d.value = ctypes.Int64(-10);
    Assert.equal(d.value, -10);
  }
  d.value = ctypes.UInt64(5);
  Assert.equal(d.value, 5);

  d.value = limits[0];
  Assert.equal(d.value, limits[0]);
  d.value = limits[1];
  Assert.equal(d.value, limits[1]);
  d.value = 0;
  Assert.equal(d.value, 0);
  d.value = -0;
  Assert.equal(1 / d.value, 1 / 0);
  d.value = false;
  Assert.equal(d.value, 0);
  d.value = true;
  Assert.equal(d.value, 1);

  do_check_throws(function() {
    d.value = "5";
  }, TypeError);

  // don't convert anything else
  let vals = [
    limits[0] - 1,
    limits[1] + 1,
    Infinity,
    -Infinity,
    NaN,
    0.1,
    null,
    undefined,
    "",
    "aa",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      d.value = vals[i];
    }, TypeError);
  }

  Assert.ok(d.address().constructor === t.ptr);
  Assert.equal(d.address().contents, 1);
  Assert.equal(d.toSource(), "ctypes." + name + "(" + d.value + ")");
  Assert.equal(d.toSource(), d.toString());

  // Test string autoconversion (and lack thereof).
  let literal = "autoconverted";
  let s = t.array()(literal);
  Assert.equal(s.readString(), literal);
  Assert.equal(s.constructor.length, literal.length + 1);
  s = t.array(50)(literal);
  Assert.equal(s.readString(), literal);
  do_check_throws(function() {
    t.array(3)(literal);
  }, TypeError);

  do_check_throws(function() {
    t.ptr(literal);
  }, TypeError);
  let p = t.ptr(s);
  Assert.equal(p.readString(), literal);

  // Test the function call ABI for calls involving the type,
  // and check the alignment of the type against what C says.
  run_basic_abi_tests(
    library,
    t,
    name,
    toprimitive,
    109,
    [0, limits[0], limits[1]],
    [
      [0, 0, 0],
      [0, 1, 1],
      [1, 0, 1],
      [1, 1, 2],
      [limits[0], 1, limits[0] + 1],
      [limits[1], 1, limits[0]],
    ],
    [
      [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 18],
    ]
  );
}

function run_char16_tests(library, t, name, limits) {
  run_basic_class_tests(t);

  Assert.equal(t.name, name);
  Assert.equal(t.size, 2);

  Assert.equal(t.toString(), "type " + name);
  Assert.equal(t.toSource(), "ctypes." + name);
  Assert.ok(t.ptr === ctypes.PointerType(t));
  Assert.equal(t.array().name, name + "[]");
  Assert.equal(t.array(5).name, name + "[5]");

  function toprimitive(a) {
    return a.charCodeAt(0);
  }

  let d = t();
  Assert.equal(d.value.length, 1);
  Assert.equal(toprimitive(d.value), 0);
  d.value = 5;
  Assert.equal(d.value.length, 1);
  Assert.equal(toprimitive(d.value), 5);
  d = t(10);
  Assert.equal(toprimitive(d.value), 10);
  do_check_throws(function() {
    d.value = -10;
  }, TypeError);
  d = new t(20);
  Assert.equal(toprimitive(d.value), 20);

  d.value = ctypes.Int64(5);
  Assert.equal(d.value.charCodeAt(0), 5);
  do_check_throws(function() {
    d.value = ctypes.Int64(-10);
  }, TypeError);
  d.value = ctypes.UInt64(5);
  Assert.equal(d.value.charCodeAt(0), 5);

  d.value = limits[0];
  Assert.equal(toprimitive(d.value), limits[0]);
  d.value = limits[1];
  Assert.equal(toprimitive(d.value), limits[1]);
  d.value = 0;
  Assert.equal(toprimitive(d.value), 0);
  d.value = -0;
  Assert.equal(1 / toprimitive(d.value), 1 / 0);
  d.value = false;
  Assert.equal(toprimitive(d.value), 0);
  d.value = true;
  Assert.equal(toprimitive(d.value), 1);

  d.value = "\0";
  Assert.equal(toprimitive(d.value), 0);
  d.value = "a";
  Assert.equal(d.value, "a");

  // don't convert anything else
  let vals = [
    limits[0] - 1,
    limits[1] + 1,
    Infinity,
    -Infinity,
    NaN,
    0.1,
    null,
    undefined,
    "",
    "aa",
    {},
    [],
    new Number(16),
    {
      toString() {
        return 7;
      },
    },
    {
      valueOf() {
        return 7;
      },
    },
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      d.value = vals[i];
    }, TypeError);
  }

  Assert.ok(d.address().constructor === t.ptr);
  Assert.equal(d.address().contents, "a");
  Assert.equal(d.toSource(), "ctypes." + name + '("' + d.value + '")');
  Assert.equal(d.toSource(), d.toString());

  // Test string autoconversion (and lack thereof).
  let literal = "autoconverted";
  let s = t.array()(literal);
  Assert.equal(s.readString(), literal);
  Assert.equal(s.constructor.length, literal.length + 1);
  s = t.array(50)(literal);
  Assert.equal(s.readString(), literal);
  do_check_throws(function() {
    t.array(3)(literal);
  }, TypeError);

  do_check_throws(function() {
    t.ptr(literal);
  }, TypeError);
  let p = t.ptr(s);
  Assert.equal(p.readString(), literal);

  // Test the function call ABI for calls involving the type,
  // and check the alignment of the type against what C says.
  run_basic_abi_tests(
    library,
    t,
    name,
    toprimitive,
    109,
    [0, limits[0], limits[1]],
    [
      [0, 0, 0],
      [0, 1, 1],
      [1, 0, 1],
      [1, 1, 2],
      [limits[0], 1, limits[0] + 1],
      [limits[1], 1, limits[0]],
    ],
    [
      [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
      [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 18],
    ]
  );
}

// Test the class and prototype hierarchy for a given type constructor 'c'.
function run_type_ctor_class_tests(
  c,
  t,
  t2,
  props = [],
  fns = [],
  instanceProps = [],
  instanceFns = [],
  specialProps = []
) {
  Assert.ok(c.prototype.__proto__ === ctypes.CType.prototype);
  Assert.ok(c.prototype instanceof ctypes.CType);
  Assert.ok(c.prototype.constructor === c);

  // Check that 'c.prototype' has the correct properties and functions.
  for (let p of props) {
    Assert.ok(c.prototype.hasOwnProperty(p));
  }
  for (let f of fns) {
    Assert.ok(c.prototype.hasOwnProperty(f));
  }

  // Check that the shared properties and functions on 'c.prototype' throw.
  for (let p of props) {
    do_check_throws(function() {
      c.prototype[p];
    }, TypeError);
  }
  for (let f of fns) {
    do_check_throws(function() {
      c.prototype[f]();
    }, TypeError);
  }

  Assert.ok(t.__proto__ === c.prototype);
  Assert.ok(t instanceof c);

  // 't.prototype.__proto__' is the common ancestor of all types constructed
  // from 'c'; while not available from 'c' directly, it should be identically
  // equal to 't2.prototype.__proto__' where 't2' is a different CType
  // constructed from 'c'.
  Assert.ok(t.prototype.__proto__ === t2.prototype.__proto__);
  if (t instanceof ctypes.FunctionType) {
    Assert.ok(
      t.prototype.__proto__.__proto__ === ctypes.PointerType.prototype.prototype
    );
  } else {
    Assert.ok(t.prototype.__proto__.__proto__ === ctypes.CData.prototype);
  }
  Assert.ok(t.prototype instanceof ctypes.CData);
  Assert.ok(t.prototype.constructor === t);

  // Check that 't.prototype.__proto__' has the correct properties and
  // functions.
  for (let p of instanceProps) {
    Assert.ok(t.prototype.__proto__.hasOwnProperty(p));
  }
  for (let f of instanceFns) {
    Assert.ok(t.prototype.__proto__.hasOwnProperty(f));
  }

  // Check that the shared properties and functions on 't.prototype.__proto__'
  // (and thus also 't.prototype') throw.
  for (let p of instanceProps) {
    do_check_throws(function() {
      t.prototype.__proto__[p];
    }, TypeError);
    do_check_throws(function() {
      t.prototype[p];
    }, TypeError);
  }
  for (let f of instanceFns) {
    do_check_throws(function() {
      t.prototype.__proto__[f]();
    }, TypeError);
    do_check_throws(function() {
      t.prototype[f]();
    }, TypeError);
  }

  // Check that 't.prototype' has the correct special properties.
  for (let p of specialProps) {
    Assert.ok(t.prototype.hasOwnProperty(p));
  }

  // Check that the shared special properties on 't.prototype' throw.
  for (let p of specialProps) {
    do_check_throws(function() {
      t.prototype[p];
    }, TypeError);
  }

  // Make sure we can access 'prototype' on a CTypeProto.
  if (t instanceof ctypes.FunctionType) {
    Assert.ok(
      Object.getPrototypeOf(c.prototype.prototype) ===
        ctypes.PointerType.prototype.prototype
    );
  } else {
    Assert.ok(
      Object.getPrototypeOf(c.prototype.prototype) ===
        ctypes.CType.prototype.prototype
    );
  }

  // Test that an instance 'd' of 't' is a CData.
  if (t.__proto__ != ctypes.FunctionType.prototype) {
    let d = t();
    Assert.ok(d.__proto__ === t.prototype);
    Assert.ok(d instanceof t);
    Assert.ok(d.constructor === t);
  }
}

function run_StructType_tests() {
  run_type_ctor_class_tests(
    ctypes.StructType,
    ctypes.StructType("s", [{ a: ctypes.int32_t }, { b: ctypes.int64_t }]),
    ctypes.StructType("t", [{ c: ctypes.int32_t }, { d: ctypes.int64_t }]),
    ["fields"],
    ["define"],
    [],
    ["addressOfField"],
    ["a", "b"]
  );

  do_check_throws(function() {
    ctypes.StructType();
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", [], 5);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType(null, []);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", null);
  }, TypeError);

  // Check that malformed descriptors are an error.
  do_check_throws(function() {
    ctypes.StructType("a", [{ x: ctypes.int32_t }, { x: ctypes.int8_t }]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", [5]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", [{}]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", [{ 5: ctypes.int32_t }]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", [{ "5": ctypes.int32_t }]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", [{ x: 5 }]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("a", [{ x: ctypes.int32_t() }]);
  }, TypeError);

  // Check that opaque structs work.
  let opaque_t = ctypes.StructType("a");
  Assert.equal(opaque_t.name, "a");
  Assert.equal(opaque_t.toString(), "type a");
  Assert.equal(opaque_t.toSource(), 'ctypes.StructType("a")');
  Assert.ok(opaque_t.prototype === undefined);
  Assert.ok(opaque_t.fields === undefined);
  Assert.ok(opaque_t.size === undefined);
  do_check_throws(function() {
    opaque_t();
  }, Error);
  let opaqueptr_t = opaque_t.ptr;
  Assert.ok(opaqueptr_t.targetType === opaque_t);
  Assert.equal(opaqueptr_t.name, "a*");
  Assert.equal(opaqueptr_t.toString(), "type a*");
  Assert.equal(opaqueptr_t.toSource(), 'ctypes.StructType("a").ptr');

  // Check that type checking works with opaque structs.
  let opaqueptr = opaqueptr_t();
  opaqueptr.value = opaqueptr_t(1);
  Assert.equal(ptrValue(opaqueptr), 1);
  do_check_throws(function() {
    opaqueptr.value = ctypes.StructType("a").ptr();
  }, TypeError);

  // Check that 'define' works.
  do_check_throws(function() {
    opaque_t.define();
  }, TypeError);
  do_check_throws(function() {
    opaque_t.define([], 0);
  }, TypeError);
  do_check_throws(function() {
    opaque_t.define([{}]);
  }, TypeError);
  do_check_throws(function() {
    opaque_t.define([{ a: 0 }]);
  }, TypeError);
  do_check_throws(function() {
    opaque_t.define([{ a: ctypes.int32_t, b: ctypes.int64_t }]);
  }, TypeError);
  do_check_throws(function() {
    opaque_t.define([{ a: ctypes.int32_t }, { b: 0 }]);
  }, TypeError);
  Assert.ok(!opaque_t.hasOwnProperty("prototype"));

  // Check that circular references work with opaque structs...
  // but not crazy ones.
  do_check_throws(function() {
    opaque_t.define([{ b: opaque_t }]);
  }, TypeError);
  let circular_t = ctypes.StructType("circular", [{ a: opaqueptr_t }]);
  opaque_t.define([{ b: circular_t }]);
  let opaque = opaque_t();
  let circular = circular_t(opaque.address());
  opaque.b = circular;
  Assert.equal(circular.a.toSource(), opaque.address().toSource());
  Assert.equal(opaque.b.toSource(), circular.toSource());

  // Check that attempting to redefine a struct fails and if attempted, the
  // original definition is preserved.
  do_check_throws(function() {
    opaque_t.define([{ c: ctypes.int32_t.array(8) }]);
  }, Error);
  Assert.equal(opaque_t.size, circular_t.size);
  Assert.ok(opaque_t.prototype.hasOwnProperty("b"));
  Assert.ok(!opaque_t.prototype.hasOwnProperty("c"));

  // StructType size, alignment, and offset calculations have already been
  // checked for each basic type. We do not need to check them again.
  let name = "g_t";
  let g_t = ctypes.StructType(name, [
    { a: ctypes.int32_t },
    { b: ctypes.double },
  ]);
  Assert.equal(g_t.name, name);

  Assert.equal(g_t.toString(), "type " + name);
  Assert.equal(
    g_t.toSource(),
    'ctypes.StructType("g_t", [{ "a": ctypes.int32_t }, { "b": ctypes.double }])'
  );
  Assert.ok(g_t.ptr === ctypes.PointerType(g_t));
  Assert.equal(g_t.array().name, name + "[]");
  Assert.equal(g_t.array(5).name, name + "[5]");

  let s_t = new ctypes.StructType("s_t", [
    { a: ctypes.int32_t },
    { b: g_t },
    { c: ctypes.int8_t },
  ]);

  let fields = [
    { a: ctypes.int32_t },
    { b: ctypes.int8_t },
    { c: g_t },
    { d: ctypes.int8_t },
  ];
  let t_t = new ctypes.StructType("t_t", fields);
  Assert.equal(t_t.fields.length, 4);
  Assert.ok(t_t.fields[0].a === ctypes.int32_t);
  Assert.ok(t_t.fields[1].b === ctypes.int8_t);
  Assert.ok(t_t.fields[2].c === g_t);
  Assert.ok(t_t.fields[3].d === ctypes.int8_t);
  /* disabled temporarily per bug 598225.
  do_check_throws(function() { t_t.fields.z = 0; }, Error);
  do_check_throws(function() { t_t.fields[4] = 0; }, Error);
  do_check_throws(function() { t_t.fields[4].a = 0; }, Error);
  do_check_throws(function() { t_t.fields[4].e = 0; }, Error);
*/

  // Check that struct size bounds work, and that large, but not illegal, sizes
  // are OK.
  if (ctypes.size_t.size == 4) {
    // Test 1: overflow struct size + field padding + field size.
    let large_t = ctypes.StructType("large_t", [
      { a: ctypes.int8_t.array(0xffffffff) },
    ]);
    Assert.equal(large_t.size, 0xffffffff);
    do_check_throws(function() {
      ctypes.StructType("large_t", [{ a: large_t }, { b: ctypes.int8_t }]);
    }, RangeError);

    // Test 2: overflow struct size + struct tail padding.
    // To do this, we use a struct with maximum size and alignment 2.
    large_t = ctypes.StructType("large_t", [
      { a: ctypes.int16_t.array(0xfffffffe / 2) },
    ]);
    Assert.equal(large_t.size, 0xfffffffe);
    do_check_throws(function() {
      ctypes.StructType("large_t", [{ a: large_t }, { b: ctypes.int8_t }]);
    }, RangeError);
  } else {
    // Test 1: overflow struct size when converting from size_t to jsdouble.
    let large_t = ctypes.StructType("large_t", [
      { a: ctypes.int8_t.array(0xfffffffffffff800) },
    ]);
    Assert.equal(large_t.size, 0xfffffffffffff800);
    do_check_throws(function() {
      ctypes.StructType("large_t", [{ a: large_t }, { b: ctypes.int8_t }]);
    }, RangeError);
    let small_t = ctypes.int8_t.array(0x400);
    do_check_throws(function() {
      ctypes.StructType("large_t", [{ a: large_t }, { b: small_t }]);
    }, RangeError);

    large_t = ctypes.StructType("large_t", [
      { a: ctypes.int8_t.array(0x1fffffffffffff) },
    ]);
    Assert.equal(large_t.size, 0x1fffffffffffff);
    do_check_throws(function() {
      ctypes.StructType("large_t", [
        { a: large_t.array(2) },
        { b: ctypes.int8_t },
      ]);
    }, RangeError);

    // Test 2: overflow struct size + field padding + field size.
    large_t = ctypes.int8_t.array(0xfffffffffffff800);
    small_t = ctypes.int8_t.array(0x800);
    do_check_throws(function() {
      ctypes.StructType("large_t", [{ a: large_t }, { b: small_t }]);
    }, RangeError);

    // Test 3: overflow struct size + struct tail padding.
    // To do this, we use a struct with maximum size and alignment 2.
    large_t = ctypes.StructType("large_t", [
      { a: ctypes.int16_t.array(0xfffffffffffff000 / 2) },
    ]);
    Assert.equal(large_t.size, 0xfffffffffffff000);
    small_t = ctypes.int8_t.array(0xfff);
    do_check_throws(function() {
      ctypes.StructType("large_t", [{ a: large_t }, { b: small_t }]);
    }, RangeError);
  }

  let g = g_t();
  Assert.equal(g.a, 0);
  Assert.equal(g.b, 0);
  g = new g_t(1, 2);
  Assert.equal(g.a, 1);
  Assert.equal(g.b, 2);
  do_check_throws(function() {
    g_t(1);
  }, TypeError);
  do_check_throws(function() {
    g_t(1, 2, 3);
  }, TypeError);

  for (let field in g) {
    Assert.ok(field == "a" || field == "b");
  }

  let g_a = g.address();
  Assert.ok(g_a.constructor === g_t.ptr);
  Assert.equal(g_a.contents.a, g.a);

  let s = new s_t(3, g, 10);
  Assert.equal(s.a, 3);
  s.a = 4;
  Assert.equal(s.a, 4);
  Assert.equal(s.b.a, 1);
  Assert.equal(s.b.b, 2);
  Assert.equal(s.c, 10);
  let g2 = s.b;
  Assert.equal(g2.a, 1);
  g2.a = 7;
  Assert.equal(g2.a, 7);
  Assert.equal(s.b.a, 7);

  g_a = s.addressOfField("b");
  Assert.ok(g_a.constructor === g_t.ptr);
  Assert.equal(g_a.contents.a, s.b.a);
  do_check_throws(function() {
    s.addressOfField();
  }, TypeError);
  do_check_throws(function() {
    s.addressOfField("d");
  }, TypeError);
  do_check_throws(function() {
    s.addressOfField("a", 2);
  }, TypeError);

  Assert.equal(s.toSource(), 's_t(4, {"a": 7, "b": 2}, 10)');
  Assert.equal(s.toSource(), s.toString());
  // eslint-disable-next-line no-eval
  var s2 = eval(s.toSource());
  Assert.ok(s2.constructor === s_t);
  Assert.equal(s.b.b, s2.b.b);

  // Test that structs can be set from an object using 'value'.
  do_check_throws(function() {
    s.value;
  }, TypeError);
  let s_init = { a: 2, b: { a: 9, b: 5 }, c: 13 };
  s.value = s_init;
  Assert.equal(s.b.a, 9);
  Assert.equal(s.c, 13);
  do_check_throws(function() {
    s.value = 5;
  }, TypeError);
  do_check_throws(function() {
    s.value = ctypes.int32_t();
  }, TypeError);
  do_check_throws(function() {
    s.value = {};
  }, TypeError);
  do_check_throws(function() {
    s.value = { a: 2 };
  }, TypeError);
  do_check_throws(function() {
    s.value = { a: 2, b: 5, c: 10 };
  }, TypeError);
  do_check_throws(function() {
    s.value = { "5": 2, b: { a: 9, b: 5 }, c: 13 };
  }, TypeError);
  do_check_throws(function() {
    s.value = { a: 2, b: { a: 9, b: 5 }, c: 13, d: 17 };
  }, TypeError);
  do_check_throws(function() {
    s.value = { a: 2, b: { a: 9, b: 5, e: 9 }, c: 13 };
  }, TypeError);

  // Test that structs can be constructed similarly through ExplicitConvert,
  // and that the single-field case is disambiguated correctly.
  s = s_t(s_init);
  Assert.equal(s.b.a, 9);
  Assert.equal(s.c, 13);
  let v_t = ctypes.StructType("v_t", [{ x: ctypes.int32_t }]);
  let v = v_t({ x: 5 });
  Assert.equal(v.x, 5);
  v = v_t(8);
  Assert.equal(v.x, 8);
  let w_t = ctypes.StructType("w_t", [{ y: v_t }]);
  do_check_throws(function() {
    w_t(9);
  }, TypeError);
  let w = w_t({ x: 3 });
  Assert.equal(w.y.x, 3);
  w = w_t({ y: { x: 19 } });
  Assert.equal(w.y.x, 19);
  let u_t = ctypes.StructType("u_t", [
    { z: ctypes.ArrayType(ctypes.int32_t, 3) },
  ]);
  let u = u_t([1, 2, 3]);
  Assert.equal(u.z[1], 2);
  u = u_t({ z: [4, 5, 6] });
  Assert.equal(u.z[1], 5);

  // Check that the empty struct has size 1.
  let z_t = ctypes.StructType("z_t", []);
  Assert.equal(z_t.size, 1);
  Assert.equal(z_t.fields.length, 0);

  // Check that structs containing arrays of undefined or zero length
  // are illegal, but arrays of defined length work.
  do_check_throws(function() {
    ctypes.StructType("z_t", [{ a: ctypes.int32_t.array() }]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.StructType("z_t", [{ a: ctypes.int32_t.array(0) }]);
  }, TypeError);
  z_t = ctypes.StructType("z_t", [{ a: ctypes.int32_t.array(6) }]);
  Assert.equal(z_t.size, ctypes.int32_t.size * 6);
  let z = z_t([1, 2, 3, 4, 5, 6]);
  Assert.equal(z.a[3], 4);
}

function ptrValue(p) {
  return ctypes.cast(p, ctypes.uintptr_t).value.toString();
}

function run_PointerType_tests() {
  run_type_ctor_class_tests(
    ctypes.PointerType,
    ctypes.PointerType(ctypes.int32_t),
    ctypes.PointerType(ctypes.int64_t),
    ["targetType"],
    [],
    ["contents"],
    ["isNull", "increment", "decrement"],
    []
  );

  do_check_throws(function() {
    ctypes.PointerType();
  }, TypeError);
  do_check_throws(function() {
    ctypes.PointerType(ctypes.int32_t, 5);
  }, TypeError);
  do_check_throws(function() {
    ctypes.PointerType(null);
  }, TypeError);
  do_check_throws(function() {
    ctypes.PointerType(ctypes.int32_t());
  }, TypeError);
  do_check_throws(function() {
    ctypes.PointerType("void");
  }, TypeError);

  let name = "g_t";
  let g_t = ctypes.StructType(name, [
    { a: ctypes.int32_t },
    { b: ctypes.double },
  ]);
  let g = g_t(1, 2);

  let p_t = ctypes.PointerType(g_t);
  Assert.equal(p_t.name, name + "*");
  Assert.equal(p_t.size, ctypes.uintptr_t.size);
  Assert.ok(p_t.targetType === g_t);
  Assert.ok(p_t === g_t.ptr);

  Assert.equal(p_t.toString(), "type " + name + "*");
  Assert.equal(
    p_t.toSource(),
    'ctypes.StructType("g_t", [{ "a": ctypes.int32_t }, { "b": ctypes.double }]).ptr'
  );
  Assert.ok(p_t.ptr === ctypes.PointerType(p_t));
  Assert.equal(p_t.array().name, name + "*[]");
  Assert.equal(p_t.array(5).name, name + "*[5]");

  // Test ExplicitConvert.
  let p = p_t();
  do_check_throws(function() {
    p.value;
  }, TypeError);
  Assert.equal(ptrValue(p), 0);
  do_check_throws(function() {
    p.contents;
  }, TypeError);
  do_check_throws(function() {
    p.contents = g;
  }, TypeError);
  p = p_t(5);
  Assert.equal(ptrValue(p), 5);
  p = p_t(ctypes.UInt64(10));
  Assert.equal(ptrValue(p), 10);

  // Test ImplicitConvert.
  p.value = null;
  Assert.equal(ptrValue(p), 0);
  do_check_throws(function() {
    p.value = 5;
  }, TypeError);

  // Test opaque pointers.
  let f_t = ctypes.StructType("FILE").ptr;
  Assert.equal(f_t.name, "FILE*");
  Assert.equal(f_t.toSource(), 'ctypes.StructType("FILE").ptr');
  let f = new f_t();
  do_check_throws(function() {
    f.contents;
  }, TypeError);
  do_check_throws(function() {
    f.contents = 0;
  }, TypeError);
  f = f_t(5);
  do_check_throws(function() {
    f.contents = 0;
  }, TypeError);
  Assert.equal(f.toSource(), 'FILE.ptr(ctypes.UInt64("0x5"))');

  do_check_throws(function() {
    f_t(p);
  }, TypeError);
  do_check_throws(function() {
    f.value = p;
  }, TypeError);
  do_check_throws(function() {
    p.value = f;
  }, TypeError);

  // Test void pointers.
  let v_t = ctypes.PointerType(ctypes.void_t);
  Assert.ok(v_t === ctypes.voidptr_t);
  let v = v_t(p);
  Assert.equal(ptrValue(v), ptrValue(p));

  // Test 'contents'.
  let int32_t = ctypes.int32_t(9);
  p = int32_t.address();
  Assert.equal(p.contents, int32_t.value);
  p.contents = ctypes.int32_t(12);
  Assert.equal(int32_t.value, 12);

  // Test 'isNull'.
  let n = f_t(0);
  Assert.ok(n.isNull() === true);
  n = p.address();
  Assert.ok(n.isNull() === false);

  // Test 'increment'/'decrement'.
  g_t = ctypes.StructType("g_t", [{ a: ctypes.int32_t }, { b: ctypes.double }]);
  let a_t = ctypes.ArrayType(g_t, 2);
  let a = new a_t();
  a[0] = g_t(1, 2);
  a[1] = g_t(2, 4);
  let a_p = a.addressOfElement(0).increment();
  Assert.equal(a_p.contents.a, 2);
  Assert.equal(a_p.contents.b, 4);
  a_p = a_p.decrement();
  Assert.equal(a_p.contents.a, 1);
  Assert.equal(a_p.contents.b, 2);

  // Check that pointers to arrays of undefined or zero length are legal,
  // but that the former cannot be dereferenced.
  let z_t = ctypes.int32_t.array().ptr;
  Assert.equal(ptrValue(z_t()), 0);
  do_check_throws(function() {
    z_t().contents;
  }, TypeError);
  z_t = ctypes.int32_t.array(0).ptr;
  Assert.equal(ptrValue(z_t()), 0);
  let z = ctypes.int32_t
    .array(0)()
    .address();
  Assert.equal(z.contents.length, 0);

  // TODO: Somehow, somewhere we should check that:
  //
  //  (a) ArrayBuffer and TypedArray can be passed by pointer to a C function
  //  (b) SharedArrayBuffer and TypedArray on SAB can NOT be passed in that
  //      way (at least not at the moment).

  // Set up conversion tests on AB, SAB, TA
  let c_arraybuffer = new ArrayBuffer(256);
  let typed_array_samples = [
    [new Int8Array(c_arraybuffer), ctypes.int8_t],
    [new Uint8Array(c_arraybuffer), ctypes.uint8_t],
    [new Int16Array(c_arraybuffer), ctypes.int16_t],
    [new Uint16Array(c_arraybuffer), ctypes.uint16_t],
    [new Int32Array(c_arraybuffer), ctypes.int32_t],
    [new Uint32Array(c_arraybuffer), ctypes.uint32_t],
    [new Float32Array(c_arraybuffer), ctypes.float32_t],
    [new Float64Array(c_arraybuffer), ctypes.float64_t],
  ];

  if (typeof SharedArrayBuffer !== "undefined") {
    let c_shared_arraybuffer = new SharedArrayBuffer(256);
    typed_array_samples.push(
      [new Int8Array(c_shared_arraybuffer), ctypes.int8_t],
      [new Uint8Array(c_shared_arraybuffer), ctypes.uint8_t],
      [new Int16Array(c_shared_arraybuffer), ctypes.int16_t],
      [new Uint16Array(c_shared_arraybuffer), ctypes.uint16_t],
      [new Int32Array(c_shared_arraybuffer), ctypes.int32_t],
      [new Uint32Array(c_shared_arraybuffer), ctypes.uint32_t],
      [new Float32Array(c_shared_arraybuffer), ctypes.float32_t],
      [new Float64Array(c_shared_arraybuffer), ctypes.float64_t]
    );
  }

  // Check that you can convert (Shared)ArrayBuffer or typed array to a C array
  for (let i = 0; i < typed_array_samples.length; ++i) {
    for (let j = 0; j < typed_array_samples.length; ++j) {
      let view = typed_array_samples[i][0];
      let item_type = typed_array_samples[j][1];
      let number_of_items = c_arraybuffer.byteLength / item_type.size;
      let array_type = item_type.array(number_of_items);

      // Int8Array on unshared memory is interconvertible with Int8Array on
      // shared memory, etc.
      if (i % 8 != j % 8) {
        info(
          "Checking that typed array " +
            view.constructor.name +
            " can NOT be converted to " +
            item_type +
            " array"
        );
        do_check_throws(function() {
          array_type(view);
        }, TypeError);
      } else {
        info(
          "Checking that typed array " +
            view.constructor.name +
            " can be converted to " +
            item_type +
            " array"
        );

        // Convert ArrayBuffer to array of the right size and check contents
        let c_array = array_type(c_arraybuffer);
        for (let k = 0; k < number_of_items; ++k) {
          Assert.equal(c_array[k], view[k]);
        }

        // Convert typed array to array of the right size and check contents
        c_array = array_type(view);
        for (let k = 0; k < number_of_items; ++k) {
          Assert.equal(c_array[k], view[k]);
        }

        // Convert typed array to array of wrong size, ensure that it fails
        let array_type_too_large = item_type.array(number_of_items + 1);
        let array_type_too_small = item_type.array(number_of_items - 1);

        do_check_throws(function() {
          array_type_too_large(c_arraybuffer);
        }, TypeError);
        do_check_throws(function() {
          array_type_too_small(c_arraybuffer);
        }, TypeError);
        do_check_throws(function() {
          array_type_too_large(view);
        }, TypeError);
        do_check_throws(function() {
          array_type_too_small(view);
        }, TypeError);

        // Convert subarray of typed array to array of right size and check contents
        c_array = array_type_too_small(view.subarray(1));
        for (let k = 1; k < number_of_items; ++k) {
          Assert.equal(c_array[k - 1], view[k]);
        }
      }
    }
  }

  // Check that you can't use a (Shared)ArrayBuffer or a typed array as a pointer
  for (let i = 0; i < typed_array_samples.length; ++i) {
    for (let j = 0; j < typed_array_samples.length; ++j) {
      let view = typed_array_samples[i][0];
      let item_type = typed_array_samples[j][1];

      info(
        "Checking that typed array " +
          view.constructor.name +
          " can NOT be converted to " +
          item_type +
          " pointer/array"
      );
      do_check_throws(function() {
        item_type.ptr(c_arraybuffer);
      }, TypeError);
      do_check_throws(function() {
        item_type.ptr(view);
      }, TypeError);
      do_check_throws(function() {
        ctypes.voidptr_t(c_arraybuffer);
      }, TypeError);
      do_check_throws(function() {
        ctypes.voidptr_t(view);
      }, TypeError);
    }
  }
}

function run_FunctionType_tests() {
  run_type_ctor_class_tests(
    ctypes.FunctionType,
    ctypes.FunctionType(ctypes.default_abi, ctypes.void_t),
    ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, [ctypes.int32_t]),
    ["abi", "returnType", "argTypes", "isVariadic"],
    undefined,
    undefined,
    undefined,
    undefined
  );

  do_check_throws(function() {
    ctypes.FunctionType();
  }, TypeError);
  do_check_throws(function() {
    ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, [ctypes.void_t]);
  }, TypeError);
  do_check_throws(function() {
    ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, [ctypes.void_t], 5);
  }, TypeError);
  do_check_throws(function() {
    ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, ctypes.void_t);
  }, TypeError);
  do_check_throws(function() {
    ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, null);
  }, TypeError);
  do_check_throws(function() {
    ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t());
  }, TypeError);
  do_check_throws(function() {
    ctypes.FunctionType(ctypes.void_t, ctypes.void_t);
  }, Error);

  let g_t = ctypes.StructType("g_t", [
    { a: ctypes.int32_t },
    { b: ctypes.double },
  ]);

  let f_t = ctypes.FunctionType(ctypes.default_abi, g_t);
  let name = "g_t()";
  Assert.equal(f_t.name, name);
  Assert.equal(f_t.size, undefined);
  Assert.ok(f_t.abi === ctypes.default_abi);
  Assert.ok(f_t.returnType === g_t);
  Assert.ok(!f_t.argTypes.length);

  Assert.equal(f_t.toString(), "type " + name);
  Assert.equal(f_t.toSource(), "ctypes.FunctionType(ctypes.default_abi, g_t)");

  let fp_t = f_t.ptr;
  name = "g_t(*)()";
  Assert.equal(fp_t.name, name);
  Assert.equal(fp_t.size, ctypes.uintptr_t.size);

  Assert.equal(fp_t.toString(), "type " + name);
  Assert.equal(
    fp_t.toSource(),
    "ctypes.FunctionType(ctypes.default_abi, g_t).ptr"
  );

  // Check that constructing a FunctionType CData directly throws.
  do_check_throws(function() {
    f_t();
  }, TypeError);

  // Test ExplicitConvert.
  let f = fp_t();
  do_check_throws(function() {
    f.value;
  }, TypeError);
  Assert.equal(ptrValue(f), 0);
  f = fp_t(5);
  Assert.equal(ptrValue(f), 5);
  f = fp_t(ctypes.UInt64(10));
  Assert.equal(ptrValue(f), 10);

  // Test ImplicitConvert.
  f.value = null;
  Assert.equal(ptrValue(f), 0);
  do_check_throws(function() {
    f.value = 5;
  }, TypeError);
  Assert.equal(
    f.toSource(),
    'ctypes.FunctionType(ctypes.default_abi, g_t).ptr(ctypes.UInt64("0x0"))'
  );

  // Test ImplicitConvert from a function pointer of different type.
  let f2_t = ctypes.FunctionType(ctypes.default_abi, g_t, [ctypes.int32_t]);
  let f2 = f2_t.ptr();
  do_check_throws(function() {
    f.value = f2;
  }, TypeError);
  do_check_throws(function() {
    f2.value = f;
  }, TypeError);

  // Test that converting to a voidptr_t works.
  let v = ctypes.voidptr_t(f2);
  Assert.equal(v.toSource(), 'ctypes.voidptr_t(ctypes.UInt64("0x0"))');

  // Test some more complex names.
  Assert.equal(fp_t.array().name, "g_t(*[])()");
  Assert.equal(fp_t.array().ptr.name, "g_t(*(*)[])()");

  let f3_t = ctypes
    .FunctionType(ctypes.default_abi, ctypes.char.ptr.array().ptr)
    .ptr.ptr.array(8)
    .array();
  Assert.equal(f3_t.name, "char*(*(**[][8])())[]");

  if ("winLastError" in ctypes) {
    f3_t = ctypes
      .FunctionType(ctypes.stdcall_abi, ctypes.char.ptr.array().ptr)
      .ptr.ptr.array(8)
      .array();
    Assert.equal(f3_t.name, "char*(*(__stdcall**[][8])())[]");
    f3_t = ctypes
      .FunctionType(ctypes.winapi_abi, ctypes.char.ptr.array().ptr)
      .ptr.ptr.array(8)
      .array();
    Assert.equal(f3_t.name, "char*(*(WINAPI**[][8])())[]");
  }

  let f4_t = ctypes.FunctionType(
    ctypes.default_abi,
    ctypes.char.ptr.array().ptr,
    [ctypes.int32_t, fp_t]
  );
  Assert.ok(f4_t.argTypes.length == 2);
  Assert.ok(f4_t.argTypes[0] === ctypes.int32_t);
  Assert.ok(f4_t.argTypes[1] === fp_t);
  /* disabled temporarily per bug 598225.
  do_check_throws(function() { f4_t.argTypes.z = 0; }, Error);
  do_check_throws(function() { f4_t.argTypes[0] = 0; }, Error);
*/

  let t4_t = f4_t.ptr.ptr.array(8).array();
  Assert.equal(t4_t.name, "char*(*(**[][8])(int32_t, g_t(*)()))[]");

  // Not available in a Worker
  // eslint-disable-next-line mozilla/use-cc-etc
  if ("@mozilla.org/systemprincipal;1" in Components.classes) {
    var sp = Cc["@mozilla.org/systemprincipal;1"].createInstance(
      Ci.nsIPrincipal
    );
    var s = new Cu.Sandbox(sp);
    s.ctypes = ctypes;
    s.equal = equal;
    s.ok = ok;
    Cu.evalInSandbox(
      "var f5_t = ctypes.FunctionType(ctypes.default_abi, ctypes.int, [ctypes.int]);",
      s
    );
    Cu.evalInSandbox(
      "equal(f5_t.toSource(), 'ctypes.FunctionType(ctypes.default_abi, ctypes.int, [ctypes.int])');",
      s
    );
    Cu.evalInSandbox("equal(f5_t.name, 'int(int)');", s);
    Cu.evalInSandbox("function f5(aArg) { return 5; };", s);
    Cu.evalInSandbox("var f = f5_t.ptr(f5);", s);
    Cu.evalInSandbox("ok(f(6) == 5);", s);
  }
}

function run_ArrayType_tests() {
  run_type_ctor_class_tests(
    ctypes.ArrayType,
    ctypes.ArrayType(ctypes.int32_t, 10),
    ctypes.ArrayType(ctypes.int64_t),
    ["elementType", "length"],
    [],
    ["length"],
    ["addressOfElement"]
  );

  do_check_throws(function() {
    ctypes.ArrayType();
  }, TypeError);
  do_check_throws(function() {
    ctypes.ArrayType(null);
  }, TypeError);
  do_check_throws(function() {
    ctypes.ArrayType(ctypes.int32_t, 1, 5);
  }, TypeError);
  do_check_throws(function() {
    ctypes.ArrayType(ctypes.int32_t, -1);
  }, TypeError);

  let name = "g_t";
  let g_t = ctypes.StructType(name, [
    { a: ctypes.int32_t },
    { b: ctypes.double },
  ]);
  let g = g_t(1, 2);

  let a_t = ctypes.ArrayType(g_t, 10);
  Assert.equal(a_t.name, name + "[10]");
  Assert.equal(a_t.length, 10);
  Assert.equal(a_t.size, g_t.size * 10);
  Assert.ok(a_t.elementType === g_t);

  Assert.equal(a_t.toString(), "type " + name + "[10]");
  Assert.equal(
    a_t.toSource(),
    'ctypes.StructType("g_t", [{ "a": ctypes.int32_t }, { "b": ctypes.double }]).array(10)'
  );
  Assert.equal(a_t.array().name, name + "[][10]");
  Assert.equal(a_t.array(5).name, name + "[5][10]");
  do_check_throws(function() {
    ctypes.int32_t.array().array();
  }, Error);

  let a = new a_t();
  Assert.equal(a[0].a, 0);
  Assert.equal(a[0].b, 0);
  a[0] = g;
  Assert.equal(a[0].a, 1);
  Assert.equal(a[0].b, 2);
  do_check_throws(function() {
    a[-1];
  }, TypeError);
  Assert.equal(a[9].a, 0);
  do_check_throws(function() {
    a[10];
  }, RangeError);

  Assert.equal(a[ctypes.Int64(0)].a, 1);
  Assert.equal(a[ctypes.UInt64(0)].b, 2);

  let a_p = a.addressOfElement(0);
  Assert.ok(a_p.constructor.targetType === g_t);
  Assert.ok(a_p.constructor === g_t.ptr);
  Assert.equal(a_p.contents.a, a[0].a);
  Assert.equal(a_p.contents.b, a[0].b);
  a_p.contents.a = 5;
  Assert.equal(a[0].a, 5);

  let a2_t = ctypes.ArrayType(g_t);
  Assert.equal(a2_t.name, "g_t[]");
  Assert.equal(a2_t.length, undefined);
  Assert.equal(a2_t.size, undefined);
  let a2 = new a2_t(5);
  Assert.equal(a2.constructor.length, 5);
  Assert.equal(a2.length, 5);
  Assert.equal(a2.constructor.size, g_t.size * 5);
  do_check_throws(function() {
    new a2_t();
  }, TypeError);
  do_check_throws(function() {
    ctypes.ArrayType(ctypes.ArrayType(g_t));
  }, Error);
  do_check_throws(function() {
    ctypes.ArrayType(ctypes.ArrayType(g_t), 5);
  }, Error);

  let b_t = ctypes.int8_t.array(ctypes.UInt64(0xffff));
  Assert.equal(b_t.length, 0xffff);
  b_t = ctypes.int8_t.array(ctypes.Int64(0xffff));
  Assert.equal(b_t.length, 0xffff);

  // Check that array size bounds work, and that large, but not illegal, sizes
  // are OK.
  if (ctypes.size_t.size == 4) {
    do_check_throws(function() {
      ctypes.ArrayType(ctypes.int8_t, 0x100000000);
    }, TypeError);
    do_check_throws(function() {
      ctypes.ArrayType(ctypes.int16_t, 0x80000000);
    }, RangeError);

    let large_t = ctypes.int8_t.array(0x80000000);
    do_check_throws(function() {
      large_t.array(2);
    }, RangeError);
  } else {
    do_check_throws(function() {
      ctypes.ArrayType(ctypes.int8_t, ctypes.UInt64("0xffffffffffffffff"));
    }, TypeError);
    do_check_throws(function() {
      ctypes.ArrayType(ctypes.int16_t, ctypes.UInt64("0x8000000000000000"));
    }, RangeError);

    let large_t = ctypes.int8_t.array(0x8000000000000000);
    do_check_throws(function() {
      large_t.array(2);
    }, RangeError);
  }

  // Test that arrays ImplicitConvert to pointers.
  let b = ctypes.int32_t.array(10)();
  let p = ctypes.int32_t.ptr();
  p.value = b;
  Assert.equal(ptrValue(b.addressOfElement(0)), ptrValue(p));
  p = ctypes.voidptr_t();
  p.value = b;
  Assert.equal(ptrValue(b.addressOfElement(0)), ptrValue(p));

  // Test that arrays can be constructed through ImplicitConvert.
  let c_t = ctypes.int32_t.array(6);
  let c = c_t();
  c.value = [1, 2, 3, 4, 5, 6];
  Assert.equal(c.toSource(), "ctypes.int32_t.array(6)([1, 2, 3, 4, 5, 6])");
  Assert.equal(c.toSource(), c.toString());
  // eslint-disable-next-line no-eval
  var c2 = eval(c.toSource());
  Assert.equal(c2.constructor.name, "int32_t[6]");
  Assert.equal(c2.length, 6);
  Assert.equal(c2[3], c[3]);

  c.value = c;
  Assert.equal(c[3], 4);
  do_check_throws(function() {
    c.value;
  }, TypeError);
  do_check_throws(function() {
    c.value = [1, 2, 3, 4, 5];
  }, TypeError);
  do_check_throws(function() {
    c.value = [1, 2, 3, 4, 5, 6, 7];
  }, TypeError);
  do_check_throws(function() {
    c.value = [1, 2, 7.4, 4, 5, 6];
  }, TypeError);
  do_check_throws(function() {
    c.value = [];
  }, TypeError);
}

function run_type_toString_tests() {
  var c = ctypes;

  // Figure out whether we can create functions with ctypes.stdcall_abi and ctypes.winapi_abi.
  var haveStdCallABI;
  try {
    c.FunctionType(c.stdcall_abi, c.int);
    haveStdCallABI = true;
  } catch (x) {
    haveStdCallABI = false;
  }

  var haveWinAPIABI;
  try {
    c.FunctionType(c.winapi_abi, c.int);
    haveWinAPIABI = true;
  } catch (x) {
    haveWinAPIABI = false;
  }

  Assert.equal(c.char.toString(), "type char");
  Assert.equal(c.short.toString(), "type short");
  Assert.equal(c.int.toString(), "type int");
  Assert.equal(c.long.toString(), "type long");
  Assert.equal(c.long_long.toString(), "type long_long");
  Assert.equal(c.ssize_t.toString(), "type ssize_t");
  Assert.equal(c.int8_t.toString(), "type int8_t");
  Assert.equal(c.int16_t.toString(), "type int16_t");
  Assert.equal(c.int32_t.toString(), "type int32_t");
  Assert.equal(c.int64_t.toString(), "type int64_t");
  Assert.equal(c.intptr_t.toString(), "type intptr_t");

  Assert.equal(c.unsigned_char.toString(), "type unsigned_char");
  Assert.equal(c.unsigned_short.toString(), "type unsigned_short");
  Assert.equal(c.unsigned_int.toString(), "type unsigned_int");
  Assert.equal(c.unsigned_long.toString(), "type unsigned_long");
  Assert.equal(c.unsigned_long_long.toString(), "type unsigned_long_long");
  Assert.equal(c.size_t.toString(), "type size_t");
  Assert.equal(c.uint8_t.toString(), "type uint8_t");
  Assert.equal(c.uint16_t.toString(), "type uint16_t");
  Assert.equal(c.uint32_t.toString(), "type uint32_t");
  Assert.equal(c.uint64_t.toString(), "type uint64_t");
  Assert.equal(c.uintptr_t.toString(), "type uintptr_t");

  Assert.equal(c.float.toString(), "type float");
  Assert.equal(c.double.toString(), "type double");
  Assert.equal(c.bool.toString(), "type bool");
  Assert.equal(c.void_t.toString(), "type void");
  Assert.equal(c.voidptr_t.toString(), "type void*");
  Assert.equal(c.char16_t.toString(), "type char16_t");

  var simplestruct = c.StructType("simplestruct", [{ smitty: c.voidptr_t }]);
  Assert.equal(simplestruct.toString(), "type simplestruct");

  // One type modifier, int base type.
  Assert.equal(c.int.ptr.toString(), "type int*");
  Assert.equal(c.ArrayType(c.int).toString(), "type int[]");
  Assert.equal(c.ArrayType(c.int, 4).toString(), "type int[4]");
  Assert.equal(c.FunctionType(c.default_abi, c.int).toString(), "type int()");
  Assert.equal(
    c.FunctionType(c.default_abi, c.int, [c.bool]).toString(),
    "type int(bool)"
  );
  Assert.equal(
    c.FunctionType(c.default_abi, c.int, [c.bool, c.short]).toString(),
    "type int(bool, short)"
  );
  if (haveStdCallABI) {
    Assert.equal(
      c.FunctionType(c.stdcall_abi, c.int).toString(),
      "type int __stdcall()"
    );
  }
  if (haveWinAPIABI) {
    Assert.equal(
      c.FunctionType(c.winapi_abi, c.int).toString(),
      "type int WINAPI()"
    );
  }

  // One type modifier, struct base type.
  Assert.equal(simplestruct.ptr.toString(), "type simplestruct*");
  Assert.equal(c.ArrayType(simplestruct).toString(), "type simplestruct[]");
  Assert.equal(c.ArrayType(simplestruct, 4).toString(), "type simplestruct[4]");
  Assert.equal(
    c.FunctionType(c.default_abi, simplestruct).toString(),
    "type simplestruct()"
  );

  // Two levels of type modifiers, int base type.
  Assert.equal(c.int.ptr.ptr.toString(), "type int**");
  Assert.equal(c.ArrayType(c.int.ptr).toString(), "type int*[]");
  Assert.equal(
    c.FunctionType(c.default_abi, c.int.ptr).toString(),
    "type int*()"
  );

  Assert.equal(c.ArrayType(c.int).ptr.toString(), "type int(*)[]");
  Assert.equal(c.ArrayType(c.ArrayType(c.int, 4)).toString(), "type int[][4]");
  // Functions can't return arrays.

  Assert.equal(
    c.FunctionType(c.default_abi, c.int).ptr.toString(),
    "type int(*)()"
  );
  // You can't have an array of functions.
  // Functions can't return functions.

  // We don't try all the permissible three-deep combinations, but this is fun.
  Assert.equal(
    c
      .FunctionType(c.default_abi, c.FunctionType(c.default_abi, c.int).ptr)
      .toString(),
    "type int(*())()"
  );
}

function run_cast_tests() {
  // Test casting between basic types.
  let i = ctypes.int32_t();
  let j = ctypes.cast(i, ctypes.int16_t);
  Assert.equal(ptrValue(i.address()), ptrValue(j.address()));
  Assert.equal(i.value, j.value);
  let k = ctypes.cast(i, ctypes.uint32_t);
  Assert.equal(ptrValue(i.address()), ptrValue(k.address()));
  Assert.equal(i.value, k.value);

  // Test casting to a type of undefined or larger size.
  do_check_throws(function() {
    ctypes.cast(i, ctypes.void_t);
  }, TypeError);
  do_check_throws(function() {
    ctypes.cast(i, ctypes.int32_t.array());
  }, TypeError);
  do_check_throws(function() {
    ctypes.cast(i, ctypes.int64_t);
  }, TypeError);

  // Test casting between special types.
  let g_t = ctypes.StructType("g_t", [
    { a: ctypes.int32_t },
    { b: ctypes.double },
  ]);
  let a_t = ctypes.ArrayType(g_t, 4);
  let f_t = ctypes.FunctionType(ctypes.default_abi, ctypes.void_t).ptr;

  let a = a_t();
  a[0] = { a: 5, b: 7.5 };
  let g = ctypes.cast(a, g_t);
  Assert.equal(ptrValue(a.address()), ptrValue(g.address()));
  Assert.equal(a[0].a, g.a);

  let a2 = ctypes.cast(g, g_t.array(1));
  Assert.equal(ptrValue(a2.address()), ptrValue(g.address()));
  Assert.equal(a2[0].a, g.a);

  let p = g.address();
  let ip = ctypes.cast(p, ctypes.int32_t.ptr);
  Assert.equal(ptrValue(ip), ptrValue(p));
  Assert.equal(ptrValue(ip.address()), ptrValue(p.address()));
  Assert.equal(ip.contents, g.a);

  let f = f_t(0x5);
  let f2 = ctypes.cast(f, ctypes.voidptr_t);
  Assert.equal(ptrValue(f2), ptrValue(f));
  Assert.equal(ptrValue(f2.address()), ptrValue(f.address()));
}

function run_void_tests(library) {
  let test_void_t = library.declare(
    "test_void_t_cdecl",
    ctypes.default_abi,
    ctypes.void_t
  );
  Assert.equal(test_void_t(), undefined);

  // Test that library.declare throws with void function args.
  do_check_throws(function() {
    library.declare(
      "test_void_t_cdecl",
      ctypes.default_abi,
      ctypes.void_t,
      ctypes.void_t
    );
  }, TypeError);

  if ("winLastError" in ctypes) {
    test_void_t = library.declare(
      "test_void_t_stdcall",
      ctypes.stdcall_abi,
      ctypes.void_t
    );
    Assert.equal(test_void_t(), undefined);

    // Check that WINAPI symbol lookup for a regular stdcall function fails on
    // Win32 (it's all the same on Win64 though).
    if (ctypes.voidptr_t.size == 4) {
      do_check_throws(function() {
        library.declare(
          "test_void_t_stdcall",
          ctypes.winapi_abi,
          ctypes.void_t
        );
      }, Error);
    }
  }
}

function run_string_tests(library) {
  let test_ansi_len = library.declare(
    "test_ansi_len",
    ctypes.default_abi,
    ctypes.int32_t,
    ctypes.char.ptr
  );
  Assert.equal(test_ansi_len(""), 0);
  Assert.equal(test_ansi_len("hello world"), 11);

  // don't convert anything else to a string
  let vals = [
    true,
    0,
    1 / 3,
    undefined,
    {},
    {
      toString() {
        return "bad";
      },
    },
    [],
  ];
  for (let i = 0; i < vals.length; i++) {
    do_check_throws(function() {
      test_ansi_len(vals[i]);
    }, TypeError);
  }

  let test_wide_len = library.declare(
    "test_wide_len",
    ctypes.default_abi,
    ctypes.int32_t,
    ctypes.char16_t.ptr
  );
  Assert.equal(test_wide_len("hello world"), 11);

  let test_ansi_ret = library.declare(
    "test_ansi_ret",
    ctypes.default_abi,
    ctypes.char.ptr
  );
  Assert.equal(test_ansi_ret().readString(), "success");

  let test_wide_ret = library.declare(
    "test_wide_ret",
    ctypes.default_abi,
    ctypes.char16_t.ptr
  );
  Assert.equal(test_wide_ret().readString(), "success");

  let test_ansi_echo = library.declare(
    "test_ansi_echo",
    ctypes.default_abi,
    ctypes.char.ptr,
    ctypes.char.ptr
  );
  // We cannot pass a string literal directly into test_ansi_echo, since the
  // conversion to ctypes.char.ptr is only valid for the duration of the ffi
  // call. The escaped pointer that's returned will point to freed memory.
  let arg = ctypes.char.array()("anybody in there?");
  Assert.equal(test_ansi_echo(arg).readString(), "anybody in there?");
  Assert.equal(ptrValue(test_ansi_echo(null)), 0);
}

function run_readstring_tests(library) {
  // ASCII decode test, "hello world"
  let ascii_string = ctypes.unsigned_char.array(12)();
  ascii_string[0] = 0x68;
  ascii_string[1] = 0x65;
  ascii_string[2] = 0x6c;
  ascii_string[3] = 0x6c;
  ascii_string[4] = 0x6f;
  ascii_string[5] = 0x20;
  ascii_string[6] = 0x77;
  ascii_string[7] = 0x6f;
  ascii_string[8] = 0x72;
  ascii_string[9] = 0x6c;
  ascii_string[10] = 0x64;
  ascii_string[11] = 0;
  Assert.equal("hello world", ascii_string.readStringReplaceMalformed());

  // UTF-8 decode test, "U+AC00 U+B098 U+B2E4"
  let utf8_string = ctypes.unsigned_char.array(10)();
  utf8_string[0] = 0xea;
  utf8_string[1] = 0xb0;
  utf8_string[2] = 0x80;
  utf8_string[3] = 0xeb;
  utf8_string[4] = 0x82;
  utf8_string[5] = 0x98;
  utf8_string[6] = 0xeb;
  utf8_string[7] = 0x8b;
  utf8_string[8] = 0xa4;
  utf8_string[9] = 0x00;
  let utf8_result = utf8_string.readStringReplaceMalformed();
  Assert.equal(0xac00, utf8_result.charCodeAt(0));
  Assert.equal(0xb098, utf8_result.charCodeAt(1));
  Assert.equal(0xb2e4, utf8_result.charCodeAt(2));

  // KS5601 decode test, invalid encoded byte should be replaced with U+FFFD
  let ks5601_string = ctypes.unsigned_char.array(7)();
  ks5601_string[0] = 0xb0;
  ks5601_string[1] = 0xa1;
  ks5601_string[2] = 0xb3;
  ks5601_string[3] = 0xaa;
  ks5601_string[4] = 0xb4;
  ks5601_string[5] = 0xd9;
  ks5601_string[6] = 0x00;
  let ks5601_result = ks5601_string.readStringReplaceMalformed();
  Assert.equal(0xfffd, ks5601_result.charCodeAt(0));
  Assert.equal(0xfffd, ks5601_result.charCodeAt(1));
  Assert.equal(0xfffd, ks5601_result.charCodeAt(2));
  Assert.equal(0xfffd, ks5601_result.charCodeAt(3));
  Assert.equal(0xfffd, ks5601_result.charCodeAt(4));
  Assert.equal(0xfffd, ks5601_result.charCodeAt(5));

  // Mixed decode test, "test" + "U+AC00 U+B098 U+B2E4" + "test"
  // invalid encoded byte should be replaced with U+FFFD
  let mixed_string = ctypes.unsigned_char.array(15)();
  mixed_string[0] = 0x74;
  mixed_string[1] = 0x65;
  mixed_string[2] = 0x73;
  mixed_string[3] = 0x74;
  mixed_string[4] = 0xb0;
  mixed_string[5] = 0xa1;
  mixed_string[6] = 0xb3;
  mixed_string[7] = 0xaa;
  mixed_string[8] = 0xb4;
  mixed_string[9] = 0xd9;
  mixed_string[10] = 0x74;
  mixed_string[11] = 0x65;
  mixed_string[12] = 0x73;
  mixed_string[13] = 0x74;
  mixed_string[14] = 0x00;
  let mixed_result = mixed_string.readStringReplaceMalformed();
  Assert.equal(0x74, mixed_result.charCodeAt(0));
  Assert.equal(0x65, mixed_result.charCodeAt(1));
  Assert.equal(0x73, mixed_result.charCodeAt(2));
  Assert.equal(0x74, mixed_result.charCodeAt(3));
  Assert.equal(0xfffd, mixed_result.charCodeAt(4));
  Assert.equal(0xfffd, mixed_result.charCodeAt(5));
  Assert.equal(0xfffd, mixed_result.charCodeAt(6));
  Assert.equal(0xfffd, mixed_result.charCodeAt(7));
  Assert.equal(0xfffd, mixed_result.charCodeAt(8));
  Assert.equal(0xfffd, mixed_result.charCodeAt(9));
  Assert.equal(0x74, mixed_result.charCodeAt(10));
  Assert.equal(0x65, mixed_result.charCodeAt(11));
  Assert.equal(0x73, mixed_result.charCodeAt(12));
  Assert.equal(0x74, mixed_result.charCodeAt(13));

  // Test of all posible invalid encoded sequence
  let invalid_string = ctypes.unsigned_char.array(27)();
  invalid_string[0] = 0x80; // 10000000
  invalid_string[1] = 0xd0; // 11000000 01110100
  invalid_string[2] = 0x74;
  invalid_string[3] = 0xe0; // 11100000 01110100
  invalid_string[4] = 0x74;
  invalid_string[5] = 0xe0; // 11100000 10100000 01110100
  invalid_string[6] = 0xa0;
  invalid_string[7] = 0x74;
  invalid_string[8] = 0xe0; // 11100000 10000000 01110100
  invalid_string[9] = 0x80;
  invalid_string[10] = 0x74;
  invalid_string[11] = 0xf0; // 11110000 01110100
  invalid_string[12] = 0x74;
  invalid_string[13] = 0xf0; // 11110000 10010000 01110100
  invalid_string[14] = 0x90;
  invalid_string[15] = 0x74;
  invalid_string[16] = 0xf0; // 11110000 10010000 10000000 01110100
  invalid_string[17] = 0x90;
  invalid_string[18] = 0x80;
  invalid_string[19] = 0x74;
  invalid_string[20] = 0xf0; // 11110000 10000000 10000000 01110100
  invalid_string[21] = 0x80;
  invalid_string[22] = 0x80;
  invalid_string[23] = 0x74;
  invalid_string[24] = 0xf0; // 11110000 01110100
  invalid_string[25] = 0x74;
  invalid_string[26] = 0x00;
  let invalid_result = invalid_string.readStringReplaceMalformed();
  Assert.equal(0xfffd, invalid_result.charCodeAt(0)); // 10000000
  Assert.equal(0xfffd, invalid_result.charCodeAt(1)); // 11000000 01110100
  Assert.equal(0x74, invalid_result.charCodeAt(2));
  Assert.equal(0xfffd, invalid_result.charCodeAt(3)); // 11100000 01110100
  Assert.equal(0x74, invalid_result.charCodeAt(4));
  Assert.equal(0xfffd, invalid_result.charCodeAt(5)); // 11100000 10100000 01110100
  Assert.equal(0x74, invalid_result.charCodeAt(6));
  Assert.equal(0xfffd, invalid_result.charCodeAt(7)); // 11100000 10000000 01110100
  Assert.equal(0xfffd, invalid_result.charCodeAt(8));
  Assert.equal(0x74, invalid_result.charCodeAt(9));
  Assert.equal(0xfffd, invalid_result.charCodeAt(10)); // 11110000 01110100
  Assert.equal(0x74, invalid_result.charCodeAt(11));
  Assert.equal(0xfffd, invalid_result.charCodeAt(12)); // 11110000 10010000 01110100
  Assert.equal(0x74, invalid_result.charCodeAt(13));
  Assert.equal(0xfffd, invalid_result.charCodeAt(14)); // 11110000 10010000 10000000 01110100
  Assert.equal(0x74, invalid_result.charCodeAt(15));
  Assert.equal(0xfffd, invalid_result.charCodeAt(16)); // 11110000 10000000 10000000 01110100
  Assert.equal(0xfffd, invalid_result.charCodeAt(17));
  Assert.equal(0xfffd, invalid_result.charCodeAt(18));
  Assert.equal(0x74, invalid_result.charCodeAt(19));
  Assert.equal(0xfffd, invalid_result.charCodeAt(20)); // 11110000 01110100
  Assert.equal(0x74, invalid_result.charCodeAt(21));

  // Test decoding of UTF-8 and CESU-8
  let utf8_cesu8_string = ctypes.unsigned_char.array(10)();
  utf8_cesu8_string[0] = 0xf0; // U+10400 in UTF-8
  utf8_cesu8_string[1] = 0x90;
  utf8_cesu8_string[2] = 0x90;
  utf8_cesu8_string[3] = 0x80;
  utf8_cesu8_string[4] = 0xed; // U+10400 in CESU-8
  utf8_cesu8_string[5] = 0xa0;
  utf8_cesu8_string[6] = 0x81;
  utf8_cesu8_string[7] = 0xed;
  utf8_cesu8_string[8] = 0xb0;
  utf8_cesu8_string[9] = 0x80;
  let utf8_cesu8_result = utf8_cesu8_string.readStringReplaceMalformed();
  Assert.equal(0xd801, utf8_cesu8_result.charCodeAt(0));
  Assert.equal(0xdc00, utf8_cesu8_result.charCodeAt(1));
  Assert.equal(0xfffd, utf8_cesu8_result.charCodeAt(2));
  Assert.equal(0xfffd, utf8_cesu8_result.charCodeAt(3));
}

function run_struct_tests(library) {
  const point_t = new ctypes.StructType("myPOINT", [
    { x: ctypes.int32_t },
    { y: ctypes.int32_t },
  ]);
  const rect_t = new ctypes.StructType("myRECT", [
    { top: ctypes.int32_t },
    { left: ctypes.int32_t },
    { bottom: ctypes.int32_t },
    { right: ctypes.int32_t },
  ]);

  let test_pt_in_rect = library.declare(
    "test_pt_in_rect",
    ctypes.default_abi,
    ctypes.int32_t,
    rect_t,
    point_t
  );
  let rect = new rect_t(10, 5, 5, 10);
  let pt1 = new point_t(6, 6);
  Assert.equal(test_pt_in_rect(rect, pt1), 1);
  let pt2 = new point_t(2, 2);
  Assert.equal(test_pt_in_rect(rect, pt2), 0);

  const inner_t = new ctypes.StructType("INNER", [
    { i1: ctypes.uint8_t },
    { i2: ctypes.int64_t },
    { i3: ctypes.uint8_t },
  ]);
  const nested_t = new ctypes.StructType("NESTED", [
    { n1: ctypes.int32_t },
    { n2: ctypes.int16_t },
    { inner: inner_t },
    { n3: ctypes.int64_t },
    { n4: ctypes.int32_t },
  ]);

  let test_nested_struct = library.declare(
    "test_nested_struct",
    ctypes.default_abi,
    ctypes.int32_t,
    nested_t
  );
  let inner = new inner_t(161, 523412, 43);
  let nested = new nested_t(13155, 1241, inner, 24512115, 1234111);
  // add up all the numbers and make sure the C function agrees
  Assert.equal(test_nested_struct(nested), 26284238);

  // test returning a struct by value
  let test_struct_return = library.declare(
    "test_struct_return",
    ctypes.default_abi,
    point_t,
    rect_t
  );
  let ret = test_struct_return(rect);
  Assert.equal(ret.x, rect.left);
  Assert.equal(ret.y, rect.top);

  // struct parameter ABI depends on size; test returning a large struct by value
  test_struct_return = library.declare(
    "test_large_struct_return",
    ctypes.default_abi,
    rect_t,
    rect_t,
    rect_t
  );
  ret = test_struct_return(rect_t(1, 2, 3, 4), rect_t(5, 6, 7, 8));
  Assert.equal(ret.left, 2);
  Assert.equal(ret.right, 4);
  Assert.equal(ret.top, 5);
  Assert.equal(ret.bottom, 7);

  // ... and tests structs < 8 bytes in size
  for (let i = 1; i < 8; ++i) {
    run_small_struct_test(library, rect_t, i);
  }

  // test passing a struct by pointer
  let test_init_pt = library.declare(
    "test_init_pt",
    ctypes.default_abi,
    ctypes.void_t,
    point_t.ptr,
    ctypes.int32_t,
    ctypes.int32_t
  );
  test_init_pt(pt1.address(), 9, 10);
  Assert.equal(pt1.x, 9);
  Assert.equal(pt1.y, 10);
}

function run_small_struct_test(library, rect_t, bytes) {
  let fields = [];
  for (let i = 0; i < bytes; ++i) {
    let field = {};
    field["f" + i] = ctypes.uint8_t;
    fields.push(field);
  }
  const small_t = new ctypes.StructType("SMALL", fields);

  let test_small_struct_return = library.declare(
    "test_" + bytes + "_byte_struct_return",
    ctypes.default_abi,
    small_t,
    rect_t
  );
  let ret = test_small_struct_return(rect_t(1, 7, 13, 45));

  let exp = [1, 7, 13, 45];
  let j = 0;
  for (let i = 0; i < bytes; ++i) {
    Assert.equal(ret["f" + i], exp[j]);
    if (++j == 4) {
      j = 0;
    }
  }
}

function run_function_tests(library) {
  let test_ansi_len = library.declare(
    "test_ansi_len",
    ctypes.default_abi,
    ctypes.int32_t,
    ctypes.char.ptr
  );
  let fn_t = ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [
    ctypes.char.ptr,
  ]).ptr;

  let test_fnptr = library.declare("test_fnptr", ctypes.default_abi, fn_t);

  // Test that the value handed back by test_fnptr matches the function pointer
  // for test_ansi_len itself.
  let ptr = test_fnptr();
  Assert.equal(ptrValue(test_ansi_len), ptrValue(ptr));

  // Test that we can call ptr().
  Assert.equal(ptr("function pointers rule!"), 23);

  // Test that we can call via call and apply
  /* eslint-disable no-useless-call */
  Assert.equal(ptr.call(null, "function pointers rule!"), 23);
  Assert.equal(ptr.apply(null, ["function pointers rule!"]), 23);

  // Test that we cannot call non-function pointers via call and apply
  let p_t = ctypes.PointerType(ctypes.int32_t);
  let p = p_t();
  do_check_throws(function() {
    p.call(null, "woo");
  }, TypeError);
  do_check_throws(function() {
    p.apply(null, ["woo"]);
  }, TypeError);
  /* eslint-enable no-useless-call */

  // Test the function pointers still behave as regular pointers
  Assert.ok(!ptr.isNull(), "PointerType methods should still be valid");

  // Test that library.declare() returns data of type FunctionType.ptr, and that
  // it is immutable.
  Assert.ok(
    test_ansi_len.constructor.targetType.__proto__ ===
      ctypes.FunctionType.prototype
  );
  Assert.equal(
    test_ansi_len.constructor.toSource(),
    "ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.char.ptr]).ptr"
  );
  /* disabled temporarily per bug 598225.
  do_check_throws(function() { test_ansi_len.value = null; }, Error);
  Assert.equal(ptrValue(test_ansi_len), ptrValue(ptr));
*/

  // Test that the library.declare(name, functionType) form works.
  let test_ansi_len_2 = library.declare("test_ansi_len", fn_t);
  Assert.ok(test_ansi_len_2.constructor === fn_t);
  Assert.equal(ptrValue(test_ansi_len), ptrValue(test_ansi_len_2));
  /* disabled temporarily per bug 598225.
  do_check_throws(function() { test_ansi_len_2.value = null; }, Error);
  Assert.equal(ptrValue(test_ansi_len_2), ptrValue(ptr));
*/
}

function run_closure_tests(library) {
  run_single_closure_tests(library, ctypes.default_abi, "cdecl");
  if ("winLastError" in ctypes) {
    run_single_closure_tests(library, ctypes.stdcall_abi, "stdcall");

    // Check that attempting to construct a ctypes.winapi_abi closure throws.
    function closure_fn() {
      return 1;
    }
    let fn_t = ctypes.FunctionType(ctypes.winapi_abi, ctypes.int32_t, []).ptr;
    do_check_throws(function() {
      fn_t(closure_fn);
    }, Error);
  }
}

function run_single_closure_tests(library, abi, suffix) {
  let b = 23;

  function closure_fn(i) {
    if (i == 42) {
      throw new Error("7.5 million years for that?");
    }
    return "a" in this ? i + this.a : i + b;
  }

  Assert.equal(closure_fn(7), 7 + b);
  let thisobj = { a: 5 };
  Assert.equal(closure_fn.call(thisobj, 7), 7 + thisobj.a);

  // Construct a closure, and call it ourselves.
  let fn_t = ctypes.FunctionType(abi, ctypes.int32_t, [ctypes.int8_t]).ptr;
  let closure = fn_t(closure_fn);
  Assert.equal(closure(-17), -17 + b);

  // Have C code call it.
  let test_closure = library.declare(
    "test_closure_" + suffix,
    ctypes.default_abi,
    ctypes.int32_t,
    ctypes.int8_t,
    fn_t
  );
  Assert.equal(test_closure(-52, closure), -52 + b);

  // Do the same, but specify 'this'.
  let closure2 = fn_t(closure_fn, thisobj);
  Assert.equal(closure2(-17), -17 + thisobj.a);
  Assert.equal(test_closure(-52, closure2), -52 + thisobj.a);

  // Specify an error sentinel, and have the JS code throw (see bug 599791).
  let closure3 = fn_t(closure_fn, null, 54);
  Assert.equal(closure3(42), 54);
  Assert.equal(test_closure(42, closure3), 54);

  // Check what happens when the return type is bigger than a word.
  var fn_64_t = ctypes.FunctionType(ctypes.default_abi, ctypes.uint64_t, [
    ctypes.bool,
  ]).ptr;
  var bignum1 = ctypes.UInt64.join(0xdeadbeef, 0xbadf00d);
  var bignum2 = ctypes.UInt64.join(0xdefec8ed, 0xd15ea5e);
  function closure_fn_64(fail) {
    if (fail) {
      throw new Error("Just following orders, sir!");
    }
    return bignum1;
  }
  var closure64 = fn_64_t(closure_fn_64, null, bignum2);
  Assert.equal(ctypes.UInt64.compare(closure64(false), bignum1), 0);
  Assert.equal(ctypes.UInt64.compare(closure64(true), bignum2), 0);

  // Test a callback that returns void (see bug 682504).
  var fn_v_t = ctypes.FunctionType(ctypes.default_abi, ctypes.void_t, []).ptr;
  fn_v_t(function() {})(); // Don't crash

  // Code evaluated in a sandbox uses (and pushes) a separate JSContext.
  // Make sure that we don't run into an assertion caused by a cx stack
  // mismatch with the cx stashed in the closure.
  try {
    var sb = Cu.Sandbox("http://www.example.com");
    sb.fn = fn_v_t(function() {
      sb.foo = {};
    });
    Cu.evalInSandbox("fn();", sb);
  } catch (e) {} // Components not available in workers.

  // Make sure that a void callback can't return an error sentinel.
  var sentinelThrew = false;
  try {
    fn_v_t(function() {}, null, -1);
  } catch (e) {
    sentinelThrew = true;
  }
  Assert.ok(sentinelThrew);
}

function run_variadic_tests(library) {
  let sum_va_type = ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [
      ctypes.uint8_t,
      "...",
    ]).ptr,
    sum_va = library.declare(
      "test_sum_va_cdecl",
      ctypes.default_abi,
      ctypes.int32_t,
      ctypes.uint8_t,
      "..."
    );

  Assert.equal(
    sum_va_type.toSource(),
    'ctypes.FunctionType(ctypes.default_abi, ctypes.int32_t, [ctypes.uint8_t, "..."]).ptr'
  );
  Assert.equal(sum_va.constructor.name, "int32_t(*)(uint8_t, ...)");
  Assert.ok(sum_va.constructor.targetType.isVariadic);

  Assert.equal(
    sum_va(3, ctypes.int32_t(1), ctypes.int32_t(2), ctypes.int32_t(3)),
    6
  );

  do_check_throws(function() {
    ctypes.FunctionType(ctypes.default_abi, ctypes.bool, [
      ctypes.bool,
      "...",
      ctypes.bool,
    ]);
  }, Error);

  do_check_throws(function() {
    ctypes.FunctionType(ctypes.default_abi, ctypes.bool, ["..."]);
  }, Error);

  if ("winLastError" in ctypes) {
    do_check_throws(function() {
      ctypes.FunctionType(ctypes.stdcall_abi, ctypes.bool, [
        ctypes.bool,
        "...",
      ]);
    }, Error);
    do_check_throws(function() {
      ctypes.FunctionType(ctypes.winapi_abi, ctypes.bool, [ctypes.bool, "..."]);
    }, Error);
  }

  do_check_throws(function() {
    // No variadic closure callbacks allowed.
    sum_va_type(function() {});
  }, Error);

  let count_true_va = library.declare(
    "test_sum_va_cdecl",
    ctypes.default_abi,
    ctypes.uint8_t,
    ctypes.uint8_t,
    "..."
  );
  Assert.equal(
    count_true_va(
      8,
      ctypes.bool(false),
      ctypes.bool(false),
      ctypes.bool(false),
      ctypes.bool(true),
      ctypes.bool(true),
      ctypes.bool(false),
      ctypes.bool(true),
      ctypes.bool(true)
    ),
    4
  );

  let add_char_short_int_va = library.declare(
      "test_add_char_short_int_va_cdecl",
      ctypes.default_abi,
      ctypes.void_t,
      ctypes.uint32_t.ptr,
      "..."
    ),
    result = ctypes.uint32_t(3);

  add_char_short_int_va(
    result.address(),
    ctypes.char(5),
    ctypes.short(7),
    ctypes.uint32_t(11)
  );

  Assert.equal(result.value, 3 + 5 + 7 + 11);

  result = ctypes.int32_t.array(3)([1, 1, 1]);
  let v1 = ctypes.int32_t.array(4)([1, 2, 3, 5]);
  let v2 = ctypes.int32_t.array(3)([7, 11, 13]);
  let vector_add_va = library.declare(
    "test_vector_add_va_cdecl",
    ctypes.default_abi,
    ctypes.int32_t.ptr,
    ctypes.uint8_t,
    ctypes.uint8_t,
    ctypes.int32_t.ptr,
    "..."
  );
  // Note that vector_add_va zeroes out result first.
  let vec_sum = vector_add_va(2, 3, result, v1, v2);
  Assert.equal(vec_sum.contents, 8);
  Assert.equal(result[0], 8);
  Assert.equal(result[1], 13);
  Assert.equal(result[2], 16);

  Assert.ok(!!(sum_va_type().value = sum_va_type()));
  let sum_notva_type = ctypes.FunctionType(
    sum_va_type.targetType.abi,
    sum_va_type.targetType.returnType,
    [ctypes.uint8_t]
  ).ptr;
  do_check_throws(function() {
    sum_va_type().value = sum_notva_type();
  }, TypeError);
}

function run_static_data_tests(library) {
  const rect_t = new ctypes.StructType("myRECT", [
    { top: ctypes.int32_t },
    { left: ctypes.int32_t },
    { bottom: ctypes.int32_t },
    { right: ctypes.int32_t },
  ]);

  let data_rect = library.declare("data_rect", rect_t);

  // Test reading static data.
  Assert.ok(data_rect.constructor === rect_t);
  Assert.equal(data_rect.top, -1);
  Assert.equal(data_rect.left, -2);
  Assert.equal(data_rect.bottom, 3);
  Assert.equal(data_rect.right, 4);

  // Test writing.
  data_rect.top = 9;
  data_rect.left = 8;
  data_rect.bottom = -11;
  data_rect.right = -12;
  Assert.equal(data_rect.top, 9);
  Assert.equal(data_rect.left, 8);
  Assert.equal(data_rect.bottom, -11);
  Assert.equal(data_rect.right, -12);

  // Make sure it's been written, not copied.
  let data_rect_2 = library.declare("data_rect", rect_t);
  Assert.equal(data_rect_2.top, 9);
  Assert.equal(data_rect_2.left, 8);
  Assert.equal(data_rect_2.bottom, -11);
  Assert.equal(data_rect_2.right, -12);
  Assert.equal(ptrValue(data_rect.address()), ptrValue(data_rect_2.address()));
}

function run_cpp_class_tests(library) {
  // try the gcc mangling, unless we're using MSVC.
  let OS = get_os();
  let ctor_symbol;
  let add_symbol;
  let abi;
  if (OS == "WINNT") {
    // for compatibility for Win32 vs Win64
    abi = ctypes.thiscall_abi;
    if (ctypes.size_t.size == 8) {
      ctor_symbol = "??0TestClass@@QEAA@H@Z";
      add_symbol = "?Add@TestClass@@QEAAHH@Z";
    } else {
      ctor_symbol = "??0TestClass@@QAE@H@Z";
      add_symbol = "?Add@TestClass@@QAEHH@Z";
    }
  } else {
    abi = ctypes.default_abi;
    ctor_symbol = "_ZN9TestClassC1Ei";
    add_symbol = "_ZN9TestClass3AddEi";
  }

  let test_class_ctor = library.declare(
    ctor_symbol,
    abi,
    ctypes.void_t,
    ctypes.int32_t.ptr,
    ctypes.int32_t
  );
  let i = ctypes.int32_t();
  test_class_ctor(i.address(), 8);
  Assert.equal(i.value, 8);

  let test_class_add = library.declare(
    add_symbol,
    abi,
    ctypes.int32_t,
    ctypes.int32_t.ptr,
    ctypes.int32_t
  );
  let j = test_class_add(i.address(), 5);
  Assert.equal(j, 13);
  Assert.equal(i.value, 13);
}

// bug 522360 - try loading system library without full path
function run_load_system_library() {
  let syslib;
  let OS = get_os();
  if (OS == "WINNT") {
    syslib = ctypes.open("user32.dll");
  } else if (OS == "Darwin") {
    syslib = ctypes.open("libm.dylib");
  } else if (OS == "Linux" || OS == "Android" || OS.match(/BSD$/)) {
    try {
      syslib = ctypes.open("libm.so");
    } catch (e) {
      // limb.so wasn't available, try libm.so.6 instead
      syslib = ctypes.open("libm.so.6");
    }
  } else {
    do_throw("please add a system library for this test");
  }
  syslib.close();
  return true;
}
