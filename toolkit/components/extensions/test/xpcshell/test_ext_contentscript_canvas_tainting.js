"use strict";

const server = createHttpServer({
  hosts: ["green.example.com", "red.example.com"],
});

server.registerDirectory("/data/", do_get_file("data"));

server.registerPathHandler("/pixel.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write(`<!DOCTYPE html>
    <script>
      function readByWeb() {
        let ctx = document.querySelector("canvas").getContext("2d");
        let {data} = ctx.getImageData(0, 0, 1, 1);
        return data.slice(0, 3).join();
      }
    </script>
  `);
});

add_task(async function test_contentscript_canvas_tainting() {
  async function contentScript() {
    let canvas = document.createElement("canvas");
    let ctx = canvas.getContext("2d");
    document.body.appendChild(canvas);

    function draw(url) {
      return new Promise(resolve => {
        let img = document.createElement("img");
        img.onload = () => {
          ctx.drawImage(img, 0, 0, 1, 1);
          resolve();
        };
        img.src = url;
      });
    }

    function readByExt() {
      let { data } = ctx.getImageData(0, 0, 1, 1);
      return data.slice(0, 3).join();
    }

    let readByWeb = window.wrappedJSObject.readByWeb;

    // Test reading after drawing an image from the same origin as the web page.
    await draw("http://green.example.com/data/pixel_green.gif");
    browser.test.assertEq(
      readByWeb(),
      "0,255,0",
      "Content can read same-origin image"
    );
    browser.test.assertEq(
      readByExt(),
      "0,255,0",
      "Extension can read same-origin image"
    );

    // Test reading after drawing a blue pixel data URI from extension content script.
    await draw(
      "data:image/gif;base64,R0lGODlhAQABAIABAAAA/wAAACwAAAAAAQABAAACAkQBADs="
    );
    browser.test.assertThrows(
      readByWeb,
      /operation is insecure/,
      "Content can't read extension's image"
    );
    browser.test.assertEq(
      readByExt(),
      "0,0,255",
      "Extension can read its own image"
    );

    // Test after tainting the canvas with an image from a third party domain.
    await draw("http://red.example.com/data/pixel_red.gif");
    browser.test.assertThrows(
      readByWeb,
      /operation is insecure/,
      "Content can't read third party image"
    );
    browser.test.assertThrows(
      readByExt,
      /operation is insecure/,
      "Extension can't read fully tainted"
    );

    // Test canvas is still fully tainted after drawing extension's data: image again.
    await draw(
      "data:image/gif;base64,R0lGODlhAQABAIABAAAA/wAAACwAAAAAAQABAAACAkQBADs="
    );
    browser.test.assertThrows(
      readByWeb,
      /operation is insecure/,
      "Canvas still fully tainted for content"
    );
    browser.test.assertThrows(
      readByExt,
      /operation is insecure/,
      "Canvas still fully tainted for extension"
    );

    browser.test.sendMessage("done");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://green.example.com/pixel.html"],
          js: ["cs.js"],
        },
      ],
    },
    files: {
      "cs.js": contentScript,
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://green.example.com/pixel.html"
  );
  await extension.awaitMessage("done");

  await contentPage.close();
  await extension.unload();
});
