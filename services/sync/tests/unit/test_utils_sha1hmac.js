Cu.import("resource://services-sync/util.js");

function _hexToString(hex) {
  var ret = '';
  if (hex.length % 2 != 0) {
    return false;
  }

  for (var i = 0; i < hex.length; i += 2) {
    var cur = hex[i] + hex[i + 1];
    ret += String.fromCharCode(parseInt(cur, 16));
  }
  return ret;
}

function run_test() {
  let test_data = 
   [{test_case:     1,
     key:           _hexToString("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b"),
     key_len:       20,
     data:          "Hi There",
     data_len:      8,
     digest:        "b617318655057264e28bc0b6fb378c8ef146be00"},

    {test_case:     2,
     key:           "Jefe",
     key_len:       4,
     data:          "what do ya want for nothing?",
     data_len:      28,
     digest:        "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79"}];
  
  let d;
  // Testing repeated hashing.
  d = test_data[0];
  do_check_eq(Utils.bytesAsHex(Utils.sha1HMACBytes(d.data, Utils.makeHMACKey(d.key))), d.digest);
  d = test_data[1];
  do_check_eq(Utils.bytesAsHex(Utils.sha1HMACBytes(d.data, Utils.makeHMACKey(d.key))), d.digest);
  d = test_data[0];
  do_check_eq(Utils.bytesAsHex(Utils.sha1HMACBytes(d.data, Utils.makeHMACKey(d.key))), d.digest);
  d = test_data[1];
  do_check_eq(Utils.bytesAsHex(Utils.sha1HMACBytes(d.data, Utils.makeHMACKey(d.key))), d.digest);
  let kk = Utils.makeHMACKey(d.key);
  do_check_eq(
      Utils.bytesAsHex(
        Utils.sha1HMACBytes(
          Utils.sha1HMACBytes(
            Utils.sha1HMACBytes(d.data, kk),
            kk),
          kk)),
      Utils.bytesAsHex(
        Utils.sha1HMACBytes(
          Utils.sha1HMACBytes(
            Utils.sha1HMACBytes(d.data, kk),
            kk),
          kk)));
  
  d = test_data[0];
  kk = Utils.makeHMACKey(d.key);
  do_check_eq(Utils.bytesAsHex(Utils.sha1HMACBytes(d.data, kk)), d.digest);
  do_check_eq(Utils.bytesAsHex(Utils.sha1HMACBytes(d.data, kk)), d.digest);
}
