_("Make sure sha256 hmac works with various messages and keys");
Cu.import("resource://weave/util.js");

function run_test() {
  let key1 = Svc.KeyFactory.keyFromString(Ci.nsIKeyObject.HMAC, "key1");
  let key2 = Svc.KeyFactory.keyFromString(Ci.nsIKeyObject.HMAC, "key2");

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
}
