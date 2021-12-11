// small red image
const IMAGE = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  if (aRequest.queryString.includes("result")) {
    aResponse.write(getState("hints") || 0);
    setState("hints", "0");
  } else {
    let hints = parseInt(getState("hints") || 0) + 1;
    setState("hints", hints.toString());

    aResponse.setHeader("Content-Type", "image/png", false);
    aResponse.write(IMAGE);
  }
}
