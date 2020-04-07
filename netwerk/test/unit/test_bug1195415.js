// Test for bug 1195415

"use strict";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var ssm = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
    Ci.nsIScriptSecurityManager
  );

  // NON-UNICODE
  var uri = ios.newURI("http://foo.com/file.txt");
  Assert.equal(uri.asciiHostPort, "foo.com");
  uri = uri
    .mutate()
    .setPort(90)
    .finalize();
  var prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "foo.com:90");
  Assert.equal(prin.origin, "http://foo.com:90");

  uri = ios.newURI("http://foo.com:10/file.txt");
  Assert.equal(uri.asciiHostPort, "foo.com:10");
  uri = uri
    .mutate()
    .setPort(500)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "foo.com:500");
  Assert.equal(prin.origin, "http://foo.com:500");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "foo.com:5000");
  uri = uri
    .mutate()
    .setPort(20)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "foo.com:20");
  Assert.equal(prin.origin, "http://foo.com:20");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "foo.com:5000");
  uri = uri
    .mutate()
    .setPort(-1)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "foo.com");
  Assert.equal(prin.origin, "http://foo.com");

  uri = ios.newURI("http://foo.com:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "foo.com:5000");
  uri = uri
    .mutate()
    .setPort(80)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "foo.com");
  Assert.equal(prin.origin, "http://foo.com");

  // UNICODE
  uri = ios.newURI("http://jos\u00e9.example.net.ch/file.txt");
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch");
  uri = uri
    .mutate()
    .setPort(90)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch:90");
  Assert.equal(prin.origin, "http://xn--jos-dma.example.net.ch:90");

  uri = ios.newURI("http://jos\u00e9.example.net.ch:10/file.txt");
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch:10");
  uri = uri
    .mutate()
    .setPort(500)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch:500");
  Assert.equal(prin.origin, "http://xn--jos-dma.example.net.ch:500");

  uri = ios.newURI("http://jos\u00e9.example.net.ch:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch:5000");
  uri = uri
    .mutate()
    .setPort(20)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch:20");
  Assert.equal(prin.origin, "http://xn--jos-dma.example.net.ch:20");

  uri = ios.newURI("http://jos\u00e9.example.net.ch:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch:5000");
  uri = uri
    .mutate()
    .setPort(-1)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch");
  Assert.equal(prin.origin, "http://xn--jos-dma.example.net.ch");

  uri = ios.newURI("http://jos\u00e9.example.net.ch:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch:5000");
  uri = uri
    .mutate()
    .setPort(80)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "xn--jos-dma.example.net.ch");
  Assert.equal(prin.origin, "http://xn--jos-dma.example.net.ch");

  // ipv6
  uri = ios.newURI("http://[123:45::678]/file.txt");
  Assert.equal(uri.asciiHostPort, "[123:45::678]");
  uri = uri
    .mutate()
    .setPort(90)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "[123:45::678]:90");
  Assert.equal(prin.origin, "http://[123:45::678]:90");

  uri = ios.newURI("http://[123:45::678]:10/file.txt");
  Assert.equal(uri.asciiHostPort, "[123:45::678]:10");
  uri = uri
    .mutate()
    .setPort(500)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "[123:45::678]:500");
  Assert.equal(prin.origin, "http://[123:45::678]:500");

  uri = ios.newURI("http://[123:45::678]:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "[123:45::678]:5000");
  uri = uri
    .mutate()
    .setPort(20)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "[123:45::678]:20");
  Assert.equal(prin.origin, "http://[123:45::678]:20");

  uri = ios.newURI("http://[123:45::678]:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "[123:45::678]:5000");
  uri = uri
    .mutate()
    .setPort(-1)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "[123:45::678]");
  Assert.equal(prin.origin, "http://[123:45::678]");

  uri = ios.newURI("http://[123:45::678]:5000/file.txt");
  Assert.equal(uri.asciiHostPort, "[123:45::678]:5000");
  uri = uri
    .mutate()
    .setPort(80)
    .finalize();
  prin = ssm.createContentPrincipal(uri, {});
  Assert.equal(uri.asciiHostPort, "[123:45::678]");
  Assert.equal(prin.origin, "http://[123:45::678]");
}
