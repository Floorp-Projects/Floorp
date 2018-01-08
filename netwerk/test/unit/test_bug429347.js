function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].
    getService(Ci.nsIIOService);

  var uri1 = ios.newURI("http://example.com#bar");
  var uri2 = ios.newURI("http://example.com/#bar");
  Assert.ok(uri1.equals(uri2));

  uri1 = uri1.mutate().setSpec("http://example.com?bar").finalize();
  uri2 = uri2.mutate().setSpec("http://example.com/?bar").finalize();
  Assert.ok(uri1.equals(uri2));

  // see https://bugzilla.mozilla.org/show_bug.cgi?id=665706
  // ";" is not parsed as special anymore and thus ends up
  // in the authority component (see RFC 3986)
  uri1 = uri1.mutate().setSpec("http://example.com;bar").finalize();
  uri2 = uri2.mutate().setSpec("http://example.com/;bar").finalize();
  Assert.ok(!uri1.equals(uri2));

  uri1 = uri1.mutate().setSpec("http://example.com#").finalize();
  uri2 = uri2.mutate().setSpec("http://example.com/#").finalize();
  Assert.ok(uri1.equals(uri2));

  uri1 = uri1.mutate().setSpec("http://example.com?").finalize();
  uri2 = uri2.mutate().setSpec("http://example.com/?").finalize();
  Assert.ok(uri1.equals(uri2));

  // see https://bugzilla.mozilla.org/show_bug.cgi?id=665706
  // ";" is not parsed as special anymore and thus ends up
  // in the authority component (see RFC 3986)
  uri1 = uri1.mutate().setSpec("http://example.com;").finalize();
  uri2 = uri2.mutate().setSpec("http://example.com/;").finalize();
  Assert.ok(!uri1.equals(uri2));

  uri1 = uri1.mutate().setSpec("http://example.com").finalize();
  uri2 = uri2.mutate().setSpec("http://example.com/").finalize();
  Assert.ok(uri1.equals(uri2));
}
