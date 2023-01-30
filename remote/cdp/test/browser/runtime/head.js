/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/cdp/test/browser/head.js",
  this
);

const BASE_ORIGIN = "https://example.com";
const BASE_PATH = `${BASE_ORIGIN}/browser/remote/cdp/test/browser/runtime`;
const FRAMESET_SINGLE_URL = `${BASE_PATH}/doc_frameset_single.html`;
const PAGE_FRAME_URL = `${BASE_PATH}/doc_frame.html`;
const PAGE_URL = `${BASE_PATH}/doc_empty.html`;
