/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

let indexes_10_100 = [
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  11,
  12,
  13,
  14,
  16,
  18,
  20,
  22,
  24,
  27,
  30,
  33,
  37,
  41,
  46,
  51,
  57,
  63,
  70,
  78,
  87,
  97,
  108,
  120,
  133,
  148,
  165,
  184,
  205,
  228,
  254,
  282,
  314,
  349,
  388,
  431,
  479,
  533,
  593,
  659,
  733,
  815,
  906,
  1008,
  1121,
  1247,
  1387,
  1542,
  1715,
  1907,
  2121,
  2359,
  2623,
  2917,
  3244,
  3607,
  4011,
  4460,
  4960,
  5516,
  6134,
  6821,
  7585,
  8435,
  9380,
  10431,
  11600,
  12900,
  14345,
  15952,
  17739,
  19727,
  21937,
  24395,
  27129,
  30169,
  33549,
  37308,
  41488,
  46137,
  51307,
  57056,
  63449,
  70559,
  78465,
  87257,
  97035,
  107908,
  120000,
];

let indexes_gt_100 = [
  0,
  30000,
  30643,
  31300,
  31971,
  32657,
  33357,
  34072,
  34803,
  35549,
  36311,
  37090,
  37885,
  38697,
  39527,
  40375,
  41241,
  42125,
  43028,
  43951,
  44894,
  45857,
  46840,
  47845,
  48871,
  49919,
  50990,
  52084,
  53201,
  54342,
  55507,
  56697,
  57913,
  59155,
  60424,
  61720,
  63044,
  64396,
  65777,
  67188,
  68629,
  70101,
  71604,
  73140,
  74709,
  76311,
  77948,
  79620,
  81327,
  83071,
  84853,
  86673,
  88532,
  90431,
  92370,
  94351,
  96374,
  98441,
  100552,
  102708,
  104911,
  107161,
  109459,
  111806,
  114204,
  116653,
  119155,
  121710,
  124320,
  126986,
  129709,
  132491,
  135332,
  138234,
  141199,
  144227,
  147320,
  150479,
  153706,
  157002,
  160369,
  163808,
  167321,
  170909,
  174574,
  178318,
  182142,
  186048,
  190038,
  194114,
  198277,
  202529,
  206872,
  211309,
  215841,
  220470,
  225198,
  230028,
  234961,
  240000,
];

registerCleanupFunction(async () => {
  http3_clear_prefs();
  Services.prefs.clearUserPref(
    "toolkit.telemetry.testing.overrideProductsCheck"
  );
});

add_task(async function setup() {
  // Enable the collection (during test) for all products so even products
  // that don't collect the data will be able to run the test without failure.
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  await http3_setup_tests("h3-29");
});

let Http3Listener = function() {};

Http3Listener.prototype = {
  onStartRequest: function testOnStartRequest(request) {
    Assert.equal(request.status, Cr.NS_OK);
    Assert.equal(request.responseStatus, 200);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3-29");
    this.finish();
  },
};

function chanPromise(chan, listener) {
  return new Promise(resolve => {
    function finish(result) {
      resolve(result);
    }
    listener.finish = finish;
    chan.asyncOpen(listener);
  });
}

function makeChan(uri, amount) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;

  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.data = generateContent(amount);
  let uchan = chan.QueryInterface(Ci.nsIUploadChannel);
  uchan.setUploadStream(stream, "text/plain", stream.available());
  chan.requestMethod = "POST";
  return chan;
}

// Generate a post.
function generateContent(size) {
  let content = "";
  for (let i = 0; i < size; i++) {
    content += "0";
  }
  return content;
}

async function test_large_post(amount, hist_name, key, indexes) {
  let hist = TelemetryTestUtils.getAndClearKeyedHistogram(hist_name);

  let listener = new Http3Listener();
  listener.amount = amount;
  let chan = makeChan("https://foo.example.com/post", amount);
  let tchan = chan.QueryInterface(Ci.nsITimedChannel);
  tchan.timingEnabled = true;
  await chanPromise(chan, listener);

  let time = (tchan.responseStartTime - tchan.requestStartTime) / 1000;
  let i = 0;
  while (i < indexes.length && time > indexes[i + 1]) {
    i += 1;
  }
  TelemetryTestUtils.assertKeyedHistogramValue(hist, key, indexes[i], 1);
}

add_task(async function test_11M() {
  await test_large_post(
    11 * (1 << 20),
    "HTTP3_UPLOAD_TIME_10M_100M",
    "uses_http3_10_50",
    indexes_10_100
  );
});

add_task(async function test_51M() {
  await test_large_post(
    51 * (1 << 20),
    "HTTP3_UPLOAD_TIME_10M_100M",
    "uses_http3_50_100",
    indexes_10_100
  );
});

add_task(async function test_101M() {
  await test_large_post(
    101 * (1 << 20),
    "HTTP3_UPLOAD_TIME_GT_100M",
    "uses_http3",
    indexes_gt_100
  );
});
