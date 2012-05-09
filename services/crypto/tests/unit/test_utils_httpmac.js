/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");

function run_test() {
  initTestLogging();

  run_next_test();
}

add_test(function test_sha1() {
  _("Ensure HTTP MAC SHA1 generation works as expected.");

  let id = "vmo1txkttblmn51u2p3zk2xiy16hgvm5ok8qiv1yyi86ffjzy9zj0ez9x6wnvbx7";
  let key = "b8u1cc5iiio5o319og7hh8faf2gi5ym4aq0zwf112cv1287an65fudu5zj7zo7dz";
  let ts = 1329181221;
  let method = "GET";
  let nonce = "wGX71";
  let uri = CommonUtils.makeURI("http://10.250.2.176/alias/");

  let result = CryptoUtils.computeHTTPMACSHA1(id, key, method, uri,
                                              {ts: ts, nonce: nonce});

  do_check_eq(btoa(result.mac), "jzh5chjQc2zFEvLbyHnPdX11Yck=");

  do_check_eq(result.getHeader(),
              'MAC id="vmo1txkttblmn51u2p3zk2xiy16hgvm5ok8qiv1yyi86ffjzy9zj0ez9x6wnvbx7", ' +
              'ts="1329181221", nonce="wGX71", mac="jzh5chjQc2zFEvLbyHnPdX11Yck="');

  let ext = "EXTRA DATA; foo,bar=1";

  let result = CryptoUtils.computeHTTPMACSHA1(id, key, method, uri,
                                              {ts: ts, nonce: nonce, ext: ext});
  do_check_eq(btoa(result.mac), "bNf4Fnt5k6DnhmyipLPkuZroH68=");
  do_check_eq(result.getHeader(),
              'MAC id="vmo1txkttblmn51u2p3zk2xiy16hgvm5ok8qiv1yyi86ffjzy9zj0ez9x6wnvbx7", ' +
              'ts="1329181221", nonce="wGX71", mac="bNf4Fnt5k6DnhmyipLPkuZroH68=", ' +
              'ext="EXTRA DATA; foo,bar=1"');

  run_next_test();
});

add_test(function test_nonce_length() {
  _("Ensure custom nonce lengths are honoured.");

  function get_mac(length) {
    let uri = CommonUtils.makeURI("http://example.com/");
    return CryptoUtils.computeHTTPMACSHA1("foo", "bar", "GET", uri, {
      nonce_bytes: length
    });
  }

  let result = get_mac(12);
  do_check_eq(12, atob(result.nonce).length);

  let result = get_mac(2);
  do_check_eq(2, atob(result.nonce).length);

  let result = get_mac(0);
  do_check_eq(8, atob(result.nonce).length);

  let result = get_mac(-1);
  do_check_eq(8, atob(result.nonce).length);

  run_next_test();
});
