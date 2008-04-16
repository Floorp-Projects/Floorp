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
  // 2 errors, 5ms time period, max 3 requests per ten milliseconds,
  // 5ms backoff interval, 20ms max delay
  var rb = new jslib.RequestBackoff(2, 5, 3, 10, 5, 20);
  setNow(1);
  rb.noteServerResponse(200);
  do_check_true(rb.canMakeRequest());

  setNow(2);
  rb.noteServerResponse(500);
  do_check_true(rb.canMakeRequest());

  setNow(3);
  rb.noteServerResponse(200);
  do_check_true(rb.canMakeRequest());

  // Trigger backoff
  setNow(4);
  rb.noteServerResponse(502);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 9);

  // Trigger backoff again
  setNow(10);
  do_check_true(rb.canMakeRequest());
  rb.noteServerResponse(503);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 25);

  // Trigger backoff a third time and hit max timeout
  setNow(30);
  do_check_true(rb.canMakeRequest());
  rb.noteServerResponse(302);
  do_check_false(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 50);

  // Request goes through
  setNow(100);
  do_check_true(rb.canMakeRequest());
  rb.noteServerResponse(200);
  do_check_true(rb.canMakeRequest());
  do_check_eq(rb.nextRequestTime_, 0);

  // Another error (shouldn't trigger backoff)
  setNow(101);
  rb.noteServerResponse(500);
  do_check_true(rb.canMakeRequest());

  // Another error, but not in ERROR_PERIOD, so it should be ok
  setNow(107);
  rb.noteServerResponse(500);
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
