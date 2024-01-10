const PDF_FILE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "file_pdf.pdf";

add_task(async function canvas_placeholder_pdfjs() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PDF_FILE);

  const data = await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    function extractCanvasData() {
      const canvas = document.createElement("canvas");
      canvas.width = 10;
      canvas.height = 10;

      const context = canvas.getContext("2d");

      context.fillStyle = "#332211";
      context.fillRect(0, 0, 10, 10);

      // Add the canvas element to the document
      document.body.appendChild(canvas);

      return context.getImageData(0, 0, 10, 10).data;
    }

    return content.eval(`(${extractCanvasData})()`);
  });

  is(data.length, 10 * 10 * 4, "correct canvas data size");

  let failure = false;
  for (var i = 0; i < 10 * 10 * 4; i += 4) {
    if (
      data[i] != 0x33 ||
      data[i + 1] != 0x22 ||
      data[i + 2] != 0x11 ||
      data[i + 3] != 0xff
    ) {
      ok(false, `incorrect data ${data.slice(i, i + 4)} @ ${i}..${i + 3}`);
      failure = true;
      break;
    }
  }
  ok(!failure, "canvas is correct");

  BrowserTestUtils.removeTab(tab);
});
