var perf_data = {
  start: null,
  end: null,
};

function build_dom(n, elemName, options) {
  // By default we use different elements in the DOM to defeat the style sharing
  // cache, otherwise this sythetic DOM is trivially stylable by engines with that
  // optimization.
  options = options || {};
  var elemNameLeft = options.elemNameLeft || "div";
  var elemNameRight = options.elemNameRight || "span";

  var ours = document.createElement(elemName);
  for (var attr in options.attributes) {
    ours.setAttribute(attr, options.attributes[attr]);
  }

  if (n != 1) {
    var leftSize = Math.floor(n / 2);
    var rightSize = Math.floor((n - 1) / 2);
    ours.appendChild(build_dom(leftSize, elemNameLeft, options));
    if (rightSize > 0)
      ours.appendChild(build_dom(rightSize, elemNameRight, options));
  }
  return ours;
}

function build_rule(selector, selectorRepeat, declaration, ruleRepeat) {
  ruleRepeat = ruleRepeat || 1;
  var s = document.createElement("style");
  var rule = Array(selectorRepeat).fill(selector).join(", ") + declaration;
  s.textContent = Array(ruleRepeat).fill(rule).join("\n\n");
  return s;
}

function build_text(word, wordRepeat, paraRepeat) {
  wordRepeat = wordRepeat || 1;
  paraRepeat = paraRepeat || 1;
  let para = Array(wordRepeat).fill(word).join(" ");
  return Array(paraRepeat).fill(para).join("\n");
}

function flush_style(element) {
  getComputedStyle(element || document.documentElement).color;
}

function flush_layout(element) {
  (element || document.documentElement).offsetHeight;
}

function perf_start() {
  if (perf_data.start !== null) {
    throw new Error("already started timing!");
  }

  perf_data.start = performance.now();
}

function perf_finish() {
  var end = performance.now();

  if (perf_data.start === null) {
    throw new Error("haven't started timing!");
  }

  if (perf_data.end !== null) {
    throw new Error("already finished timing!");
  }

  var start = perf_data.start;
  perf_data.end = end;

  // when running in talos report results; when running outside talos just alert
  if (window.tpRecordTime) {
    // Running in talos.
    window.tpRecordTime(end - start, start);
  } else if (window.parent && window.parent.report_perf_reftest_time) {
    // Running in the perf-reftest runner.
    window.parent.report_perf_reftest_time({ type: "time", value: end - start });
  } else {
    // Running standalone; just alert.
    console.log(end);
    console.log(start);
    alert("Result: " + (end - start).toFixed(2) + " (ms)");
  }
}

if (window.parent.report_perf_reftest_time) {
  window.addEventListener("error", function(e) {
    window.parent.report_perf_reftest_time({ type: "error", value: e.message });
  });
}
