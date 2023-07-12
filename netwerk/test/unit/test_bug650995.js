//
// Test that "max_entry_size" prefs for disk- and memory-cache prevents
// caching resources with size out of bounds
//

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

do_get_profile();

const prefService = Services.prefs;

const httpserver = new HttpServer();

// Repeats the given data until the total size is larger than 1K
function repeatToLargerThan1K(data) {
  while (data.length <= 1024) {
    data += data;
  }
  return data;
}

function setupChannel(suffix, value) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
    loadUsingSystemPrincipal: true,
  });
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  httpChan.setRequestHeader("x-request", value, false);

  return httpChan;
}

var tests = [
  new InitializeCacheDevices(true, false), // enable and create mem-device
  new TestCacheEntrySize(
    function () {
      prefService.setIntPref("browser.cache.memory.max_entry_size", 1);
    },
    "012345",
    "9876543210",
    "012345"
  ), // expect cached value
  new TestCacheEntrySize(
    function () {
      prefService.setIntPref("browser.cache.memory.max_entry_size", 1);
    },
    "0123456789a",
    "9876543210",
    "9876543210"
  ), // expect fresh value
  new TestCacheEntrySize(
    function () {
      prefService.setIntPref("browser.cache.memory.max_entry_size", -1);
    },
    "0123456789a",
    "9876543210",
    "0123456789a"
  ), // expect cached value

  new InitializeCacheDevices(false, true), // enable and create disk-device
  new TestCacheEntrySize(
    function () {
      prefService.setIntPref("browser.cache.disk.max_entry_size", 1);
    },
    "012345",
    "9876543210",
    "012345"
  ), // expect cached value
  new TestCacheEntrySize(
    function () {
      prefService.setIntPref("browser.cache.disk.max_entry_size", 1);
    },
    "0123456789a",
    "9876543210",
    "9876543210"
  ), // expect fresh value
  new TestCacheEntrySize(
    function () {
      prefService.setIntPref("browser.cache.disk.max_entry_size", -1);
    },
    "0123456789a",
    "9876543210",
    "0123456789a"
  ), // expect cached value
];

function nextTest() {
  // We really want each test to be self-contained. Make sure cache is
  // cleared and also let all operations finish before starting a new test
  syncWithCacheIOThread(function () {
    Services.cache2.clear();
    syncWithCacheIOThread(runNextTest);
  });
}

function runNextTest() {
  var aTest = tests.shift();
  if (!aTest) {
    httpserver.stop(do_test_finished);
    return;
  }
  executeSoon(function () {
    aTest.start();
  });
}

// Just make sure devices are created
function InitializeCacheDevices(memDevice, diskDevice) {
  this.start = function () {
    prefService.setBoolPref("browser.cache.memory.enable", memDevice);
    if (memDevice) {
      let cap = prefService.getIntPref("browser.cache.memory.capacity", 0);
      if (cap == 0) {
        prefService.setIntPref("browser.cache.memory.capacity", 1024);
      }
    }
    prefService.setBoolPref("browser.cache.disk.enable", diskDevice);
    if (diskDevice) {
      let cap = prefService.getIntPref("browser.cache.disk.capacity", 0);
      if (cap == 0) {
        prefService.setIntPref("browser.cache.disk.capacity", 1024);
      }
    }
    var channel = setupChannel("/bug650995", "Initial value");
    channel.asyncOpen(new ChannelListener(nextTest, null));
  };
}

function TestCacheEntrySize(
  setSizeFunc,
  firstRequest,
  secondRequest,
  secondExpectedReply
) {
  // Initially, this test used 10 bytes as the limit for caching entries.
  // Since we now use 1K granularity we have to extend lengths to be larger
  // than 1K if it is larger than 10
  if (firstRequest.length > 10) {
    firstRequest = repeatToLargerThan1K(firstRequest);
  }
  if (secondExpectedReply.length > 10) {
    secondExpectedReply = repeatToLargerThan1K(secondExpectedReply);
  }

  this.start = function () {
    setSizeFunc();
    var channel = setupChannel("/bug650995", firstRequest);
    channel.asyncOpen(new ChannelListener(this.initialLoad, this));
  };
  this.initialLoad = function (request, data, ctx) {
    Assert.equal(firstRequest, data);
    var channel = setupChannel("/bug650995", secondRequest);
    executeSoon(function () {
      channel.asyncOpen(new ChannelListener(ctx.testAndTriggerNext, ctx));
    });
  };
  this.testAndTriggerNext = function (request, data, ctx) {
    Assert.equal(secondExpectedReply, data);
    executeSoon(nextTest);
  };
}

function run_test() {
  httpserver.registerPathHandler("/bug650995", handler);
  httpserver.start(-1);

  prefService.setBoolPref("network.http.rcwn.enabled", false);

  nextTest();
  do_test_pending();
}

function handler(metadata, response) {
  var body = "BOOM!";
  try {
    body = metadata.getHeader("x-request");
  } catch (e) {}

  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Cache-Control", "max-age=3600", false);
  response.bodyOutputStream.write(body, body.length);
}
