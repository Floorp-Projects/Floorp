/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL("<div style='background-color: green'>Hello world</div>");

add_task(async function transferModes({ client }) {
  const { IO, Page } = client;
  await loadURL(DOC);

  // as base64 encoded data
  const base64 = await Page.printToPDF({ transferMode: "ReturnAsBase64" });
  is(base64.stream, null, "No stream handle is returned");
  ok(!!base64.data, "Base64 encoded data is returned");
  verifyPDF(atob(base64.data).trimEnd());

  // defaults to base64 encoded data
  const defaults = await Page.printToPDF();
  is(defaults.stream, null, "By default no stream handle is returned");
  ok(!!defaults.data, "By default base64 encoded data is returned");
  verifyPDF(atob(defaults.data).trimEnd());

  // unknown transfer modes default to base64
  const fallback = await Page.printToPDF({ transferMode: "ReturnAsFoo" });
  is(fallback.stream, null, "Unknown mode doesn't return a stream");
  ok(!!fallback.data, "Unknown mode defaults to base64 encoded data");
  verifyPDF(atob(fallback.data).trimEnd());

  // as stream handle
  const stream = await Page.printToPDF({ transferMode: "ReturnAsStream" });
  ok(!!stream.stream, "Stream handle is returned");
  is(stream.data, null, "No base64 encoded data is returned");
  let streamData = "";

  while (true) {
    const { data, base64Encoded, eof } = await IO.read({
      handle: stream.stream,
    });
    streamData += base64Encoded ? atob(data) : data;
    if (eof) {
      await IO.close({ handle: stream.stream });
      break;
    }
  }

  verifyPDF(streamData.trimEnd());
});

function verifyPDF(data) {
  is(data.slice(0, 5), "%PDF-", "Decoded data starts with the PDF signature");
  is(data.slice(-5), "%%EOF", "Decoded data ends with the EOF flag");
}
