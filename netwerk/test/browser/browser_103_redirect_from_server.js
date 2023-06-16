/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

registerCleanupFunction(function () {
  Services.prefs.clearUserPref("network.early-hints.enabled");
});

// This function tests Early Hint responses before and in between HTTP redirects.
//
// Arguments:
// - name: String identifying the test case for easier parsing in the log
// - chain and destination: defines the redirect chain, see example below
//                          note: ALL preloaded urls must be image urls
// - expected: number of normal, cancelled and completed hinted responses.
//
// # Example
// The parameter values of
// ```
// chain = [
//  {link:"https://link1", host:"https://host1.com"},
//  {link:"https://link2", host:"https://host2.com"},
// ]
// ```
// and `destination = "https://host3.com/page.html" would result in the
// following HTTP exchange (simplified):
//
// ```
// > GET https://host1.com/redirect?something1
//
// < 103 Early Hints
// < Link: <https://link1>;rel=preload;as=image
// <
// < 307 Temporary Redirect
// < Location: https://host2.com/redirect?something2
// <
//
// > GET https://host2.com/redirect?something2
//
// < 103 Early Hints
// < Link: <https://link2>;rel=preload;as=image
// <
// < 307 Temporary Redirect
// < Location: https://host3.com/page.html
// <
//
// > GET https://host3.com/page.html
//
// < [...] Result depends on the final page
// ```
//
// Legend:
// * `>` indicates a request going from client to server
// * `<` indicates a response going from server to client
// * all lines are terminated with a `\r\n`
//
async function test_hint_redirect(
  name,
  chain,
  destination,
  hint_destination,
  expected
) {
  // pass the full redirect chain as a url parameter. Each redirect is handled
  // by `early_hint_redirect_html.sjs` which url-decodes the query string and
  // redirects to the result
  let links = [];
  let url = destination;
  for (let i = chain.length - 1; i >= 0; i--) {
    let qp = new URLSearchParams();
    if (chain[i].link != "") {
      qp.append("link", "<" + chain[i].link + ">;rel=preload;as=image");
      links.push(chain[i].link);
    }
    qp.append("location", url);

    url = `${
      chain[i].host
    }/browser/netwerk/test/browser/early_hint_redirect_html.sjs?${qp.toString()}`;
  }
  if (hint_destination != "") {
    links.push(hint_destination);
  }

  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  // main request and all other must get their respective OnStopRequest
  let numRequestRemaining =
    expected.normal + expected.hinted + expected.cancelled;
  let observed = {
    hinted: 0,
    normal: 0,
    cancelled: 0,
  };
  // store channelIds
  let observedChannelIds = [];
  let callback;
  let promise = new Promise(resolve => {
    callback = resolve;
  });
  if (numRequestRemaining > 0) {
    let observer = {
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
      observe(aSubject, aTopic, aData) {
        aSubject.QueryInterface(Ci.nsIIdentChannel);
        let id = aSubject.channelId;
        if (observedChannelIds.includes(id)) {
          return;
        }
        aSubject.QueryInterface(Ci.nsIRequest);
        dump("Observer aSubject.name " + aSubject.name + "\n");
        if (aTopic == "http-on-stop-request" && links.includes(aSubject.name)) {
          if (aSubject.status == Cr.NS_ERROR_ABORT) {
            observed.cancelled += 1;
          } else {
            aSubject.QueryInterface(Ci.nsIHttpChannel);
            let initiator = "";
            try {
              initiator = aSubject.getRequestHeader("X-Moz");
            } catch {}
            if (initiator == "early hint") {
              observed.hinted += 1;
            } else {
              observed.normal += 1;
            }
          }
          observedChannelIds.push(id);
          numRequestRemaining -= 1;
          dump("Observer numRequestRemaining " + numRequestRemaining + "\n");
        }
        if (numRequestRemaining == 0) {
          Services.obs.removeObserver(observer, "http-on-stop-request");
          callback();
        }
      },
    };
    Services.obs.addObserver(observer, "http-on-stop-request");
  } else {
    callback();
  }

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
      waitForLoad: true,
    },
    async function () {}
  );

  // wait until all requests are stopped, especially the cancelled ones
  await promise;

  let got = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());

  // stringify to pretty print assert output
  let g = JSON.stringify(observed);
  let e = JSON.stringify(expected);
  Assert.equal(
    expected.normal,
    observed.normal,
    `${name} normal observed from client expected ${expected.normal} (${e}) got ${observed.normal} (${g})`
  );
  Assert.equal(
    expected.hinted,
    observed.hinted,
    `${name} hinted observed from client expected ${expected.hinted} (${e})  got ${observed.hinted} (${g})`
  );
  Assert.equal(
    expected.cancelled,
    observed.cancelled,
    `${name} cancelled observed from client expected ${expected.cancelled} (${e})  got ${observed.cancelled} (${g})`
  );

  // each cancelled request might be cancelled after the request was already
  // made. Allow cancelled responses to count towards the hinted to avoid
  // intermittent test failures.
  Assert.ok(
    expected.hinted <= got.hinted &&
      got.hinted <= expected.hinted + expected.cancelled,
    `${name}: unexpected amount of hinted request made got ${
      got.hinted
    }, expected between ${expected.hinted} and ${
      expected.hinted + expected.cancelled
    }`
  );
  Assert.ok(
    got.normal == expected.normal,
    `${name}: unexpected amount of normal request made expected ${expected.normal}, got ${got.normal}`
  );
  Assert.equal(numRequestRemaining, 0, "Requests remaining");
}

add_task(async function double_redirect_cross_origin() {
  await test_hint_redirect(
    "double_redirect_cross_origin_both_hints",
    [
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.com/",
      },
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.net",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=1",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 1, normal: 0, cancelled: 2 }
  );
  await test_hint_redirect(
    "double_redirect_second_hint",
    [
      {
        link: "",
        host: "https://example.com/",
      },
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.net",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=1",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 1, normal: 0, cancelled: 1 }
  );
  await test_hint_redirect(
    "double_redirect_first_hint",
    [
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.com/",
      },
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.net",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=0",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 0, normal: 1, cancelled: 2 }
  );
});

add_task(async function redirect_cross_origin() {
  await test_hint_redirect(
    "redirect_cross_origin_start_second_preload",
    [
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.net",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=1",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 1, normal: 0, cancelled: 1 }
  );
  await test_hint_redirect(
    "redirect_cross_origin_dont_use_first_preload",
    [
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image&a",
        host: "https://example.net",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=0",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 0, normal: 1, cancelled: 1 }
  );
});

add_task(async function redirect_same_origin() {
  await test_hint_redirect(
    "hint_before_redirect_same_origin",
    [
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.org",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=1",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 1, normal: 0, cancelled: 0 }
  );
  await test_hint_redirect(
    "hint_after_redirect_same_origin",
    [
      {
        link: "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
        host: "https://example.org",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=0",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 1, normal: 0, cancelled: 0 }
  );
  await test_hint_redirect(
    "hint_after_redirect_same_origin",
    [
      {
        link: "",
        host: "https://example.org",
      },
    ],
    "https://example.org/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=image&hinted=1",
    "https://example.org/browser/netwerk/test/browser/early_hint_asset.sjs?as=image",
    { hinted: 1, normal: 0, cancelled: 0 }
  );
});
