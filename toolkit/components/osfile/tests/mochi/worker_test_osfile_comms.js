/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env mozilla/chrome-worker, node */
/* global finish, log */

"use strict";

importScripts("chrome://mochikit/content/tests/SimpleTest/WorkerSimpleTest.js");

// The set of samples for communications test. Declare as a global
// variable to prevent this from being garbage-collected too early.
var samples;

self.onmessage = function(msg) {
  info("Initializing");
  self.onmessage = function on_unexpected_message(msg) {
    throw new Error("Unexpected message " + JSON.stringify(msg.data));
  };
  importScripts("resource://gre/modules/osfile.jsm");
  info("Initialization complete");

  samples = [
    {
      typename: "OS.Shared.Type.char.in_ptr",
      valuedescr: "String",
      value: "This is a test",
      type: OS.Shared.Type.char.in_ptr,
      check: function check_string(candidate, prefix) {
        is(candidate, "This is a test", prefix);
      },
    },
    {
      typename: "OS.Shared.Type.char.in_ptr",
      valuedescr: "Typed array",
      value: (function() {
        let view = new Uint8Array(15);
        for (let i = 0; i < 15; ++i) {
          view[i] = i;
        }
        return view;
      })(),
      type: OS.Shared.Type.char.in_ptr,
      check: function check_ArrayBuffer(candidate, prefix) {
        for (let i = 0; i < 15; ++i) {
          is(
            candidate[i],
            i % 256,
            prefix +
              "Checking that the contents of the ArrayBuffer were preserved"
          );
        }
      },
    },
    {
      typename: "OS.Shared.Type.char.in_ptr",
      valuedescr: "Pointer",
      value: new OS.Shared.Type.char.in_ptr.implementation(1),
      type: OS.Shared.Type.char.in_ptr,
      check: function check_ptr(candidate, prefix) {
        let address = ctypes.cast(candidate, ctypes.uintptr_t).value.toString();
        is(
          address,
          "1",
          prefix + "Checking that the pointer address was preserved"
        );
      },
    },
    {
      typename: "OS.Shared.Type.char.in_ptr",
      valuedescr: "C array",
      value: (function() {
        let buf = new (ctypes.ArrayType(ctypes.uint8_t, 15))();
        for (let i = 0; i < 15; ++i) {
          buf[i] = i % 256;
        }
        return buf;
      })(),
      type: OS.Shared.Type.char.in_ptr,
      check: function check_array(candidate, prefix) {
        let cast = ctypes.cast(candidate, ctypes.uint8_t.ptr);
        for (let i = 0; i < 15; ++i) {
          is(
            cast.contents,
            i % 256,
            prefix +
              "Checking that the contents of the C array were preserved, index " +
              i
          );
          cast = cast.increment();
        }
      },
    },
    {
      typename: "OS.File.Error",
      valuedescr: "OS Error",
      type: OS.File.Error,
      value: new OS.File.Error("foo", 1),
      check: function check_error(candidate, prefix) {
        ok(
          candidate instanceof OS.File.Error,
          prefix + "Error is an OS.File.Error"
        );
        ok(
          candidate.unixErrno == 1 || candidate.winLastError == 1,
          prefix + "Error code is correct"
        );
        try {
          let string = candidate.toString();
          info(prefix + ".toString() works " + string);
        } catch (x) {
          ok(false, prefix + ".toString() fails " + x);
        }
      },
    },
  ];
  samples.forEach(function test(sample) {
    let type = sample.type;
    let value = sample.value;
    let check = sample.check;
    info(
      "Testing handling of type " +
        sample.typename +
        " communicating " +
        sample.valuedescr
    );

    // 1. Test serialization
    let serialized;
    let exn;
    try {
      serialized = type.toMsg(value);
    } catch (ex) {
      exn = ex;
    }
    is(
      exn,
      null,
      "Can I serialize the following value? " +
        value +
        " aka " +
        JSON.stringify(value)
    );
    if (exn) {
      return;
    }

    if ("data" in serialized) {
      // Unwrap from `Meta`
      serialized = serialized.data;
    }

    // 2. Test deserialization
    let deserialized;
    try {
      deserialized = type.fromMsg(serialized);
    } catch (ex) {
      exn = ex;
    }
    is(
      exn,
      null,
      "Can I deserialize the following message? " +
        serialized +
        " aka " +
        JSON.stringify(serialized)
    );
    if (exn) {
      return;
    }

    // 3. Local test deserialized value
    info(
      "Running test on deserialized value " +
        deserialized +
        " aka " +
        JSON.stringify(deserialized)
    );
    check(deserialized, "Local test: ");

    // 4. Test sending serialized
    info("Attempting to send message");
    try {
      self.postMessage({
        kind: "value",
        typename: sample.typename,
        value: serialized,
        check: check.toSource(),
      });
    } catch (ex) {
      exn = ex;
    }
    is(
      exn,
      null,
      "Can I send the following message? " +
        serialized +
        " aka " +
        JSON.stringify(serialized)
    );
  });

  finish();
};
