/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_protocol_trimming() {
  for (let prot of ["http", "https", "ftp"]) {
    let visit = {
      // Include the protocol in the query string to ensure we get matches (see bug 1059395)
      uri: NetUtil.newURI(prot + "://www.mozilla.org/test/?q=" + prot + encodeURIComponent("://") + "www.foo"),
      title: "Test title",
      transition: TRANSITION_TYPED
    };
    await PlacesTestUtils.addVisits(visit);

    let input = prot + "://www.";
    info("Searching for: " + input);
    await check_autocomplete({
      search: input,
      matches: [
        {
          value: prot + "://www.mozilla.org/",
          comment: prot == "http" ? "www.mozilla.org"
                                  : prot + "://www.mozilla.org",
          style: ["autofill", "heuristic"],
        },
        {
          value: visit.uri.spec,
          comment: visit.title,
          style: ["favicon"],
        }
      ],
    });

    input = "www.";
    info("Searching for: " + input);
    await check_autocomplete({
      search: input,
      matches: [
        {
          value: "www.mozilla.org/",
          comment: prot == "http" ? "www.mozilla.org"
                                  : prot + "://www.mozilla.org",
          style: ["autofill", "heuristic"],
        },
        {
          value: visit.uri.spec,
          comment: visit.title,
          style: ["favicon"],
        }
      ],
    });

    let inputs = [
      prot + "://",
      prot + ":// ",
      prot + ":// mo",
      prot + "://mo te",
      prot + "://www. ",
      prot + "://www. mo",
      prot + "://www.mo te",
      "www. ",
      "www. mo",
      "www.mo te"
    ];
    for (input of inputs) {
      info("Searching for: " + input);
      await check_autocomplete({
        search: input,
        matches: [{uri: visit.uri, title: visit.title}],
      });
    }

    await cleanup();
  }
});

