// META: global=window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

// https://w3c.github.io/performance-timeline/

'use strict';

idl_test(
  ['performance-timeline'],
  ['hr-time', 'dom'],
  idl_array => {
    try {
      const callback = (e, o) => {};
      self.observerEntry = new ResizeObserverEntry(callback);
    } catch (e) {
      // Will be surfaced when entry is undefined below.
    }

    idl_array.add_objects({
      Performance: ['performance'],
      // PerformanceEntry implicitly tested in user-timing.
      ResizeObserverEntry: ['observerEntry'],
    });
  },
  'Test IDL implementation of performance-timeline API'
);
