/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { arrayBufferToBlobURL } = ChromeUtils.importESModule(
  "chrome://global/content/ml/Utils.sys.mjs"
);

/**
 * Test arrayBufferToBlobURL function.
 */
add_task(async function test_ml_utils_array_buffer_to_blob_url() {
  const buffer = new ArrayBuffer(8);
  const blobURL = arrayBufferToBlobURL(buffer);

  Assert.equal(
    typeof blobURL,
    "string",
    "arrayBufferToBlobURL should return a string"
  );
  Assert.equal(
    blobURL.startsWith("blob:"),
    true,
    "The returned string should be a Blob URL"
  );
});
