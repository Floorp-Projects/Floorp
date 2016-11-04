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

var gRunner;
function run_test()
{
  do_test_pending();
  gRunner = step();
  gRunner.next();
}

function* step()
{
  let history = Cc["@mozilla.org/browser/history;1"]
                  .getService(Ci.mozIAsyncHistory);

  for (let scheme in SCHEMES) {
    do_print("Testing scheme " + scheme);
    for (let t in PlacesUtils.history.TRANSITIONS) {
      do_print("With transition " + t);
      let transition = PlacesUtils.history.TRANSITIONS[t];

      let uri = NetUtil.newURI(scheme + "mozilla.org/");

      history.isURIVisited(uri, function(aURI, aIsVisited) {
        do_check_true(uri.equals(aURI));
        do_check_false(aIsVisited);

        let callback = {
          handleError:  function () {},
          handleResult: function () {},
          handleCompletion: function () {
            do_print("Added visit to " + uri.spec);

            history.isURIVisited(uri, function (aURI, aIsVisited) {
              do_check_true(uri.equals(aURI));
              let checker = SCHEMES[scheme] ? do_check_true : do_check_false;
              checker(aIsVisited);

              PlacesTestUtils.clearHistory().then(function () {
                history.isURIVisited(uri, function(aURI, aIsVisited) {
                  do_check_true(uri.equals(aURI));
                  do_check_false(aIsVisited);
                  gRunner.next();
                });
              });
            });
          },
        };

        history.updatePlaces({ uri:    uri
                             , visits: [ { transitionType: transition
                                         , visitDate:      Date.now() * 1000
                                         } ]
                             }, callback);
      });
      yield undefined;
    }
  }

  do_test_finished();
}
