/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests functionality of the isURIVisited API.

const SCHEMES = {
  "http://": true,
  "https://": true,
  "ftp://": true,
  "file:///": true,
  "about:": false,
// nsIIOService.newURI() can throw if e.g. the app knows about imap://
// but the account is not set up and so the URL is invalid for it.
//  "imap://": false,
  "news://": false,
  "mailbox:": false,
  "moz-anno:favicon:http://": false,
  "view-source:http://": false,
  "chrome://browser/content/browser.xul?": false,
  "resource://": false,
  "data:,": false,
  "wyciwyg:/0/http://": false,
  "javascript:": false,
};

add_task(async function test_isURIVisited() {
  let history = Cc["@mozilla.org/browser/history;1"]
                  .getService(Ci.mozIAsyncHistory);

  function visitsPromise(uri) {
    return new Promise(resolve => {
      history.isURIVisited(uri, (receivedURI, visited) => {
        resolve([receivedURI, visited]);
      });
    });
  }

  for (let scheme in SCHEMES) {
    info("Testing scheme " + scheme);
    for (let t in PlacesUtils.history.TRANSITIONS) {
      info("With transition " + t);
      let aTransition = PlacesUtils.history.TRANSITIONS[t];

      let aURI = Services.io.newURI(scheme + "mozilla.org/");

      let [receivedURI1, visited1] = await visitsPromise(aURI);
      Assert.ok(aURI.equals(receivedURI1));
      Assert.ok(!visited1);

      if (PlacesUtils.history.canAddURI(aURI)) {
        await PlacesTestUtils.addVisits([{
          uri: aURI,
          transition: aTransition
        }]);
        info("Added visit for " + aURI.spec);
      }

      let [receivedURI2, visited2] = await visitsPromise(aURI);
      Assert.ok(aURI.equals(receivedURI2));
      Assert.equal(SCHEMES[scheme], visited2);

      await PlacesUtils.history.clear();
      let [receivedURI3, visited3] = await visitsPromise(aURI);
      Assert.ok(aURI.equals(receivedURI3));
      Assert.ok(!visited3);
    }
  }
});
