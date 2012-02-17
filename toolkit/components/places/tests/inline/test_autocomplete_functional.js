/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Functional tests for inline autocomplete

add_autocomplete_test([
  "Add urls, check for correct order",
  "vis",
  "visit2.mozilla.org/",
  function ()
  {
    let urls = [{ url: NetUtil.newURI("http://visit1.mozilla.org"),
                  transition: undefined,
                },
                { url: NetUtil.newURI("http://visit2.mozilla.org"),
                  transition: TRANSITION_TYPED,
                }];
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "Add urls, make sure www and http are ignored",
  "visit1",
  "visit1.mozilla.org/",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://www.visit1.mozilla.org"),
                  transition: undefined,
                },
               ];
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "Autocompleting after an existing host completes to the url",
  "visit3.mozilla.org/",
  "visit3.mozilla.org/",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://www.visit3.mozilla.org"),
                  transition: undefined,
                },
               ];
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "Searching for www.me should yield www.me.mozilla.org/",
  "www.me",
  "www.me.mozilla.org/",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://www.me.mozilla.org"),
                  transition: undefined,
                },
               ];
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "With a bookmark and history, the query result should be the bookmark",
  "bookmark",
  "bookmark1.mozilla.org/",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://bookmark1.mozilla.org/foo"),
                  transition: undefined,
                },
               ];
    addBookmark({ url: "http://bookmark1.mozilla.org/", });
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "Check to make sure we get the proper results with full paths",
  "smokey",
  "smokey.mozilla.org/",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=delicious"),
                  transition: undefined,
                },
                { url: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=smokey"),
                  transition: undefined,
                },
               ];
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "Check to make sure we autocomplete to the following '/'",
  "smokey.mozilla.org/fo",
  "smokey.mozilla.org/foo/",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=delicious"),
                  transition: undefined,
                },
                { url: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=smokey"),
                  transition: undefined,
                },
               ];
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "Check to make sure we autocomplete after ?",
  "smokey.mozilla.org/foo?",
  "smokey.mozilla.org/foo?bacon=delicious",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious"),
                  transition: undefined,
                },
               ];
    addVisits(urls);
  }
]);

add_autocomplete_test([
  "Check to make sure we autocomplete after #",
  "smokey.mozilla.org/foo?bacon=delicious#bar",
  "smokey.mozilla.org/foo?bacon=delicious#bar",
  function ()
  {

    let urls = [{ url: NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious#bar"),
                  transition: undefined,
                },
               ];
    addVisits(urls);
  }
]);
