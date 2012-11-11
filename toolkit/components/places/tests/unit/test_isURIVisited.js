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

const TRANSITIONS = [
  TRANSITION_LINK,
  TRANSITION_TYPED,
  TRANSITION_BOOKMARK,
  TRANSITION_EMBED,
  TRANSITION_FRAMED_LINK,
  TRANSITION_REDIRECT_PERMANENT,
  TRANSITION_REDIRECT_TEMPORARY,
  TRANSITION_DOWNLOAD,
];

let gRunner;
function run_test()
{
  do_test_pending();
  gRunner = step();
  gRunner.next();
}

function step()
{
  let history = Cc["@mozilla.org/browser/history;1"]
                  .getService(Ci.mozIAsyncHistory);

  for (let scheme in SCHEMES) {
    do_log_info("Testing scheme " + scheme);
    for (let i = 0; i < TRANSITIONS.length; i++) {
      let transition = TRANSITIONS[i];
      do_log_info("With transition " + transition);

      let uri = NetUtil.newURI(scheme + "mozilla.org/");

      history.isURIVisited(uri, function(aURI, aIsVisited) {
        do_check_true(uri.equals(aURI));
        do_check_false(aIsVisited);

        let callback = {
          handleError:  function () {},
          handleResult: function () {},
          handleCompletion: function () {
            do_log_info("Added visit to " + uri.spec);

            history.isURIVisited(uri, function (aURI, aIsVisited) {
              do_check_true(uri.equals(aURI));
              let checker = SCHEMES[scheme] ? do_check_true : do_check_false;
              checker(aIsVisited);

              promiseClearHistory().then(function () {
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
      yield;
    }
  }

  do_test_finished();
}
