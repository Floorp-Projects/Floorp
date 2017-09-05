// Some unittests (e.g., paste into JS shell)
var jslib = Cc["@mozilla.org/url-classifier/jslib;1"].
            getService().wrappedJSObject;
var _Datenow = jslib.Date.now;
function setNow(time) {
  jslib.Date.now = function() {
    return time;
  }
}

function run_test() {
  // 3 errors, 1ms retry period, max 3 requests per ten milliseconds,
  // 5ms backoff interval, 19ms max delay
  var rb = new jslib.RequestBackoff(3, 1, 3, 10, 5, 19);
  setNow(1);
  rb.noteServerResponse(200);
  do_check_true(rb.canMakeRequest());
  setNow(2);
  do_check_true(rb.canMakeRequest());

  // First error should trigger a 1ms delay
  rb.noteServerResponse(500);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 3);
  setNow(3);
  do_check_true(rb.canMakeRequest());

  // Second error should also trigger a 1ms delay
  rb.noteServerResponse(500);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 4);
  setNow(4);
  do_check_true(rb.canMakeRequest());

  // Third error should trigger a 5ms backoff
  rb.noteServerResponse(500);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 9);
  setNow(9);
  do_check_true(rb.canMakeRequest());

  // Trigger backoff again
  rb.noteServerResponse(503);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 19);
  setNow(19);
  do_check_true(rb.canMakeRequest());

  // Trigger backoff a third time and hit max timeout
  rb.noteServerResponse(302);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 38);
  setNow(38);
  do_check_true(rb.canMakeRequest());

  // One more backoff, should still be at the max timeout
  rb.noteServerResponse(400);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 57);
  setNow(57);
  do_check_true(rb.canMakeRequest());

  // Request goes through
  rb.noteServerResponse(200);
  do_check_true(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 0);
  setNow(58);
  rb.noteServerResponse(500);

  // Another error, should trigger a 1ms backoff
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 59);

  setNow(59);
  do_check_true(rb.canMakeRequest());

  setNow(200);
  rb.noteRequest();
  setNow(201);
  rb.noteRequest();
  setNow(202);
  do_check_true(rb.canMakeRequest());
  rb.noteRequest();
  do_check_false(rb.canMakeRequest());
  setNow(211);
  do_check_true(rb.canMakeRequest());

  jslib.Date.now = _Datenow;
}
