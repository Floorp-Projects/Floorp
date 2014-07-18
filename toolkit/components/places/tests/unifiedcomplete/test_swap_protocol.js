/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 424717 to make sure searching with an existing location like
 * http://site/ also matches https://site/ or ftp://site/. Same thing for
 * ftp://site/ and https://site/.
 *
 * Test bug 461483 to make sure a search for "w" doesn't match the "www." from
 * site subdomains.
 */

add_task(function* test_swap_protocol() {
  let uri1 = NetUtil.newURI("http://www.site/");
  let uri2 = NetUtil.newURI("http://site/");
  let uri3 = NetUtil.newURI("ftp://ftp.site/");
  let uri4 = NetUtil.newURI("ftp://site/");
  let uri5 = NetUtil.newURI("https://www.site/");
  let uri6 = NetUtil.newURI("https://site/");
  let uri7 = NetUtil.newURI("http://woohoo/");
  let uri8 = NetUtil.newURI("http://wwwwwwacko/");
  yield promiseAddVisits([ { uri: uri1, title: "title" },
                           { uri: uri2, title: "title" },
                           { uri: uri3, title: "title" },
                           { uri: uri4, title: "title" },
                           { uri: uri5, title: "title" },
                           { uri: uri6, title: "title" },
                           { uri: uri7, title: "title" },
                           { uri: uri8, title: "title" } ]);

  let allMatches = [
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
    { uri: uri3, title: "title" },
    { uri: uri4, title: "title" },
    { uri: uri5, title: "title" },
    { uri: uri6, title: "title" }
  ];

  Services.prefs.setBoolPref("browser.urlbar.autoFill", "false");

  do_log_info("http://www.site matches all site");
  yield check_autocomplete({
    search: "http://www.site",
    matches: allMatches
  });
/*
  do_log_info("http://site matches all site");
  yield check_autocomplete({
    search: "http://site",
    matches: allMatches
  });

  do_log_info("ftp://ftp.site matches itself");
  yield check_autocomplete({
    search: "ftp://ftp.site",
    matches: { uri: uri3, title: "title"}
  });

  do_log_info("ftp://site matches all site");
  yield check_autocomplete({
    search: "ftp://site",
    matches: allMatches
  });

  do_log_info("https://www.site matches all site");
  yield check_autocomplete({
    search: "https://www.site",
    matches: allMatches
  });

  do_log_info("https://site matches all site");
  yield check_autocomplete({
    search: "https://site",
    matches: allMatches
  });

  do_log_info("www.site matches all site");
  yield check_autocomplete({
    search: "www.site",
    matches: allMatches
  });

  do_log_info("w matches none of www.");
  yield check_autocomplete({
    search: "w",
    matches: [ { uri: uri7, title: "title" },
               { uri: uri8, title: "title" } ]
  });

  do_log_info("http://w matches none of www.");
  yield check_autocomplete({
    search: "http://w",
    matches: [ { uri: uri7, title: "title" },
               { uri: uri8, title: "title" } ]
  });

  do_log_info("http://w matches none of www.");
  yield check_autocomplete({
    search: "http://www.w",
    matches: [ { uri: uri7, title: "title" },
               { uri: uri8, title: "title" } ]
  });

  do_log_info("ww matches none of www.");
  yield check_autocomplete({
    search: "ww",
    matches: [ { uri: uri8, title: "title" } ]
  });

  do_log_info("ww matches none of www.");
  yield check_autocomplete({
    search: "ww",
    matches: [ { uri: uri8, title: "title" } ]
  });

  do_log_info("http://ww matches none of www.");
  yield check_autocomplete({
    search: "http://ww",
    matches: [ { uri: uri8, title: "title" } ]
  });

  do_log_info("http://www.ww matches none of www.");
  yield check_autocomplete({
    search: "http://www.ww",
    matches: [ { uri: uri8, title: "title" } ]
  });

  do_log_info("www matches none of www.");
  yield check_autocomplete({
    search: "www",
    matches: [ { uri: uri8, title: "title" } ]
  });

  do_log_info("http://www matches none of www.");
  yield check_autocomplete({
    search: "http://www",
    matches: [ { uri: uri8, title: "title" } ]
  });

  do_log_info("http://www.www matches none of www.");
  yield check_autocomplete({
    search: "http://www.www",
    matches: [ { uri: uri8, title: "title" } ]
  });
*/
  yield cleanup();
});
