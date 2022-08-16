"use strict";

var valid_URIs = [
  "http://[::]/",
  "http://[::1]/",
  "http://[1::]/",
  "http://[::]/",
  "http://[::1]/",
  "http://[1::]/",
  "http://[1:2:3:4:5:6:7::]/",
  "http://[::1:2:3:4:5:6:7]/",
  "http://[1:2:a:B:c:D:e:F]/",
  "http://[1::8]/",
  "http://[1:2::8]/",
  "http://[0000:0123:4567:89AB:CDEF:abcd:ef00:0000]/",
  "http://[::192.168.1.1]/",
  "http://[1::0.0.0.0]/",
  "http://[1:2::255.255.255.255]/",
  "http://[1:2:3::255.255.255.255]/",
  "http://[1:2:3:4::255.255.255.255]/",
  "http://[1:2:3:4:5::255.255.255.255]/",
  "http://[1:2:3:4:5:6:255.255.255.255]/",
];

var invalid_URIs = [
  "http://[1]/",
  "http://[192.168.1.1]/",
  "http://[:::]/",
  "http://[:::1]/",
  "http://[1:::]/",
  "http://[::1::]/",
  "http://[1:2:3:4:5:6:7:]/",
  "http://[:2:3:4:5:6:7:8]/",
  "http://[1:2:3:4:5:6:7:8:]/",
  "http://[:1:2:3:4:5:6:7:8]/",
  "http://[1:2:3:4:5:6:7:8::]/",
  "http://[::1:2:3:4:5:6:7:8]/",
  "http://[1:2:3:4:5:6:7]/",
  "http://[1:2:3:4:5:6:7:8:9]/",
  "http://[00001:2:3:4:5:6:7:8]/",
  "http://[0001:2:3:4:5:6:7:89abc]/",
  "http://[A:b:C:d:E:f:G:h]/",
  "http://[::192.168.1]/",
  "http://[::192.168.1.]/",
  "http://[::.168.1.1]/",
  "http://[::192..1.1]/",
  "http://[::0192.168.1.1]/",
  "http://[::256.255.255.255]/",
  "http://[::1x.255.255.255]/",
  "http://[::192.4294967464.1.1]/",
  "http://[1:2:3:4:5:6::255.255.255.255]/",
  "http://[1:2:3:4:5:6:7:255.255.255.255]/",
];

function run_test() {
  for (let i = 0; i < valid_URIs.length; i++) {
    try {
      Services.io.newURI(valid_URIs[i]);
    } catch (e) {
      do_throw("cannot create URI:" + valid_URIs[i]);
    }
  }

  for (let i = 0; i < invalid_URIs.length; i++) {
    try {
      Services.io.newURI(invalid_URIs[i]);
      do_throw("should throw: " + invalid_URIs[i]);
    } catch (e) {
      Assert.equal(e.result, Cr.NS_ERROR_MALFORMED_URI);
    }
  }
}
