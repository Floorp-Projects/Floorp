/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");

function run_test() {
  initTestLogging();

  run_next_test();
}

add_test(function test_hawk() {
  let compute = CryptoUtils.computeHAWK;

  // vectors copied from the HAWK (node.js) tests
  let credentials_sha1 = {
    id: "123456",
    key: "2983d45yun89q",
    algorithm: "sha1",
  };

  let method = "POST";
  let ts = 1353809207;
  let nonce = "Ygvqdz";
  let result;

  let uri_http = CommonUtils.makeURI("http://example.net/somewhere/over/the/rainbow");
  let sha1_opts = { credentials: credentials_sha1,
                    ext: "Bazinga!",
                    ts,
                    nonce,
                    payload: "something to write about",
                  };
  result = compute(uri_http, method, sha1_opts);

  // The HAWK spec uses non-urlsafe base64 (+/) for its output MAC string.
  Assert.equal(result.field,
               'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
               'hash="bsvY3IfUllw6V5rvk4tStEvpBhE=", ext="Bazinga!", ' +
               'mac="qbf1ZPG/r/e06F4ht+T77LXi5vw="'
             );
  Assert.equal(result.artifacts.ts, ts);
  Assert.equal(result.artifacts.nonce, nonce);
  Assert.equal(result.artifacts.method, method);
  Assert.equal(result.artifacts.resource, "/somewhere/over/the/rainbow");
  Assert.equal(result.artifacts.host, "example.net");
  Assert.equal(result.artifacts.port, 80);
  // artifacts.hash is the *payload* hash, not the overall request MAC.
  Assert.equal(result.artifacts.hash, "bsvY3IfUllw6V5rvk4tStEvpBhE=");
  Assert.equal(result.artifacts.ext, "Bazinga!");

  let credentials_sha256 = {
    id: "123456",
    key: "2983d45yun89q",
    algorithm: "sha256",
  };

  let uri_https = CommonUtils.makeURI("https://example.net/somewhere/over/the/rainbow");
  let sha256_opts = { credentials: credentials_sha256,
                      ext: "Bazinga!",
                      ts,
                      nonce,
                      payload: "something to write about",
                      contentType: "text/plain",
                    };

  result = compute(uri_https, method, sha256_opts);
  Assert.equal(result.field,
               'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
               'hash="2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=", ' +
               'ext="Bazinga!", ' +
               'mac="q1CwFoSHzPZSkbIvl0oYlD+91rBUEvFk763nMjMndj8="'
             );
  Assert.equal(result.artifacts.ts, ts);
  Assert.equal(result.artifacts.nonce, nonce);
  Assert.equal(result.artifacts.method, method);
  Assert.equal(result.artifacts.resource, "/somewhere/over/the/rainbow");
  Assert.equal(result.artifacts.host, "example.net");
  Assert.equal(result.artifacts.port, 443);
  Assert.equal(result.artifacts.hash, "2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=");
  Assert.equal(result.artifacts.ext, "Bazinga!");

  let sha256_opts_noext = { credentials: credentials_sha256,
                            ts,
                            nonce,
                            payload: "something to write about",
                            contentType: "text/plain",
                          };
  result = compute(uri_https, method, sha256_opts_noext);
  Assert.equal(result.field,
               'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
               'hash="2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=", ' +
               'mac="HTgtd0jPI6E4izx8e4OHdO36q00xFCU0FolNq3RiCYs="'
             );
  Assert.equal(result.artifacts.ts, ts);
  Assert.equal(result.artifacts.nonce, nonce);
  Assert.equal(result.artifacts.method, method);
  Assert.equal(result.artifacts.resource, "/somewhere/over/the/rainbow");
  Assert.equal(result.artifacts.host, "example.net");
  Assert.equal(result.artifacts.port, 443);
  Assert.equal(result.artifacts.hash, "2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=");

  /* Leaving optional fields out should work, although of course then we can't
   * assert much about the resulting hashes. The resulting header should look
   * roughly like:
   * Hawk id="123456", ts="1378764955", nonce="QkynqsrS44M=", mac="/C5NsoAs2fVn+d/I5wMfwe2Gr1MZyAJ6pFyDHG4Gf9U="
   */

  result = compute(uri_https, method, { credentials: credentials_sha256 });
  let fields = result.field.split(" ");
  Assert.equal(fields[0], "Hawk");
  Assert.equal(fields[1], 'id="123456",'); // from creds.id
  Assert.ok(fields[2].startsWith('ts="'));
  /* The HAWK spec calls for seconds-since-epoch, not ms-since-epoch.
   * Warning: this test will fail in the year 33658, and for time travellers
   * who journey earlier than 2001. Please plan accordingly. */
  Assert.ok(result.artifacts.ts > 1000 * 1000 * 1000);
  Assert.ok(result.artifacts.ts < 1000 * 1000 * 1000 * 1000);
  Assert.ok(fields[3].startsWith('nonce="'));
  Assert.equal(fields[3].length, ('nonce="12345678901=",').length);
  Assert.equal(result.artifacts.nonce.length, ("12345678901=").length);

  let result2 = compute(uri_https, method, { credentials: credentials_sha256 });
  Assert.notEqual(result.artifacts.nonce, result2.artifacts.nonce);

  /* Using an upper-case URI hostname shouldn't affect the hash. */

  let uri_https_upper = CommonUtils.makeURI("https://EXAMPLE.NET/somewhere/over/the/rainbow");
  result = compute(uri_https_upper, method, sha256_opts);
  Assert.equal(result.field,
               'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
               'hash="2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=", ' +
               'ext="Bazinga!", ' +
               'mac="q1CwFoSHzPZSkbIvl0oYlD+91rBUEvFk763nMjMndj8="'
             );

  /* Using a lower-case method name shouldn't affect the hash. */
  result = compute(uri_https_upper, method.toLowerCase(), sha256_opts);
  Assert.equal(result.field,
               'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
               'hash="2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=", ' +
               'ext="Bazinga!", ' +
               'mac="q1CwFoSHzPZSkbIvl0oYlD+91rBUEvFk763nMjMndj8="'
             );

  /* The localtimeOffsetMsec field should be honored. HAWK uses this to
   * compensate for clock skew between client and server: if the request is
   * rejected with a timestamp out-of-range error, the error includes the
   * server's time, and the client computes its clock offset and tries again.
   * Clients can remember this offset for a while.
   */

  result = compute(uri_https, method, { credentials: credentials_sha256,
                                        now: 1378848968650,
                                      });
  Assert.equal(result.artifacts.ts, 1378848968);

  result = compute(uri_https, method, { credentials: credentials_sha256,
                                        now: 1378848968650,
                                        localtimeOffsetMsec: 1000 * 1000,
                                      });
  Assert.equal(result.artifacts.ts, 1378848968 + 1000);

  /* Search/query-args in URIs should be included in the hash. */
  let makeURI = CommonUtils.makeURI;
  result = compute(makeURI("http://example.net/path"), method, sha256_opts);
  Assert.equal(result.artifacts.resource, "/path");
  Assert.equal(result.artifacts.mac, "WyKHJjWaeYt8aJD+H9UeCWc0Y9C+07ooTmrcrOW4MPI=");

  result = compute(makeURI("http://example.net/path/"), method, sha256_opts);
  Assert.equal(result.artifacts.resource, "/path/");
  Assert.equal(result.artifacts.mac, "xAYp2MgZQFvTKJT9u8nsvMjshCRRkuaeYqQbYSFp9Qw=");

  result = compute(makeURI("http://example.net/path?query=search"), method, sha256_opts);
  Assert.equal(result.artifacts.resource, "/path?query=search");
  Assert.equal(result.artifacts.mac, "C06a8pip2rA4QkBiosEmC32WcgFcW/R5SQC6kUWyqho=");

  /* Test handling of the payload, which is supposed to be a bytestring
  (String with codepoints from U+0000 to U+00FF, pre-encoded). */

  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                   });
  Assert.equal(result.artifacts.hash, undefined);
  Assert.equal(result.artifacts.mac, "S3f8E4hAURAqJxOlsYugkPZxLoRYrClgbSQ/3FmKMbY=");

  // Empty payload changes nothing.
  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     payload: null,
                   });
  Assert.equal(result.artifacts.hash, undefined);
  Assert.equal(result.artifacts.mac, "S3f8E4hAURAqJxOlsYugkPZxLoRYrClgbSQ/3FmKMbY=");

  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     payload: "hello",
                   });
  Assert.equal(result.artifacts.hash, "uZJnFj0XVBA6Rs1hEvdIDf8NraM0qRNXdFbR3NEQbVA=");
  Assert.equal(result.artifacts.mac, "pLsHHzngIn5CTJhWBtBr+BezUFvdd/IadpTp/FYVIRM=");

  // update, utf-8 payload
  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     payload: "andré@example.org", // non-ASCII
                   });
  Assert.equal(result.artifacts.hash, "66DiyapJ0oGgj09IXWdMv8VCg9xk0PL5RqX7bNnQW2k=");
  Assert.equal(result.artifacts.mac, "2B++3x5xfHEZbPZGDiK3IwfPZctkV4DUr2ORg1vIHvk=");

  /* If "hash" is provided, "payload" is ignored. */
  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     hash: "66DiyapJ0oGgj09IXWdMv8VCg9xk0PL5RqX7bNnQW2k=",
                     payload: "something else",
                   });
  Assert.equal(result.artifacts.hash, "66DiyapJ0oGgj09IXWdMv8VCg9xk0PL5RqX7bNnQW2k=");
  Assert.equal(result.artifacts.mac, "2B++3x5xfHEZbPZGDiK3IwfPZctkV4DUr2ORg1vIHvk=");

  // the payload "hash" is also non-urlsafe base64 (+/)
  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     payload: "something else",
                   });
  Assert.equal(result.artifacts.hash, "lERFXr/IKOaAoYw+eBseDUSwmqZTX0uKZpcWLxsdzt8=");
  Assert.equal(result.artifacts.mac, "jiZuhsac35oD7IdcblhFncBr8tJFHcwWLr8NIYWr9PQ=");

  /* Test non-ascii hostname. HAWK (via the node.js "url" module) punycodes
   * "ëxample.net" into "xn--xample-ova.net" before hashing. I still think
   * punycode was a bad joke that got out of the lab and into a spec.
   */

  result = compute(makeURI("http://ëxample.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                   });
  Assert.equal(result.artifacts.mac, "pILiHl1q8bbNQIdaaLwAFyaFmDU70MGehFuCs3AA5M0=");
  Assert.equal(result.artifacts.host, "xn--xample-ova.net");

  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     ext: "backslash=\\ quote=\" EOF",
                   });
  Assert.equal(result.artifacts.mac, "BEMW76lwaJlPX4E/dajF970T6+GzWvaeyLzUt8eOTOc=");
  Assert.equal(result.field, 'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ext="backslash=\\\\ quote=\\\" EOF", mac="BEMW76lwaJlPX4E/dajF970T6+GzWvaeyLzUt8eOTOc="');

  result = compute(makeURI("http://example.net:1234/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                   });
  Assert.equal(result.artifacts.mac, "6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE=");
  Assert.equal(result.field, 'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", mac="6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE="');

  /* HAWK (the node.js library) uses a URL parser which stores the "port"
   * field as a string, but makeURI() gives us an integer. So we'll diverge
   * on ports with a leading zero. This test vector would fail on the node.js
   * library (HAWK-1.1.1), where they get a MAC of
   * "T+GcAsDO8GRHIvZLeepSvXLwDlFJugcZroAy9+uAtcw=". I think HAWK should be
   * updated to do what we do here, so port="01234" should get the same hash
   * as port="1234".
   */
  result = compute(makeURI("http://example.net:01234/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                   });
  Assert.equal(result.artifacts.mac, "6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE=");
  Assert.equal(result.field, 'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", mac="6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE="');

  run_next_test();
});


add_test(function test_strip_header_attributes() {
  let strip = CryptoUtils.stripHeaderAttributes;

  Assert.equal(strip(undefined), "");
  Assert.equal(strip("text/plain"), "text/plain");
  Assert.equal(strip("TEXT/PLAIN"), "text/plain");
  Assert.equal(strip("  text/plain  "), "text/plain");
  Assert.equal(strip("text/plain ; charset=utf-8  "), "text/plain");

  run_next_test();
});
