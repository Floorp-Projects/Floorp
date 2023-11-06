/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ShoppingProduct } = ChromeUtils.importESModule(
  "chrome://global/content/shopping/ShoppingProduct.mjs"
);
const IMAGE_URL = "http://example.com/api/image.jpg";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/api/", do_get_file("/data"));

function BinaryHttpResponse(status, headerNames, headerValues, content) {
  this.status = status;
  this.headerNames = headerNames;
  this.headerValues = headerValues;
  this.content = content;
}

BinaryHttpResponse.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIBinaryHttpResponse"]),
};

let ohttp = Cc["@mozilla.org/network/oblivious-http;1"].getService(
  Ci.nsIObliviousHttp
);
let ohttpServer = ohttp.server();

server.registerPathHandler(
  new URL(API_OHTTP_CONFIG).pathname,
  (request, response) => {
    let bstream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    bstream.setOutputStream(response.bodyOutputStream);
    bstream.writeByteArray(ohttpServer.encodedConfig);
  }
);

let gExpectedOHTTPMethod = "GET";
server.registerPathHandler(
  new URL(API_OHTTP_RELAY).pathname,
  async (request, response) => {
    let inputStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
      Ci.nsIBinaryInputStream
    );
    inputStream.setInputStream(request.bodyInputStream);
    let requestBody = inputStream.readByteArray(inputStream.available());
    let ohttpRequest = ohttpServer.decapsulate(requestBody);
    let bhttp = Cc["@mozilla.org/network/binary-http;1"].getService(
      Ci.nsIBinaryHttp
    );
    let decodedRequest = bhttp.decodeRequest(ohttpRequest.request);
    Assert.equal(
      decodedRequest.method,
      gExpectedOHTTPMethod,
      "Should get expected HTTP method"
    );
    Assert.deepEqual(decodedRequest.headerNames.sort(), [
      "Accept",
      "Content-Type",
    ]);
    Assert.deepEqual(decodedRequest.headerValues, ["image/jpeg", "image/jpeg"]);

    response.processAsync();
    let innerResponse = await fetch("http://example.com" + decodedRequest.path);
    let bytes = new Uint8Array(await innerResponse.arrayBuffer());
    let binaryResponse = new BinaryHttpResponse(
      innerResponse.status,
      ["Content-Type"],
      ["image/jpeg"],
      bytes
    );
    let encResponse = ohttpRequest.encapsulate(
      bhttp.encodeResponse(binaryResponse)
    );
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "message/ohttp-res", false);

    let bstream = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
      Ci.nsIBinaryOutputStream
    );
    bstream.setOutputStream(response.bodyOutputStream);
    bstream.writeByteArray(encResponse);
    response.finish();
  }
);

add_task(async function test_product_requestImageBlob() {
  let uri = new URL("https://www.walmart.com/ip/926485654");
  let product = new ShoppingProduct(uri, { allowValidationFailure: false });

  Assert.ok(product.isProduct(), "Should recognize a valid product.");

  let img = await ShoppingProduct.requestImageBlob(IMAGE_URL);

  Assert.ok(Blob.isInstance(img), "Image is loaded and returned as a blob");

  enableOHTTP();
  img = await ShoppingProduct.requestImageBlob(IMAGE_URL);
  disableOHTTP();

  Assert.ok(Blob.isInstance(img), "Image is loaded and returned as a blob");
});
