function run_test() {
  const ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var uri = ios.newURI("http://foo.com/file.txt");
  uri.port = 90;
  Assert.equal(uri.hostPort, "foo.com:90");

  uri = ios.newURI("http://foo.com:10/file.txt");
  uri.port = 500;
  Assert.equal(uri.hostPort, "foo.com:500");
  
  uri = ios.newURI("http://foo.com:5000/file.txt");
  uri.port = 20;
  Assert.equal(uri.hostPort, "foo.com:20");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  uri.port = -1;
  Assert.equal(uri.hostPort, "foo.com");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  uri.port = 80;
  Assert.equal(uri.hostPort, "foo.com");
}
