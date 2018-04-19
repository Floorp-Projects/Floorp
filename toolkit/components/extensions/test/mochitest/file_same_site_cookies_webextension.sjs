// Custom *.sjs file specifically for the needs of Bug 1454914

const WIN = `<html><body>dummy page setting a same-site cookie</body></html>`;
const FRAME = `<html><body>dummy frame getting a same-site cookie</body></html>`;

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString === "loadWin") {
    response.write(WIN);
    return;
  }

  // using startsWith and discard the math random
  if (request.queryString.startsWith("loadImage")) {
    response.setHeader("Set-Cookie", "myKey=mySameSiteExtensionCookie; samesite=strict", true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  if (request.queryString === "loadXHR") {
    let cookie = "noCookie";
    if (request.hasHeader("Cookie")) {
      cookie = request.getHeader("Cookie");
    }
    response.write(cookie);
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.write("D'oh");
}
