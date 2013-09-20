function run_test() {
  const ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var uri = ios.newURI("http://foo.com/file.txt", null, null);
  uri.port = 90;
  do_check_eq(uri.hostPort, "foo.com:90");

  uri = ios.newURI("http://foo.com:10/file.txt", null, null);
  uri.port = 500;
  do_check_eq(uri.hostPort, "foo.com:500");
  
  uri = ios.newURI("http://foo.com:5000/file.txt", null, null);
  uri.port = 20;
  do_check_eq(uri.hostPort, "foo.com:20");

  uri = ios.newURI("http://foo.com:5000/file.txt", null, null);
  uri.port = -1;
  do_check_eq(uri.hostPort, "foo.com");

  uri = ios.newURI("http://foo.com:5000/file.txt", null, null);
  uri.port = 80;
  do_check_eq(uri.hostPort, "foo.com");
}
