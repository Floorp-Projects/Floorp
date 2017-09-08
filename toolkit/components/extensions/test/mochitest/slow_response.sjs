/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80 ft=javascript: */
"use strict";

/* eslint-disable no-unused-vars */

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/AppConstants.jsm");

const DELAY = AppConstants.DEBUG ? 4000 : 800;

let nsTimer = Components.Constructor("@mozilla.org/timer;1", "nsITimer", "initWithCallback");

let timer;
function delay() {
  return new Promise(resolve => {
    timer = nsTimer(resolve, DELAY, Ci.nsITimer.TYPE_ONE_SHOT);
  });
}

const PARTS = [
  `<!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <title></title>
    </head>
    <body>`,
  "Lorem ipsum dolor sit amet, <br>",
  "consectetur adipiscing elit, <br>",
  "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. <br>",
  "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. <br>",
  "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. <br>",
  "Excepteur sint occaecat cupidatat non proident, <br>",
  "sunt in culpa qui officia deserunt mollit anim id est laborum.<br>",
  `
    </body>
    </html>`,
];

async function handleRequest(request, response) {
  response.processAsync();

  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);

  await delay();

  for (let part of PARTS) {
    response.write(`${part}\n`);
    await delay();
  }

  response.finish();
}

