/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is just a sanity test that Gecko is giving SpiderMonkey a MallocSizeOf
// function for new JSRuntimes. There is more extensive testing around the
// expected byte sizes within SpiderMonkey's jit-tests, we just want to make
// sure that Gecko is providing SpiderMonkey with the callback it needs.

var Cu = Components.utils;
const { byteSize } = Cu.getJSTestingFunctions();

function run_test()
{
  const objects = [
    {},
    { w: 1, x: 2, y: 3, z:4, a: 5 },
    [],
    Array(10).fill(null),
    new RegExp("(2|two) problems", "g"),
    new Date(),
    new Uint8Array(64),
    Promise.resolve(1),
    function f() {},
    Object
  ];

  for (let obj of objects) {
    do_print(uneval(obj));
    ok(byteSize(obj), "We should get some (non-zero) byte size");
  }
}
