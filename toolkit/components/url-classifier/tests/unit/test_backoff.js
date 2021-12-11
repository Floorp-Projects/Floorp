// Some unittests (e.g., paste into JS shell)
var jslib = Cc["@mozilla.org/url-classifier/jslib;1"].getService()
  .wrappedJSObject;

var jslibDate = Cu.getGlobalForObject(jslib).Date;

var _Datenow = jslibDate.now;
function setNow(time) {
  jslibDate.now = function() {
    return time;
  };
}

function run_test() {
  // 3 errors, 1ms retry period, max 3 requests per ten milliseconds,
  // 5ms backoff interval, 19ms max delay
  var rb = new jslib.RequestBackoff(3, 1, 3, 10, 5, 19, 0);
  setNow(1);
  rb.noteServerResponse(200);
  Assert.ok(rb.canMakeRequest());
  setNow(2);
  Assert.ok(rb.canMakeRequest());

  // First error should trigger a 1ms delay
  rb.noteServerResponse(500);
  Assert.ok(!rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 3);
  setNow(3);
  Assert.ok(rb.canMakeRequest());

  // Second error should also trigger a 1ms delay
  rb.noteServerResponse(500);
  Assert.ok(!rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 4);
  setNow(4);
  Assert.ok(rb.canMakeRequest());

  // Third error should trigger a 5ms backoff
  rb.noteServerResponse(500);
  Assert.ok(!rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 9);
  setNow(9);
  Assert.ok(rb.canMakeRequest());

  // Trigger backoff again
  rb.noteServerResponse(503);
  Assert.ok(!rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 19);
  setNow(19);
  Assert.ok(rb.canMakeRequest());

  // Trigger backoff a third time and hit max timeout
  rb.noteServerResponse(302);
  Assert.ok(!rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 38);
  setNow(38);
  Assert.ok(rb.canMakeRequest());

  // One more backoff, should still be at the max timeout
  rb.noteServerResponse(400);
  Assert.ok(!rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 57);
  setNow(57);
  Assert.ok(rb.canMakeRequest());

  // Request goes through
  rb.noteServerResponse(200);
  Assert.ok(rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 0);
  setNow(58);
  rb.noteServerResponse(500);

  // Another error, should trigger a 1ms backoff
  Assert.ok(!rb.canMakeRequest());
  Assert.equal(rb.nextRequestTime_, 59);

  setNow(59);
  Assert.ok(rb.canMakeRequest());

  setNow(200);
  rb.noteRequest();
  setNow(201);
  rb.noteRequest();
  setNow(202);
  Assert.ok(rb.canMakeRequest());
  rb.noteRequest();
  Assert.ok(!rb.canMakeRequest());
  setNow(211);
  Assert.ok(rb.canMakeRequest());

  jslibDate.now = _Datenow;
}
