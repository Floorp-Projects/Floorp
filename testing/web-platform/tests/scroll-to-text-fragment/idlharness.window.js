// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js

'use strict';

idl_test(
  ['scroll-to-text-fragment'],
  ['html'],
  idl_array => {
    idl_array.add_objects({
      Location: ['document.location'],
      FragmentDirective: ['document.location.fragmentDirective'],
    });
  }
);
