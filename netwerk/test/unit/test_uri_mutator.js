/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
Cu.import("resource://gre/modules/NetUtil.jsm");

function standardMutator()
{
  return Cc['@mozilla.org/network/standard-url-mutator;1']
           .createInstance(Ci.nsIURIMutator);
}

add_task(async function test_simple_setter_chaining() {
  let uri = standardMutator()
              .setSpec("http://example.com/")
              .setQuery("hello")
              .setRef("bla")
              .setScheme("ftp")
              .finalize();
  equal(uri.spec, "ftp://example.com/?hello#bla");
});
