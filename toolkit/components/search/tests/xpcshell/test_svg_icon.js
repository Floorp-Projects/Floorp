/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var url;
var requestHandled;

const icon =
  '<?xml version="1.0" encoding="UTF-8" standalone="no"?>' +
  '<svg xmlns="http://www.w3.org/2000/svg" ' +
       'width="16" height="16" viewBox="0 0 16 16">' +
   '<rect x="4" y="4" width="8px" height="8px" style="fill: blue"/>' +
  '</svg>';

function run_test() {
  useHttpServer(); // Unused, but required to call addTestEngines.

  requestHandled = new Promise(resolve => {
    let srv = new HttpServer();
    srv.registerPathHandler("/icon.svg", (metadata, response) => {
      response.setStatusLine("1.0", 200, "OK");
      response.setHeader("Content-Type", "image/svg+xml", false);

      response.write(icon);
      resolve();
    });
    srv.start(-1);
    do_register_cleanup(() => srv.stop(() => {}));

    url = "http://localhost:" + srv.identity.primaryPort + "/icon.svg";
  });

  run_next_test();
}

add_task(function* test_svg_icon() {
  yield asyncInit();

  let [engine] = yield addTestEngines([
    { name: "SVGIcon", details: [url, "", "SVG icon", "GET",
                                 "http://icon.svg/search?q={searchTerms}"] },
  ]);

  yield requestHandled;
  yield promiseAfterCache();

  ok(engine.iconURI, "the engine has an icon");
  ok(engine.iconURI.spec.startsWith("data:image/svg+xml"),
     "the icon is saved as an SVG data url");
});
