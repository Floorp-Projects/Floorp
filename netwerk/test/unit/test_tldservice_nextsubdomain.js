var Cc = Components.classes;
var Ci = Components.interfaces;

function run_test() {
  var tld = Cc["@mozilla.org/network/effective-tld-service;1"]
              .getService(Ci.nsIEffectiveTLDService);

  var tests = [
    { data: "bar.foo.co.uk", result: "foo.co.uk" },
    { data: "foo.bar.foo.co.uk", result: "bar.foo.co.uk" },
    { data: "foo.co.uk", throw: true },
    { data: "co.uk", throw: true },
    { data: ".co.uk", throw: true },
    { data: "com", throw: true },
    { data: "tûlîp.foo.fr", result: "foo.fr" },
    { data: "tûlîp.fôû.fr", result: "xn--f-xgav.fr" },
    { data: "file://foo/bar", throw: true },
  ];

  tests.forEach(function(test) {
    try {
      var r = tld.getNextSubDomain(test.data);
      do_check_eq(r, test.result);
    } catch (e) {
      do_check_true(test.throw);
    }
  });
}
