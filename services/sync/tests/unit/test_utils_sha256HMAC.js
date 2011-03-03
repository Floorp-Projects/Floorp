_("Make sure sha256 hmac works with various messages and keys");
Cu.import("resource://services-sync/util.js");

function run_test() {
  let key1 = Utils.makeHMACKey("key1");
  let key2 = Utils.makeHMACKey("key2");

  let mes1 = "message 1";
  let mes2 = "message 2";

  _("Make sure right sha256 hmacs are generated");
  let hmac11 = Utils.sha256HMAC(mes1, key1);
  do_check_eq(hmac11, "54f035bfbd6b44445d771c7c430e0753f1c00823974108fb4723703782552296");
  let hmac12 = Utils.sha256HMAC(mes1, key2);
  do_check_eq(hmac12, "1dbeae48de1b12f69517d828fb32969c74c8adc0715babc41b8f50254a980e70");
  let hmac21 = Utils.sha256HMAC(mes2, key1);
  do_check_eq(hmac21, "e00e91db4e86973868de8b3e818f4c968894d4135a3209bfea7b9e699484f07a");
  let hmac22 = Utils.sha256HMAC(mes2, key2);
  do_check_eq(hmac22, "4624312da31ada485b87beeecef0e5f0311cd5de60ea12291ce34cab158e0cc7");

  _("Repeated hmacs shouldn't change the digest");
  do_check_eq(Utils.sha256HMAC(mes1, key1), hmac11);
  do_check_eq(Utils.sha256HMAC(mes1, key2), hmac12);
  do_check_eq(Utils.sha256HMAC(mes2, key1), hmac21);
  do_check_eq(Utils.sha256HMAC(mes2, key2), hmac22);
  
  // RFC tests for SHA256-HMAC.
  let k1 = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
  let d1 = "\x48\x69\x20\x54\x68\x65\x72\x65";
  let o1 = "\xb0\x34\x4c\x61\xd8\xdb\x38\x53\x5c\xa8\xaf\xce\xaf\x0b\xf1\x2b\x88\x1d\xc2\x00\xc9\x83\x3d\xa7\x26\xe9\x37\x6c\x2e\x32\xcf\xf7";
  
  // Sample data for Marcus Wohlschon.
  let k2 = "78xR6gnAnBja7nYl6vkECo5bTlax5iHelP/jenj+dhQ=";
  let d2 = "4F5T+hK0VFBx880sYlMtfkBD2ZGe337tgbbQRTFxndFEgCC1fRrJkvzZ6Wytr+DySw3rxJ05O4Lqfn9F8Kxlvc4pcnAX//TK6MvRLs1NmcZr6HTo3NPurNB1VRTnJCE6";
  let o2 = "3dd17eb5091e0f2400a733f9e2cf8264d59206b6351078c2ce88499f1971f9b0";
    
  do_check_eq(Utils.sha256HMACBytes(d1, Utils.makeHMACKey(k1)), o1);
  do_check_eq(Utils.sha256HMAC(d2, Utils.makeHMACKey(k2)), o2);
  
  // Checking HMAC exceptions.
  let ex;
  try {
    Utils.throwHMACMismatch("aaa", "bbb");
  }
  catch (e) {
    ex = e;
  }
  do_check_true(Utils.isHMACMismatch(ex));
  do_check_false(!!Utils.isHMACMismatch(new Error()));
  do_check_false(!!Utils.isHMACMismatch(null));
  do_check_false(!!Utils.isHMACMismatch("Error"));
}
