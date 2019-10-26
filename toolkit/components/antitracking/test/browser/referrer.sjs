// A 1x1 PNG image.
// Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
const IMAGE = atob("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
                   "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=");

const IFRAME = "<!DOCTYPE html>\n" +
               "<script>\n" +
                 "onmessage = event => {\n" +
                   "parent.postMessage(document.referrer, '*');\n" +
                 "};\n" +
               "</script>";

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  let key = aRequest.queryString.includes("what=script") ? "script" :
            (aRequest.queryString.includes("what=image") ? "image" : "iframe");

  if (aRequest.queryString.includes("result")) {
    aResponse.write(getState(key));
    setState(key, "");
    return;
  }

  if (aRequest.hasHeader('Referer')) {
    let referrer = aRequest.getHeader('Referer');
    setState(key, referrer);
  }

  if (key == "script") {
    aResponse.setHeader("Content-Type", "text/javascript", false);
    aResponse.write("42;");
  } else if (key == "image") {
    aResponse.setHeader("Content-Type", "image/png", false);
    aResponse.write(IMAGE);
  } else {
    aResponse.setHeader("Content-Type", "text/html", false);
    aResponse.write(IFRAME);
  }
}
