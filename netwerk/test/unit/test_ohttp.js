/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test_known_config() {
  let ohttp = Cc["@mozilla.org/network/oblivious-http;1"].getService(
    Ci.nsIObliviousHttp
  );
  let encodedConfig = hexStringToBytes(
    "0100209403aafe76dfd4568481e04e44b42d744287eae4070b50e48baa7a91a4e80d5600080001000100010003"
  );
  let request = hexStringToBytes(
    "00034745540568747470730b6578616d706c652e636f6d012f"
  );
  let ohttpRequest = ohttp.encapsulateRequest(encodedConfig, request);
  ok(ohttpRequest);
}

function test_with_server() {
  let ohttp = Cc["@mozilla.org/network/oblivious-http;1"].getService(
    Ci.nsIObliviousHttp
  );
  let server = ohttp.server();
  ok(server.encodedConfig);
  let request = hexStringToBytes(
    "00034745540568747470730b6578616d706c652e636f6d012f"
  );
  let ohttpRequest = ohttp.encapsulateRequest(server.encodedConfig, request);
  let ohttpResponse = server.decapsulate(ohttpRequest.encRequest);
  ok(ohttpResponse);
  deepEqual(ohttpResponse.request, request);
  let response = hexStringToBytes("0140c8");
  let encResponse = ohttpResponse.encapsulate(response);
  deepEqual(ohttpRequest.response.decapsulate(encResponse), response);
}

function run_test() {
  test_known_config();
  test_with_server();
}
