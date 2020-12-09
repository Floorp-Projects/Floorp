/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function canonicalize(url) {
  let urlUtils = Cc["@mozilla.org/url-classifier/utils;1"].getService(
    Ci.nsIUrlClassifierUtils
  );

  let uri = Services.io.newURI(url);
  return uri.scheme + "://" + urlUtils.getKeyForURI(uri);
}

function run_test() {
  // These testcases are from
  // https://developers.google.com/safe-browsing/v4/urls-hashing
  equal(canonicalize("http://host/%25%32%35"), "http://host/%25");
  equal(canonicalize("http://host/%25%32%35%25%32%35"), "http://host/%25%25");
  equal(canonicalize("http://host/%2525252525252525"), "http://host/%25");
  equal(canonicalize("http://host/asdf%25%32%35asd"), "http://host/asdf%25asd");
  equal(
    canonicalize("http://host/%%%25%32%35asd%%"),
    "http://host/%25%25%25asd%25%25"
  );
  equal(canonicalize("http://www.google.com/"), "http://www.google.com/");
  equal(
    canonicalize(
      "http://%31%36%38%2e%31%38%38%2e%39%39%2e%32%36/%2E%73%65%63%75%72%65/%77%77%77%2E%65%62%61%79%2E%63%6F%6D/"
    ),
    "http://168.188.99.26/.secure/www.ebay.com/"
  );
  equal(
    canonicalize(
      "http://195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/"
    ),
    "http://195.127.0.11/uploads/%20%20%20%20/.verify/.eBaysecure=updateuserdataxplimnbqmn-xplmvalidateinfoswqpcmlx=hgplmcx/"
  );
  equal(canonicalize("http://3279880203/blah"), "http://195.127.0.11/blah");
  equal(
    canonicalize("http://www.google.com/blah/.."),
    "http://www.google.com/"
  );
  equal(
    canonicalize("http://www.evil.com/blah#frag"),
    "http://www.evil.com/blah"
  );
  equal(canonicalize("http://www.GOOgle.com/"), "http://www.google.com/");
  equal(canonicalize("http://www.google.com.../"), "http://www.google.com/");
  equal(
    canonicalize("http://www.google.com/foo\tbar\rbaz\n2"),
    "http://www.google.com/foobarbaz2"
  );
  equal(canonicalize("http://www.google.com/q?"), "http://www.google.com/q?");
  equal(
    canonicalize("http://www.google.com/q?r?"),
    "http://www.google.com/q?r?"
  );
  equal(
    canonicalize("http://www.google.com/q?r?s"),
    "http://www.google.com/q?r?s"
  );
  equal(canonicalize("http://evil.com/foo#bar#baz"), "http://evil.com/foo");
  equal(canonicalize("http://evil.com/foo;"), "http://evil.com/foo;");
  equal(canonicalize("http://evil.com/foo?bar;"), "http://evil.com/foo?bar;");
  equal(
    canonicalize("http://notrailingslash.com"),
    "http://notrailingslash.com/"
  );
  equal(
    canonicalize("http://www.gotaport.com:1234/"),
    "http://www.gotaport.com/"
  );
  equal(
    canonicalize("https://www.securesite.com/"),
    "https://www.securesite.com/"
  );
  equal(canonicalize("http://host.com/ab%23cd"), "http://host.com/ab%23cd");
  equal(
    canonicalize("http://host.com//twoslashes?more//slashes"),
    "http://host.com/twoslashes?more//slashes"
  );
}
