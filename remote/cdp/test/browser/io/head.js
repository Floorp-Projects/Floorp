/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/cdp/test/browser/head.js",
  this
);

const { streamRegistry } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/domains/parent/IO.sys.mjs"
);

async function registerFileStream(contents, options) {
  const stream = await createFileStream(contents, options);
  const handle = streamRegistry.add(stream);

  return { handle, stream };
}
