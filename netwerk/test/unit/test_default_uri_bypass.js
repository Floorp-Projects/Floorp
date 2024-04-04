/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Test that default uri is bypassable by an unknown protocol that is
 *  present in the bypass list (and the pref is enabled)
 */
"use strict";

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  // In-Parent-only process pref setup
  if (!inChildProcess()) {
    Services.prefs.setBoolPref("network.url.useDefaultURI", true);
    Services.prefs.setBoolPref(
      "network.url.some_schemes_bypass_defaultURI_fallback",
      true
    );

    Services.prefs.setCharPref(
      "network.url.simple_uri_schemes",
      "simpleprotocol,otherproto"
    );
  }

  // check valid url is fine
  let uri = NetUtil.newURI("https://example.com/");
  Assert.equal(uri.spec, "https://example.com/"); // same

  // nsStandardURL removes second colon when nesting protocols
  let uri1 = NetUtil.newURI("https://https://example.com/");
  Assert.equal(uri1.spec, "https://https//example.com/");

  // defaultUri removes second colon
  // no-bypass protocol uses defaultURI
  let uri2 = NetUtil.newURI("nonsimpleprotocol://https://example.com");
  Assert.equal(uri2.spec, "nonsimpleprotocol://https//example.com");

  // simpleURI keeps the second colon
  // an unknown protocol in the bypass list will use simpleURI
  // despite network.url.useDefaultURI being enabled
  let same = "simpleprotocol://https://example.com";
  let uri3 = NetUtil.newURI(same);
  Assert.equal(uri3.spec, same); // simple uri keeps second colon

  // setCharPref not accessible from child process
  if (!inChildProcess()) {
    // check that pref update removes simpleprotocol from bypass list
    Services.prefs.setCharPref("network.url.simple_uri_schemes", "otherproto");
    let uri4 = NetUtil.newURI("simpleprotocol://https://example.com");
    Assert.equal(uri4.spec, "simpleprotocol://https//example.com"); // default uri removes second colon

    // check that spaces are parsed out
    Services.prefs.setCharPref(
      "network.url.simple_uri_schemes",
      " simpleprotocol , otherproto "
    );
    let uri5 = NetUtil.newURI("simpleprotocol://https://example.com");
    Assert.equal(uri5.spec, "simpleprotocol://https://example.com"); // simple uri keeps second colon
    let uri6 = NetUtil.newURI("otherproto://https://example.com");
    Assert.equal(uri6.spec, "otherproto://https://example.com"); // simple uri keeps second colon
  }
}
