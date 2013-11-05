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
                    ts: ts,
                    nonce: nonce,
                    payload: "something to write about",
                  };
  result = compute(uri_http, method, sha1_opts);

  // The HAWK spec uses non-urlsafe base64 (+/) for its output MAC string.
  do_check_eq(result.field,
              'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
              'hash="bsvY3IfUllw6V5rvk4tStEvpBhE=", ext="Bazinga!", ' +
              'mac="qbf1ZPG/r/e06F4ht+T77LXi5vw="'
             );
  do_check_eq(result.artifacts.ts, ts);
  do_check_eq(result.artifacts.nonce, nonce);
  do_check_eq(result.artifacts.method, method);
  do_check_eq(result.artifacts.resource, "/somewhere/over/the/rainbow");
  do_check_eq(result.artifacts.host, "example.net");
  do_check_eq(result.artifacts.port, 80);
  // artifacts.hash is the *payload* hash, not the overall request MAC.
  do_check_eq(result.artifacts.hash, "bsvY3IfUllw6V5rvk4tStEvpBhE=");
  do_check_eq(result.artifacts.ext, "Bazinga!");

  let credentials_sha256 = {
    id: "123456",
    key: "2983d45yun89q",
    algorithm: "sha256",
  };

  let uri_https = CommonUtils.makeURI("https://example.net/somewhere/over/the/rainbow");
  let sha256_opts = { credentials: credentials_sha256,
                      ext: "Bazinga!",
                      ts: ts,
                      nonce: nonce,
                      payload: "something to write about",
                      contentType: "text/plain",
                    };

  result = compute(uri_https, method, sha256_opts);
  do_check_eq(result.field,
              'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
              'hash="2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=", ' +
              'ext="Bazinga!", ' +
              'mac="q1CwFoSHzPZSkbIvl0oYlD+91rBUEvFk763nMjMndj8="'
             );
  do_check_eq(result.artifacts.ts, ts);
  do_check_eq(result.artifacts.nonce, nonce);
  do_check_eq(result.artifacts.method, method);
  do_check_eq(result.artifacts.resource, "/somewhere/over/the/rainbow");
  do_check_eq(result.artifacts.host, "example.net");
  do_check_eq(result.artifacts.port, 443);
  do_check_eq(result.artifacts.hash, "2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=");
  do_check_eq(result.artifacts.ext, "Bazinga!");

  let sha256_opts_noext = { credentials: credentials_sha256,
                            ts: ts,
                            nonce: nonce,
                            payload: "something to write about",
                            contentType: "text/plain",
                          };
  result = compute(uri_https, method, sha256_opts_noext);
  do_check_eq(result.field,
              'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
              'hash="2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=", ' +
              'mac="HTgtd0jPI6E4izx8e4OHdO36q00xFCU0FolNq3RiCYs="'
             );
  do_check_eq(result.artifacts.ts, ts);
  do_check_eq(result.artifacts.nonce, nonce);
  do_check_eq(result.artifacts.method, method);
  do_check_eq(result.artifacts.resource, "/somewhere/over/the/rainbow");
  do_check_eq(result.artifacts.host, "example.net");
  do_check_eq(result.artifacts.port, 443);
  do_check_eq(result.artifacts.hash, "2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=");

  /* Leaving optional fields out should work, although of course then we can't
   * assert much about the resulting hashes. The resulting header should look
   * roughly like:
   * Hawk id="123456", ts="1378764955", nonce="QkynqsrS44M=", mac="/C5NsoAs2fVn+d/I5wMfwe2Gr1MZyAJ6pFyDHG4Gf9U="
   */

  result = compute(uri_https, method, { credentials: credentials_sha256 });
  let fields = result.field.split(" ");
  do_check_eq(fields[0], "Hawk");
  do_check_eq(fields[1], 'id="123456",'); // from creds.id
  do_check_true(fields[2].startsWith('ts="'));
  /* The HAWK spec calls for seconds-since-epoch, not ms-since-epoch.
   * Warning: this test will fail in the year 33658, and for time travellers
   * who journey earlier than 2001. Please plan accordingly. */
  do_check_true(result.artifacts.ts > 1000*1000*1000);
  do_check_true(result.artifacts.ts < 1000*1000*1000*1000);
  do_check_true(fields[3].startsWith('nonce="'));
  do_check_eq(fields[3].length, ('nonce="12345678901=",').length);
  do_check_eq(result.artifacts.nonce.length, ("12345678901=").length);

  let result2 = compute(uri_https, method, { credentials: credentials_sha256 });
  do_check_neq(result.artifacts.nonce, result2.artifacts.nonce);

  /* Using an upper-case URI hostname shouldn't affect the hash. */

  let uri_https_upper = CommonUtils.makeURI("https://EXAMPLE.NET/somewhere/over/the/rainbow");
  result = compute(uri_https_upper, method, sha256_opts);
  do_check_eq(result.field,
              'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ' +
              'hash="2QfCt3GuY9HQnHWyWD3wX68ZOKbynqlfYmuO2ZBRqtY=", ' +
              'ext="Bazinga!", ' +
              'mac="q1CwFoSHzPZSkbIvl0oYlD+91rBUEvFk763nMjMndj8="'
             );

  /* Using a lower-case method name shouldn't affect the hash. */
  result = compute(uri_https_upper, method.toLowerCase(), sha256_opts);
  do_check_eq(result.field,
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
  do_check_eq(result.artifacts.ts, 1378848968);

  result = compute(uri_https, method, { credentials: credentials_sha256,
                                        now: 1378848968650,
                                        localtimeOffsetMsec: 1000*1000,
                                      });
  do_check_eq(result.artifacts.ts, 1378848968 + 1000);

  /* Search/query-args in URIs should be included in the hash. */
  let makeURI = CommonUtils.makeURI;
  result = compute(makeURI("http://example.net/path"), method, sha256_opts);
  do_check_eq(result.artifacts.resource, "/path");
  do_check_eq(result.artifacts.mac, "WyKHJjWaeYt8aJD+H9UeCWc0Y9C+07ooTmrcrOW4MPI=");

  result = compute(makeURI("http://example.net/path/"), method, sha256_opts);
  do_check_eq(result.artifacts.resource, "/path/");
  do_check_eq(result.artifacts.mac, "xAYp2MgZQFvTKJT9u8nsvMjshCRRkuaeYqQbYSFp9Qw=");

  result = compute(makeURI("http://example.net/path?query=search"), method, sha256_opts);
  do_check_eq(result.artifacts.resource, "/path?query=search");
  do_check_eq(result.artifacts.mac, "C06a8pip2rA4QkBiosEmC32WcgFcW/R5SQC6kUWyqho=");

  /* Test handling of the payload, which is supposed to be a bytestring
  (String with codepoints from U+0000 to U+00FF, pre-encoded). */

  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                   });
  do_check_eq(result.artifacts.hash, undefined);
  do_check_eq(result.artifacts.mac, "S3f8E4hAURAqJxOlsYugkPZxLoRYrClgbSQ/3FmKMbY=");

  // Empty payload changes nothing.
  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     payload: null,
                   });
  do_check_eq(result.artifacts.hash, undefined);
  do_check_eq(result.artifacts.mac, "S3f8E4hAURAqJxOlsYugkPZxLoRYrClgbSQ/3FmKMbY=");

  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     payload: "hello",
                   });
  do_check_eq(result.artifacts.hash, "uZJnFj0XVBA6Rs1hEvdIDf8NraM0qRNXdFbR3NEQbVA=");
  do_check_eq(result.artifacts.mac, "pLsHHzngIn5CTJhWBtBr+BezUFvdd/IadpTp/FYVIRM=");

  // update, utf-8 payload
  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     payload: "andré@example.org", // non-ASCII
                   });
  do_check_eq(result.artifacts.hash, "66DiyapJ0oGgj09IXWdMv8VCg9xk0PL5RqX7bNnQW2k=");
  do_check_eq(result.artifacts.mac, "2B++3x5xfHEZbPZGDiK3IwfPZctkV4DUr2ORg1vIHvk=");

  /* If "hash" is provided, "payload" is ignored. */
  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     hash: "66DiyapJ0oGgj09IXWdMv8VCg9xk0PL5RqX7bNnQW2k=",
                     payload: "something else",
                   });
  do_check_eq(result.artifacts.hash, "66DiyapJ0oGgj09IXWdMv8VCg9xk0PL5RqX7bNnQW2k=");
  do_check_eq(result.artifacts.mac, "2B++3x5xfHEZbPZGDiK3IwfPZctkV4DUr2ORg1vIHvk=");

  /* Test non-ascii hostname. HAWK (via the node.js "url" module) punycodes
   * "ëxample.net" into "xn--xample-ova.net" before hashing. I still think
   * punycode was a bad joke that got out of the lab and into a spec.
   */

  result = compute(makeURI("http://ëxample.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                   });
  do_check_eq(result.artifacts.mac, "pILiHl1q8bbNQIdaaLwAFyaFmDU70MGehFuCs3AA5M0=");
  do_check_eq(result.artifacts.host, "xn--xample-ova.net");

  result = compute(makeURI("http://example.net/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                     ext: "backslash=\\ quote=\" EOF",
                   });
  do_check_eq(result.artifacts.mac, "BEMW76lwaJlPX4E/dajF970T6+GzWvaeyLzUt8eOTOc=");
  do_check_eq(result.field, 'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", ext="backslash=\\\\ quote=\\\" EOF", mac="BEMW76lwaJlPX4E/dajF970T6+GzWvaeyLzUt8eOTOc="');

  result = compute(makeURI("http://example.net:1234/path"), method,
                   { credentials: credentials_sha256,
                     ts: 1353809207,
                     nonce: "Ygvqdz",
                   });
  do_check_eq(result.artifacts.mac, "6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE=");
  do_check_eq(result.field, 'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", mac="6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE="');

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
  do_check_eq(result.artifacts.mac, "6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE=");
  do_check_eq(result.field, 'Hawk id="123456", ts="1353809207", nonce="Ygvqdz", mac="6D3JSFDtozuq8QvJTNUc1JzeCfy6h5oRvlhmSTPv6LE="');

  run_next_test();
});


add_test(function test_strip_header_attributes() {
  let strip = CryptoUtils.stripHeaderAttributes;

  do_check_eq(strip(undefined), "");
  do_check_eq(strip("text/plain"), "text/plain");
  do_check_eq(strip("TEXT/PLAIN"), "text/plain");
  do_check_eq(strip("  text/plain  "), "text/plain");
  do_check_eq(strip("text/plain ; charset=utf-8  "), "text/plain");

  run_next_test();
});
