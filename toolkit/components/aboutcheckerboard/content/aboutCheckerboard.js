/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var trace;
var service;
var reports;

function onLoad() {
  trace = document.getElementById('trace');
  service = new CheckerboardReportService();
  updateEnabled();
  reports = service.getReports();
  for (var i = 0; i < reports.length; i++) {
    let text = "Severity " + reports[i].severity + " at " + new Date(reports[i].timestamp).toString();
    let link = document.createElement('a');
    link.href = 'javascript:showReport(' + i + ')';
    link.textContent = text;
    let bullet = document.createElement('li');
    bullet.appendChild(link);
    document.getElementById(reports[i].reason).appendChild(bullet);
  }
}

function updateEnabled() {
  let enabled = document.getElementById('enabled');
  if (service.isRecordingEnabled()) {
    enabled.textContent = 'enabled';
    enabled.style.color = 'green';
  } else {
    enabled.textContent = 'disabled';
    enabled.style.color = 'red';
  }
}

function toggleEnabled() {
  service.setRecordingEnabled(!service.isRecordingEnabled());
  updateEnabled();
}

function flushReports() {
  service.flushActiveReports();
}

function showReport(index) {
  trace.value = reports[index].log;
  loadData();
}

// -- Code to load and render the trace --

const CANVAS_USE_RATIO = 0.75;
const FRAME_INTERVAL_MS = 50;
const VECTOR_NORMALIZED_MAGNITUDE = 30.0;

var renderData = new Array();
var currentFrame = 0;
var playing = false;
var timerId = 0;

var minX = undefined;
var minY = undefined;
var maxX = undefined;
var maxY = undefined;

function log(x) {
    if (console) {
        console.log(x);
    }
}

function getFlag(flag) {
    return document.getElementById(flag).checked;
}

// parses the lines in the textarea, ignoring anything that doesn't have RENDERTRACE.
// for each matching line, tokenizes on whitespace and ignores all tokens prior to
// RENDERTRACE. Additional info can be included at the end of the line, and will be
// displayed but not parsed. Allowed syntaxes:
//   <junk> RENDERTRACE <timestamp> rect <color> <x> <y> <width> <height> [extraInfo]
function loadData() {
    stopPlay();
    renderData = new Array();
    currentFrame = 0;
    minX = undefined;
    minY = undefined;
    maxX = undefined;
    maxY = undefined;

    var charPos = 0;
    var lastLineLength = 0;
    var lines = trace.value.split(/\r|\n/);
    for (var i = 0; i < lines.length; i++) {
        charPos += lastLineLength;
        lastLineLength = lines[i].length + 1;
        // skip lines without RENDERTRACE
        if (!/RENDERTRACE/.test(lines[i])) {
            continue;
        }

        var tokens = lines[i].split(/\s+/);
        var j = 0;
        // skip tokens until RENDERTRACE
        while (j < tokens.length && tokens[j++] != "RENDERTRACE"); // empty loop body
        if (j >= tokens.length - 2) {
            log("Error parsing line: " + lines[i]);
            continue;
        }

        var timestamp = tokens[j++];
        var destIndex = renderData.length;
        if (destIndex == 0) {
            // create the initial frame
            renderData.push({
                timestamp,
                rects: {},
            });
        } else if (renderData[destIndex - 1].timestamp == timestamp) {
            // timestamp hasn't changed use, so update the previous object
            destIndex--;
        } else {
            // clone a new copy of the last frame and update timestamp
            renderData.push(JSON.parse(JSON.stringify(renderData[destIndex - 1])));
            renderData[destIndex].timestamp = timestamp;
        }

        switch (tokens[j++]) {
            case "rect":
                if (j > tokens.length - 5) {
                    log("Error parsing line: " + lines[i]);
                    continue;
                }

                var rect = {};
                var color = tokens[j++];
                renderData[destIndex].rects[color] = rect;
                rect.x = parseFloat(tokens[j++]);
                rect.y = parseFloat(tokens[j++]);
                rect.width = parseFloat(tokens[j++]);
                rect.height = parseFloat(tokens[j++]);
                rect.dataText = trace.value.substring(charPos, charPos + lines[i].length);

                if (!getFlag('excludePageFromZoom') || color != 'brown') {
                    if (typeof minX == "undefined") {
                        minX = rect.x;
                        minY = rect.y;
                        maxX = rect.x + rect.width;
                        maxY = rect.y + rect.height;
                    } else {
                        minX = Math.min(minX, rect.x);
                        minY = Math.min(minY, rect.y);
                        maxX = Math.max(maxX, rect.x + rect.width);
                        maxY = Math.max(maxY, rect.y + rect.height);
                    }
                }
                break;

            default:
                log("Error parsing line " + lines[i]);
                break;
        }
    }

    if (!renderFrame()) {
        alert("No data found; nothing to render!");
    }
}

// render the current frame (i.e. renderData[currentFrame])
// returns false if currentFrame is out of bounds, true otherwise
function renderFrame() {
    var frame = currentFrame;
    if (frame < 0 || frame >= renderData.length) {
        log("Invalid frame index");
        return false;
    }

    var canvas = document.getElementById('canvas');
    if (!canvas.getContext) {
        log("No canvas context");
    }

    var context = canvas.getContext('2d');

    // midpoint of the bounding box
    var midX = (minX + maxX) / 2.0;
    var midY = (minY + maxY) / 2.0;

    // midpoint of the canvas
    var cmx = canvas.width / 2.0;
    var cmy = canvas.height / 2.0;

    // scale factor
    var scale = CANVAS_USE_RATIO * Math.min(canvas.width / (maxX - minX), canvas.height / (maxY - minY));

    function projectX(value) {
        return cmx + ((value - midX) * scale);
    }

    function projectY(value) {
        return cmy + ((value - midY) * scale);
    }

    function drawRect(color, rect) {
        context.strokeStyle = color;
        context.strokeRect(
            projectX(rect.x),
            projectY(rect.y),
            rect.width * scale,
            rect.height * scale);
    }

    // clear canvas
    context.fillStyle = 'white';
    context.fillRect(0, 0, canvas.width, canvas.height);
    var activeData = '';
    // draw rects
    for (var i in renderData[frame].rects) {
        drawRect(i, renderData[frame].rects[i]);
        activeData += "\n" + renderData[frame].rects[i].dataText;
    }
    // draw timestamp and frame counter
    context.fillStyle = 'black';
    context.fillText((frame + 1) + "/" + renderData.length + ": " + renderData[frame].timestamp, 5, 15);

    document.getElementById('active').textContent = activeData;

    return true;
}

// -- Player controls --

function reset(beginning) {
    currentFrame = (beginning ? 0 : renderData.length - 1);
    renderFrame();
}

function step(backwards) {
    if (playing) {
        togglePlay();
    }
    currentFrame += (backwards ? -1 : 1);
    if (!renderFrame()) {
        currentFrame -= (backwards ? -1 : 1);
    }
}

function pause() {
    clearInterval(timerId);
    playing = false;
}

function togglePlay() {
    if (playing) {
        pause();
    } else {
        timerId = setInterval(function() {
            currentFrame++;
            if (!renderFrame()) {
                currentFrame--;
                togglePlay();
            }
        }, FRAME_INTERVAL_MS);
        playing = true;
    }
}

function stopPlay() {
    if (playing) {
        togglePlay();
    }
    currentFrame = 0;
    renderFrame();
}
