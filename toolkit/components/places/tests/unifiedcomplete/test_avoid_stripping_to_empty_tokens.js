/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_protocol_trimming() {
  for (let prot of ["http", "https", "ftp"]) {
    let visit = {
      // Include the protocol in the query string to ensure we get matches (see bug 1059395)
      uri: NetUtil.newURI(prot + "://www.mozilla.org/test/?q=" + prot + encodeURIComponent("://") + "www.foo"),
      title: "Test title",
      transition: TRANSITION_TYPED
    };
    yield PlacesTestUtils.addVisits(visit);
    let matches = [{uri: visit.uri, title: visit.title}];

    let inputs = [
      prot + "://",
      prot + ":// ",
      prot + ":// mo",
      prot + "://mo te",
      prot + "://www.",
      prot + "://www. ",
      prot + "://www. mo",
      prot + "://www.mo te",
      "www.",
      "www. ",
      "www. mo",
      "www.mo te"
    ];
    for (let input of inputs) {
      do_print("Searching for: " + input);
      yield check_autocomplete({
        search: input,
        matches
      });
    }

    yield cleanup();
  }
});

