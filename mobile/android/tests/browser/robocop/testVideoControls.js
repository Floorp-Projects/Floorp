// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SimpleServiceDiscovery.jsm");

// The chrome window
var chromeWin;

// Track the <browser> where the tests are happening
var browser;

// The document of the video_controls web content
var contentDocument;

// The <video> we will be testing
var video;

add_test(function setup_browser() {
  chromeWin = Services.wm.getMostRecentWindow("navigator:browser");
  let BrowserApp = chromeWin.BrowserApp;

  do_register_cleanup(function cleanup() {
    BrowserApp.closeTab(BrowserApp.getTabForBrowser(browser));
  });

  // Load our test web page with <video> elements
  let url = "http://mochi.test:8888/tests/robocop/video_controls.html";
  browser = BrowserApp.addTab(url, { selected: true, parentId: BrowserApp.selectedTab.id }).browser;
  browser.addEventListener("load", function(event) {
    contentDocument = browser.contentDocument;

    video = contentDocument.getElementById("video");
    ok(video, "Found the video element");

    Services.tm.mainThread.dispatch(run_next_test, Ci.nsIThread.DISPATCH_NORMAL);
  }, {capture: true, once: true});
});

add_test(function test_webm() {
  // Load the test video
  video.src = "http://mochi.test:8888/tests/robocop/video-pattern.webm";

  Services.tm.mainThread.dispatch(testLoad, Ci.nsIThread.DISPATCH_NORMAL);
});

add_test(function test_ogg() {
  // Load the test video
  video.src = "http://mochi.test:8888/tests/robocop/video-pattern.ogg";

  Services.tm.mainThread.dispatch(testLoad, Ci.nsIThread.DISPATCH_NORMAL);
});

function getButtonByAttribute(aName, aValue) {
  let domUtil = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
  let kids = domUtil.getChildrenForNode(video, true);
  let videocontrols = kids[1];
  return contentDocument.getAnonymousElementByAttribute(videocontrols, aName, aValue);
}

function getPixelColor(aCanvas, aX, aY) {
  let cx = aCanvas.getContext("2d");
  let pixel = cx.getImageData(aX, aY, 1, 1);
  return {
    r: pixel.data[0],
    g: pixel.data[1],
    b: pixel.data[2],
    a: pixel.data[3]
  };
}

function testLoad() {
  // The video is not auto-play, so it starts paused
  let playButton = getButtonByAttribute("class", "playButton");
  ok(playButton.getAttribute("paused") == "true", "Play button is paused");

  // Let's start playing it
  video.play();
  video.addEventListener("play", testPlay);
}

function testPlay(aEvent) {
  video.removeEventListener("play", testPlay);
  let playButton = getButtonByAttribute("class", "playButton");
  ok(playButton.hasAttribute("paused") == false, "Play button is not paused");

  // Let the video play for 2 seconds, then pause it
  chromeWin.setTimeout(function() {
    video.pause();
    video.addEventListener("pause", testPause);
  }, 2000);
}

function testPause(aEvent) {
  video.removeEventListener("pause", testPause);

  // If we got here, the play button should be paused
  let playButton = getButtonByAttribute("class", "playButton");
  ok(playButton.getAttribute("paused") == "true", "Play button is paused again");

  // Let's grab an image of the frame and test it
  let width = 640;
  let height = 480;
  let canvas = contentDocument.getElementById("canvas");
  canvas.width = width;
  canvas.height = height;
  canvas.getContext("2d").drawImage(video, 0, 0, width, height);

  // Let's grab some pixel colors to verify we actually displayed a video.
  // For some reason the canvas copy of the frame does not recreate the colors
  // exactly for some devices. To keep things passing on automation and local
  // runs, we fudge it.

  // The purpose of this code is not to test drawImage, but whether a video
  // frame was displayed.
  const MAX_COLOR = 235; // ideally, 255
  const MIN_COLOR = 20; // ideally, 0

  let bar1 = getPixelColor(canvas, 45, 10);
  do_print("Color at (45, 10): " + JSON.stringify(bar1));
  ok(bar1.r >= MAX_COLOR && bar1.g >= MAX_COLOR && bar1.b >= MAX_COLOR, "Bar 1 is white");

  let bar2 = getPixelColor(canvas, 135, 10);
  do_print("Color at (135, 10): " + JSON.stringify(bar2));
  ok(bar2.r >= MAX_COLOR && bar2.g >= MAX_COLOR && bar2.b <= MIN_COLOR, "Bar 2 is yellow");

  let bar3 = getPixelColor(canvas, 225, 10);
  do_print("Color at (225, 10): " + JSON.stringify(bar3));
  ok(bar3.r <= MIN_COLOR && bar3.g >= MAX_COLOR && bar3.b >= MAX_COLOR, "Bar 3 is Cyan");

  let bar4 = getPixelColor(canvas, 315, 10);
  do_print("Color at (315, 10): " + JSON.stringify(bar4));
  ok(bar4.r <= MIN_COLOR && bar4.g >= MAX_COLOR && bar4.b <= MIN_COLOR, "Bar 4 is Green");

  let bar5 = getPixelColor(canvas, 405, 10);
  do_print("Color at (405, 10): " + JSON.stringify(bar5));
  ok(bar5.r >= MAX_COLOR && bar5.g <= MIN_COLOR && bar5.b >= MAX_COLOR, "Bar 5 is Purple");

  let bar6 = getPixelColor(canvas, 495, 10);
  do_print("Color at (495, 10): " + JSON.stringify(bar6));
  ok(bar6.r >= MAX_COLOR && bar6.g <= MIN_COLOR && bar6.b <= MIN_COLOR, "Bar 6 is Red");

  let bar7 = getPixelColor(canvas, 585, 10);
  do_print("Color at (585, 10): " + JSON.stringify(bar7));
  ok(bar7.r <= MIN_COLOR && bar7.g <= MIN_COLOR && bar7.b >= MAX_COLOR, "Bar 7 is Blue");

  run_next_test();
}

run_next_test();
