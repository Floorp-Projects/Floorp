// test that methods are not normalized

"use strict";

const testMethods = [
  ["GET"],
  ["get"],
  ["Get"],
  ["gET"],
  ["gEt"],
  ["post"],
  ["POST"],
  ["head"],
  ["HEAD"],
  ["put"],
  ["PUT"],
  ["delete"],
  ["DELETE"],
  ["connect"],
  ["CONNECT"],
  ["options"],
  ["trace"],
  ["track"],
  ["copy"],
  ["index"],
  ["lock"],
  ["m-post"],
  ["mkcol"],
  ["move"],
  ["propfind"],
  ["proppatch"],
  ["unlock"],
  ["link"],
  ["LINK"],
  ["foo"],
  ["foO"],
  ["fOo"],
  ["Foo"],
];

function run_test() {
  var chan = NetUtil.newChannel({
    uri: "http://localhost/",
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);

  for (var i = 0; i < testMethods.length; i++) {
    chan.requestMethod = testMethods[i];
    Assert.equal(chan.requestMethod, testMethods[i]);
  }
}
