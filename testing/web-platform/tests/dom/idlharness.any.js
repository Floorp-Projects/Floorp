// META: global=!window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

'use strict';

idl_test(
  ['dom'],
  ['html'],
  idl_array => {
    idl_array.add_objects({
      EventTarget: ['new EventTarget()'],
      Event: ['new Event("foo")'],
      CustomEvent: ['new CustomEvent("foo")'],
      AbortController: ['new AbortController()'],
      AbortSignal: ['new AbortController().signal'],
    });
  }
);

done();
