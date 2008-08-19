function run_test() {
  const ios = Components.classes["@mozilla.org/network/io-service;1"]
                        .getService(Components.interfaces.nsIIOService);

  var base = ios.newURI("http://localhost/bug379034/index.html", null, null);

  var uri = ios.newURI("http:a.html", null, base);
  do_check_eq(uri.spec, "http://localhost/bug379034/a.html");

  uri = ios.newURI("HtTp:b.html", null, base);
  do_check_eq(uri.spec, "http://localhost/bug379034/b.html");

  uri = ios.newURI("https:c.html", null, base);
  do_check_eq(uri.spec, "https://c.html/");

  uri = ios.newURI("./https:d.html", null, base);
  do_check_eq(uri.spec, "http://localhost/bug379034/https:d.html");
}
