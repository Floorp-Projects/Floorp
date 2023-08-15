/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ShoppingProduct } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ShoppingProduct.mjs"
);
const IMAGE_URL = "http://example.com/api/image.jpg";

add_task(async function test_product_requestImageBlob() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  let img = await product.requestImageBlob(IMAGE_URL);

  Assert.ok(Blob.isInstance(img), "Image is loaded and returned as a blob");

  enableOHTTP();
  img = await product.requestImageBlob(IMAGE_URL);
  disableOHTTP();

  Assert.ok(Blob.isInstance(img), "Image is loaded and returned as a blob");
});
