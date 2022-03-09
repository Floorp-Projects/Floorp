if (window.AudioContext == undefined) {
  window.AudioContext = window.webkitAudioContext;
  window.OfflineAudioContext = window.webkitOfflineAudioContext;
}

$ = document.querySelectorAll.bind(document);

let DURATION = null;
if (location.search) {
  let duration = location.search.match(/rendering-buffer-length=(\d+)/);
  if (duration) {
    DURATION = duration[1];
  } else {
    DURATION = 120;
  }
} else {
  DURATION = 120;
}


// Global sample rate at which we run the context.
var sampleRate = 48000;
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

function getFile(source, callback) {
  var request = new XMLHttpRequest();
  request.open("GET", source.url, true);
  request.responseType = "arraybuffer";

  request.onload = function() {
    // decode buffer at its initial sample rate
    var ctx = new OfflineAudioContext(1, 1, source.sampleRate);

    ctx.decodeAudioData(request.response, function(buffer) {
      callback(buffer, undefined);
    }, function() {
      callback(undefined, "Error decoding the file " + source.url);
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
  return getSpecificFile({ numberOfChannels: 1 });
}

function getStereoFile() {
  return getSpecificFile({ numberOfChannels: 2 });
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
        matchIfSpecified(sources[i].sampleRate, spec.sampleRate)) {
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

  if (location.search.includes("raptor")) {
    var _data = ['raptor-benchmark', 'webaudio', JSON.stringify(results)];
    window.postMessage(_data, '*');
    window.sessionStorage.setItem('benchmark_results',  JSON.stringify(_data));
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
    $("#progress-bar")[0].value++;
    if (i < testcases.length) {
      runOne(i);
    } else {
      allDone();
    }
  });
}

function runAll() {
  $("#progress-bar")[0].max = testcases_registered.length;
  $("#progress-bar")[0].value = 0;
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
  getFile(sources[i], function(buffer, err) {
    if (err) {
      throw new Error(msg);
    }

    sources[i] = buffer;
    i++;

    if (i < sources.length) {
      loadOne(i, endCallback);
    } else {
      endCallback();
    }
  });
}

function loadAllSources(endCallback) {
  loadOne(0, endCallback);
}

document.addEventListener("DOMContentLoaded", function() {
  document.getElementById("run-all").addEventListener("click", function() {
    document.getElementById("run-all").disabled = true;
    document.getElementById("in-progress").style.display = "inline";
    runAll();
  });
  loadAllSources(function() {
    // auto-run when running in raptor
    document.getElementById("loading").remove();
    document.getElementById("controls").style.display = "block";
    if (location.search.includes("raptor")) {
      document.getElementById("run-all").disabled = true;
      document.getElementById("in-progress").style.display = "inline";
      setTimeout(runAll, 100);
    }
  });
});

/* Public API */
function registerTestCase(testCase) {
  testcases_registered.push(testCase);
}

function registerTestFile(url) {
  sources.push(url);
}
