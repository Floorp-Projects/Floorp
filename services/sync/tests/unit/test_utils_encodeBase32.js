Cu.import("resource://services-sync/util.js");

function run_test() {
  // Test vectors from RFC 4648
  do_check_eq(Utils.encodeBase32(""), "");
  do_check_eq(Utils.encodeBase32("f"), "MY======");
  do_check_eq(Utils.encodeBase32("fo"), "MZXQ====");
  do_check_eq(Utils.encodeBase32("foo"), "MZXW6===");
  do_check_eq(Utils.encodeBase32("foob"), "MZXW6YQ=");
  do_check_eq(Utils.encodeBase32("fooba"), "MZXW6YTB");
  do_check_eq(Utils.encodeBase32("foobar"), "MZXW6YTBOI======");

  do_check_eq(Utils.encodeBase32("Bacon is a vegetable."),
              "IJQWG33OEBUXGIDBEB3GKZ3FORQWE3DFFY======");
}
