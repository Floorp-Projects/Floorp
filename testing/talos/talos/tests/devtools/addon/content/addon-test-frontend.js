// This file is the common bits for the test runner frontend, originally
// extracted out of the tart.html frontend when creating the damp test.

/* globals updateConfig, defaultConfig, config */ /* from damp.html */

function $(id) {
  return document.getElementById(id);
}

// Executes command at the chrome process.
// Limited to one argument (data), which is enough for TART.
// doneCallback will be called once done and, if applicable, with the result as argument.
// Execution might finish quickly (e.g. when setting prefs) or
// take a while (e.g. when triggering the test run)
function chromeExec(commandName, data, doneCallback) {
  // dispatch an event to the framescript which will take it from there.
  doneCallback = doneCallback || function dummy() {};
  dispatchEvent(
    new CustomEvent("damp@mozilla.org:chrome-exec-event", {
      bubbles: true,
      detail: {
        command: {
          name: commandName,
          data,
        },
        doneCallback
      }
    })
  );
}

function toClipboard(text) {
  chromeExec("toClipboard", text);
}

function runTest(config, doneCallback) {
  chromeExec("runTest", config, doneCallback);
}

function sum(values) {
  return values.reduce(function(a, b) { return a + b; });
}

function average(values) {
  return values.length ? sum(values) / values.length : 999999999;
}

function stddev(values, avg) {
  if (undefined == avg) avg = average(values);
  if (values.length <= 1) return 0;

  return Math.sqrt(
    values.map(function(v) { return Math.pow(v - avg, 2); })
          .reduce(function(a, b) { return a + b; }) / (values.length - 1));
}

var lastResults = '["[no results collected]"]';

function doneTest(dispResult) {
  $("hide-during-run").style.display = "block";
  $("show-during-run").style.display = "none";
  if (dispResult) {
    // Array of test results, each element has .name and .value (test name and test result).
    // Test result may also be an array of numeric values (all the intervals)

    lastResults = JSON.stringify(dispResult); // for "Copy to clipboard" button

    var stats = {}; // Used for average, stddev when repeat!=1
    var isRepeat = false;

    for (var i in dispResult) {
      var di = dispResult[i];
      var disp = [].concat(di.value).map(function(a) { return " " + (isNaN(a) ? -1 : a.toFixed(1)); }).join("&nbsp;&nbsp;");
      dispResult[i] = String(di.name) + ": " + disp;
      if (di.name.indexOf(".half") >= 0 || di.name.indexOf(".all") >= 0)
        dispResult[i] = "<b>" + dispResult[i] + "</b>";
      if (di.name.indexOf(".raw") >= 0)
        dispResult[i] = "<br/>" + dispResult[i]; // Add space before raw results (which are the first result of an animation)

      // stats:
      if (di.name.indexOf(".raw") < 0) {
        if (!stats[di.name]) {
          stats[di.name] = [];
        } else {
          isRepeat = true;
        }

        stats[di.name].push(di.value);
      }
    }

    var dispStats = "";
    if (isRepeat) {
      dispStats = "<hr/><b>Aggregated</b>:<br/>";
      for (var s in stats) {
        if (s.indexOf(".half") >= 0 )
          dispStats += "<br/>";
        dispStats += s + "&nbsp;&nbsp;&nbsp;&nbsp;Average (" + stats[s].length + "): " + average(stats[s]).toFixed(2) + " stddev: " + stddev(stats[s]).toFixed(2) + "<br/>";
      }

      dispStats += "<hr/><b>Individual animations</b>:<br/>";
    }
    $("run-results").innerHTML = "<hr/><br/>Results <button onclick='toClipboard(lastResults)'>[ Copy to clipboard as JSON ]</button>:<br/>" + dispStats + dispResult.join("<br/>");
  }
}

function triggerStart() {
  updateConfig();
  $("hide-during-run").style.display = "none";
  $("show-during-run").style.display = "block";
  $("run-results").innerHTML = "";

  runTest(config, doneTest);
}

function deselectAll() {
  for (var test in defaultConfig.subtests) {
    $("subtest-" + test).checked = false;
  }
}

// E.g. returns "world" for key "hello", "2014" for key "year", and "" for key "dummy":
// http://localhost/x.html#hello=world&x=12&year=2014
function getUriHashValue(key) {
  var k = String(key) + "=";
  var uriVars = unescape(document.location.hash).substr(1).split("&");
  for (var i in uriVars) {
    if (uriVars[i].indexOf(k) == 0)
      return uriVars[i].substr(k.length);
  }
  return "";
}

// URL e.g. chrome://devtools/content/devtools.html#auto&tests=["simple","iconFadeDpiCurrent"]
// Note - there's no error checking for arguments parsing errors.
//        Any errors will express as either javascript errors or not reading the args correctly.
//        This is not an "official" part of the UI, and when used in talos, will fail early
//        enough to not cause "weird" issues too late.
function updateOptionsFromUrl() {
  var uriTests = getUriHashValue("tests");
  var tests = uriTests ? JSON.parse(uriTests) : [];

  if (tests.length) {
    for (var d in defaultConfig.subtests) {
      $("subtest-" + d).checked = false;
      for (var t in tests) {
        if (tests[t] == d) {
          $("subtest-" + d).checked = true;
        }
      }
    }
  }
}

function init() {
  updateOptionsFromUrl();
  if (document.location.hash.indexOf("#auto") == 0) {
    triggerStart();
  }
}

addEventListener("load", init);
