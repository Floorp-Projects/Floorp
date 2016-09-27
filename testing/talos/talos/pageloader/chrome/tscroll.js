// Note: This file is used at both tscrollx and tp5o_scroll. With the former as
//       unprivileged code.
// - Please make sure that any changes apply cleanly to all use cases.

function testScroll(target, stepSize, opt_reportFunc, opt_numSteps)
{
  var win;
  if (target == "content") {
    target = content.wrappedJSObject;
    win = content;
  } else {
    win = window;
  }

  var report;
  /**
   * Sets up the value of 'report' as a function for reporting the test result[s].
   * Chooses between the "usual" tpRecordTime which the pageloader addon injects
   * to pages, or a custom function in case we're a framescript which pageloader
   * added to the tested page, or a debug tpRecordTime from talos-debug.js if
   * running in a plain browser.
   *
   * @returns Promise
   */
  function P_setupReportFn() {
    return new Promise(function(resolve) {
      report = opt_reportFunc || win.tpRecordTime;
      if (report == 'PageLoader:RecordTime') {
        report = function(duration, start, name) {
          var msg = { time: duration, startTime: start, testName: name };
          sendAsyncMessage('PageLoader:RecordTime', msg);
        }
        resolve();
        return;
      }

      // Not part of the test and does nothing if we're within talos.
      // Provides an alternative tpRecordTime (with some stats display) if running in a browser.
      if (!report && document.head) {
        var imported = document.createElement('script');
        imported.addEventListener("load", function() {
          report = tpRecordTime;
          resolve();
        });

        imported.src = '../../scripts/talos-debug.js?dummy=' + Date.now(); // For some browsers to re-read
        document.head.appendChild(imported);
        return;
      }

      resolve();
    });
  }

  function FP_wait(ms) {
    return function() {
      return new Promise(function(resolve) {
        setTimeout(resolve, ms);
      });
    };
  }

  function rAF(fn) {
    return content.requestAnimationFrame(fn);
  }

  function P_rAF() {
    return new Promise(function(resolve) {
      rAF(resolve);
    });
  }

  function myNow() {
    return (win.performance && win.performance.now) ?
            win.performance.now() :
            Date.now();
  };

  var isWindow = target.self === target;

  var getPos =       isWindow ? function() { return target.pageYOffset; }
                              : function() { return target.scrollTop; };

  var gotoTop =      isWindow ? function() { target.scroll(0, 0);  ensureScroll(); }
                              : function() { target.scrollTop = 0; ensureScroll(); };

  var doScrollTick = isWindow ? function() { target.scrollBy(0, stepSize); ensureScroll(); }
                              : function() { target.scrollTop += stepSize; ensureScroll(); };

  function ensureScroll() { // Ensure scroll by reading computed values. screenY is for X11.
    if (!this.dummyEnsureScroll) {
      this.dummyEnsureScroll = 1;
    }
    this.dummyEnsureScroll += win.screenY + getPos();
  }

  // For reference, rAF should fire on vsync, but Gecko currently doesn't use vsync.
  // Instead, it uses 1000/layout.frame_rate
  // (with 60 as default value when layout.frame_rate == -1).
  function P_syncScrollTest() {
    return new Promise(function(resolve) {
      // We should be at the top of the page now.
      var start = myNow();
      var lastScrollPos = getPos();
      var lastScrollTime = start;
      var durations = [];

      function tick() {
        var now = myNow();
        var duration = now - lastScrollTime;
        lastScrollTime = now;

        durations.push(duration);
        doScrollTick();

        /* stop scrolling if we can't scroll more, or if we've reached requested number of steps */
        if ((getPos() == lastScrollPos) || (opt_numSteps && (durations.length >= (opt_numSteps + 2)))) {
          if (typeof(Profiler) !== "undefined") {
            Profiler.pause();
          }

          // Note: The first (1-5) intervals WILL be longer than the rest.
          // First interval might include initial rendering and be extra slow.
          // Also requestAnimationFrame needs to sync (optimally in 1 frame) after long frames.
          // Suggested: Ignore the first 5 intervals.

          durations.pop(); // Last step was 0.
          durations.pop(); // and the prev one was shorter and with end-of-page logic, ignore both.

          if (win.talosDebug)
            win.talosDebug.displayData = true; // In a browser: also display all data points.

          // For analysis (otherwise, it's too many data points for talos):
          var sum = 0;
          for (var i = 0; i < durations.length; i++)
            sum += Number(durations[i]);

          // Report average interval or (failsafe) 0 if no intervls were recorded
          resolve(durations.length ? sum / durations.length : 0);
          return;
        }

        lastScrollPos = getPos();
        rAF(tick);
      }

      if (typeof(Profiler) !== "undefined") {
        Profiler.resume();
      }
      rAF(tick);
    });
  }

  P_setupReportFn()
  .then(FP_wait(260))
  .then(gotoTop)
  .then(P_rAF)
  .then(P_syncScrollTest)
  .then(function(result) { // function since 'report' might not be ready when invoking 'then'
    report(result);
  });
}

// This code below here is unique to tscroll.js inside of pageloader
try {
  function handleMessageFromChrome(message) {
    var payload = message.data.details;
    testScroll(payload.target, payload.stepSize, 'PageLoader:RecordTime', payload.opt_numSteps);
  }

  addMessageListener("PageLoader:ScrollTest", handleMessageFromChrome);
} catch (e) {}
