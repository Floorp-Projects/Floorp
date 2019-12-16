/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/test/browser/head.js",
  this
);

const { streamRegistry } = ChromeUtils.import(
  "chrome://remote/content/domains/parent/IO.jsm"
);

async function registerFileStream(contents, options = {}) {
  // Any file as registered with the stream registry will be automatically
  // deleted during the shutdown of Firefox.
  options.remove = false;

  const { file, path } = await createFile(contents, options);
  const handle = streamRegistry.add(file);

  return { handle, path };
}
