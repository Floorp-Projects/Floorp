// A 1x1 PNG image.
// Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
const IMAGE = atob("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
                   "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=");

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  let key = aRequest.queryString.includes("what=script") ? "script" : "image";

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
  } else {
    aResponse.setHeader("Content-Type", "image/png", false);
    aResponse.write(IMAGE);
  }
}
