//
//  HTTP Accept-Language header test
//

const Cc = Components.classes;
const Ci = Components.interfaces;

var testpath = "/bug672448";

function run_test() {
  test_accepted_languages();
}

function test_accepted_languages() {
  let channel = setupChannel(testpath);

  let AcceptLanguage = channel.getRequestHeader("Accept-Language");

  let acceptedLanguages = AcceptLanguage.split(",");

  for( let i = 0; i < acceptedLanguages.length; i++ ) {
    let acceptedLanguage, qualityValue;

    try {
      [_, acceptedLanguage, qualityValue] = acceptedLanguages[i].trim().match(/^([a-z0-9_-]*?)(?:;q=([0-9.]+))?$/i);
    } catch(e) {
      do_print("Invalid language tag or quality value: " + e);
    }

    if( i == 0 ) {
      do_check_eq(qualityValue, undefined); // First language shouldn't have a quality value.
    } else {
      do_check_eq(qualityValue.length, 5); // All other languages should have quality value of the format '0.123'.
    }
  }
}

function setupChannel(path) {
  let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  let chan = ios.newChannel("http://localhost:4444" + path, "", null);
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}
