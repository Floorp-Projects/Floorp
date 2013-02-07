/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Functional tests for inline autocomplete

add_autocomplete_test([
  "Check disabling autocomplete disables autofill",
  "vis",
  "vis",
  function ()
  {
    Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", false);
    promiseAddVisits({ uri: NetUtil.newURI("http://visit.mozilla.org"),
                       transition: TRANSITION_TYPED });
  }
]);

add_autocomplete_test([
  "Check disabling autofill disables autofill",
  "vis",
  "vis",
  function ()
  {
    Services.prefs.setBoolPref("browser.urlbar.autoFill", false);
    promiseAddVisits({ uri: NetUtil.newURI("http://visit.mozilla.org"),
                       transition: TRANSITION_TYPED });
  }
]);

add_autocomplete_test([
  "Add urls, check for correct order",
  "vis",
  "visit2.mozilla.org/",
  function ()
  {
    let places = [{ uri: NetUtil.newURI("http://visit1.mozilla.org") },
                  { uri: NetUtil.newURI("http://visit2.mozilla.org"),
                    transition: TRANSITION_TYPED }];
    promiseAddVisits(places);
  }
]);

add_autocomplete_test([
  "Add urls, make sure www and http are ignored",
  "visit1",
  "visit1.mozilla.org/",
  function ()
  {
    promiseAddVisits(NetUtil.newURI("http://www.visit1.mozilla.org"));
  }
]);

add_autocomplete_test([
  "Autocompleting after an existing host completes to the url",
  "visit3.mozilla.org/",
  "visit3.mozilla.org/",
  function ()
  {
    promiseAddVisits(NetUtil.newURI("http://www.visit3.mozilla.org"));
  }
]);

add_autocomplete_test([
  "Searching for www.me should yield www.me.mozilla.org/",
  "www.me",
  "www.me.mozilla.org/",
  function ()
  {
    promiseAddVisits(NetUtil.newURI("http://www.me.mozilla.org"));
  }
]);

add_autocomplete_test([
  "With a bookmark and history, the query result should be the bookmark",
  "bookmark",
  "bookmark1.mozilla.org/",
  function ()
  {
    addBookmark({ url: "http://bookmark1.mozilla.org/", });
    promiseAddVisits(NetUtil.newURI("http://bookmark1.mozilla.org/foo"));
  }
]);

add_autocomplete_test([
  "Check to make sure we get the proper results with full paths",
  "smokey",
  "smokey.mozilla.org/",
  function ()
  {

    let places = [{ uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=delicious") },
                  { uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=smokey") }];
    promiseAddVisits(places);
  }
]);

add_autocomplete_test([
  "Check to make sure we autocomplete to the following '/'",
  "smokey.mozilla.org/fo",
  "smokey.mozilla.org/foo/",
  function ()
  {

    let places = [{ uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=delicious") },
                  { uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=smokey") }];
    promiseAddVisits(places);
  }
]);

add_autocomplete_test([
  "Check to make sure we autocomplete after ?",
  "smokey.mozilla.org/foo?",
  "smokey.mozilla.org/foo?bacon=delicious",
  function ()
  {
    promiseAddVisits(NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious"));
  }
]);

add_autocomplete_test([
  "Check to make sure we autocomplete after #",
  "smokey.mozilla.org/foo?bacon=delicious#bar",
  "smokey.mozilla.org/foo?bacon=delicious#bar",
  function ()
  {
    promiseAddVisits(NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious#bar"));
  }
]);
