/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80 ft=javascript: */
"use strict";

// This script slows the load of an HTML document so that we can reliably test
// all phases of the load cycle supported by the extension API.

/* eslint-disable no-unused-vars */

const DELAY = 1 * 1000; // Delay one second before completing the request.

const Ci = Components.interfaces;

let nsTimer = Components.Constructor("@mozilla.org/timer;1", "nsITimer", "initWithCallback");

let timer;

function handleRequest(request, response) {
  response.processAsync();

  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.write(`<!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <title></title>
    </head>
    <body>
  `);

  // Note: We need to store a reference to the timer to prevent it from being
  // canceled when it's GCed.
  timer = new nsTimer(() => {
    response.write(`
        <iframe src="/"></iframe>
      </body>
      </html>`);
    response.finish();
  }, DELAY, Ci.nsITimer.TYPE_ONE_SHOT);
}
