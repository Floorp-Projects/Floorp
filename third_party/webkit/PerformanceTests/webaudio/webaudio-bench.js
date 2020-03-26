if (window.AudioContext == undefined) {
  window.AudioContext = window.webkitAudioContext;
  window.OfflineAudioContext = window.webkitOfflineAudioContext;
}

// Global samplerate at which we run the context.
var samplerate = 48000;
// Array containing at first the url of the audio resources to fetch, and the
// the actual buffers audio buffer we have at our disposal to for tests.
var sources = [];
// Array containing the results, for each benchmark.
var results = [];
// Array containing the offline contexts used to run the testcases.
var testcases = [];
// Array containing the functions that can return a runnable testcase.
var testcases_registered = [];
// Array containing the audio buffers for each benchmark
var buffers = [];
var playingSource = null;
// audiocontext used to play back the result of the benchmarks
var ac = new AudioContext();

function getFile(url, callback) {
  var request = new XMLHttpRequest();
  request.open("GET", url, true);
  request.responseType = "arraybuffer";

  request.onload = function() {
    var ctx = new AudioContext();
    ctx.decodeAudioData(request.response, function(data) {
      callback(data, undefined);
    }, function() {
      callback(undefined, "Error decoding the file " + url);
    });
  }
  request.send();
}

function recordResult(result) {
  results.push(result);
}

function benchmark(testcase, ended) {
  var context = testcase.ctx;
  var start;

  context.oncomplete = function(e) {
    var end = Date.now();
    recordResult({
      name: testcase.name,
      duration: end - start,
      buffer: e.renderedBuffer
    });
    ended();
  };

  start = Date.now();
  context.startRendering();
}

function getMonoFile() {
  return getSpecificFile({numberOfChannels: 1});
}

function getStereoFile() {
  return getSpecificFile({numberOfChannels: 2});
}

function matchIfSpecified(a, b) {
  if (b) {
    return a == b;
  }
  return true;
}

function getSpecificFile(spec) {
  for (var i = 0 ; i < sources.length; i++) {
    if (matchIfSpecified(sources[i].numberOfChannels, spec.numberOfChannels) &&
        matchIfSpecified(sources[i].samplerate, spec.samplerate)) {
      return sources[i];
    }
  }
  throw new Error("Could not find a file that matches the specs.");
}

function allDone() {
  document.getElementById("in-progress").style.display = "none";
  var result = document.getElementById("results");
  var str = "<table><thead><tr><td>Test name</td><td>Time in ms</td><td>Speedup vs. realtime</td><td>Sound</td></tr></thead>";
  var buffers_base = buffers.length;
  var product_of_durations = 1.0;

  for (var i = 0 ; i < results.length; i++) {
    var r = results[i];
    product_of_durations *= r.duration;
    str += "<tr><td>" + r.name + "</td>" +
               "<td>" + r.duration + "</td>"+
               "<td>" + Math.round((r.buffer.duration * 1000) / r.duration) + "x</td>"+
               "<td><button data-soundindex="+(buffers_base + i)+">Play</button> ("+r.buffer.duration+"s)</td>"
          +"</tr>";
    buffers[buffers_base + i] = r.buffer;
  }
  recordResult({
    name: "Geometric Mean",
    duration: Math.round(Math.pow(product_of_durations, 1.0/results.length)),
    buffer: {}
  });
  str += "</table>";
  result.innerHTML += str;
  result.addEventListener("click", function(e) {
    var t = e.target;
    if (t.dataset.soundindex != undefined) {
      if (playingSource != null) {
        playingSource.button.innerHTML = "Play";
        playingSource.onended = undefined;
        playingSource.stop(0);
        if (playingSource.button == t) {
          playingSource = null;
          return;
        }
      }
      playingSource = ac.createBufferSource();
      playingSource.connect(ac.destination);
      playingSource.buffer = buffers[t.dataset.soundindex];
      playingSource.start(0);
      playingSource.button = t;
      t.innerHTML = "Pause";
      playingSource.onended = function () {
        playingSource = null;
      }
    }
  });

  document.getElementById("run-all").disabled = false;

  if (location.search == '?raptor') {
    var _data = ['raptor-benchmark', 'webaudio', JSON.stringify(results)];
    window.postMessage(_data, '*');
  } else {
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/results", true);
    xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xhr.send("results=" + JSON.stringify(results));
  }
}

function runOne(i) {
  benchmark(testcases[i], function() {
    i++;
    if (i < testcases.length) {
      runOne(i);
    } else {
      allDone();
    }
  });
}

function runAll() {
  initAll();
  results = [];
  runOne(0);
}

function initAll() {
  for (var i = 0; i < testcases_registered.length; i++) {
    testcases[i] = {};
    testcases[i].ctx = testcases_registered[i].func();
    testcases[i].name = testcases_registered[i].name;
  }
}

function loadOne(i, endCallback) {
  getFile(sources[i], function(b) {
    sources[i] = b;
    i++;
    if (i == sources.length) {
      loadOne(i);
    } else {
      endCallback();
    }
  })
}

function loadAllSources(endCallback) {
  loadOne(0, endCallback);
}

document.addEventListener("DOMContentLoaded", function() {
  document.getElementById("run-all").addEventListener("click", function() {
    document.getElementById("in-progress").style.display = "inline";
    document.getElementById("run-all").disabled = true;
    runAll();
  });
  loadAllSources(function() {
    document.getElementById("loading").style.display = "none";
    document.getElementById("run-all").style.display = "inline";
    document.getElementById("in-progress").style.display = "inline";
    setTimeout(runAll, 100);
  });
});

/* Public API */
function registerTestCase(testCase) {
  testcases_registered.push(testCase);
}

function registerTestFile(url) {
  sources.push(url);
}
