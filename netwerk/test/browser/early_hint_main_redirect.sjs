"use strict";

// In an SJS file we need to get the setTimeout bits ourselves, despite
// what eslint might think applies for browser tests.
// eslint-disable-next-line mozilla/no-redeclare-with-import-autofix
let { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

async function handleRequest(request, response) {
  let hinted =
    request.hasHeader("X-Moz") && request.getHeader("X-Moz") === "early hint";
  let count = JSON.parse(getSharedState("earlyHintCount"));
  if (hinted) {
    count.hinted += 1;
  } else {
    count.normal += 1;
  }
  setSharedState("earlyHintCount", JSON.stringify(count));
  response.setHeader("Cache-Control", "max-age=604800", false);

  let content = "";
  let qs = new URLSearchParams(request.queryString);
  let asset = qs.get("as");

  if (asset === "image") {
    response.setHeader("Content-Type", "image/png", false);
    // set to green/black horizontal stripes (71 bytes)
    content = atob(
      hinted
        ? "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAADklEQVQIW2OU+i/FAAcADoABNV8XGBMAAAAASUVORK5CYII="
        : "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAE0lEQVQIW2P4//+/N8MkBiAGsgA1bAe1SzDY8gAAAABJRU5ErkJggg=="
    );
  } else if (asset === "style") {
    response.setHeader("Content-Type", "text/css", false);
    // green background on hint response, purple response otherwise
    content = `#square { background: ${hinted ? "#1aff1a" : "#4b0092"}`;
  } else if (asset === "script") {
    response.setHeader("Content-Type", "application/javascript", false);
    // green background on hint response, purple response otherwise
    content = `window.onload = function() {
      document.getElementById('square').style.background = "${
        hinted ? "#1aff1a" : "#4b0092"
      }";
    }`;
  } else if (asset === "module") {
    response.setHeader("Content-Type", "application/javascript", false);
    // green background on hint response, purple response otherwise
    content = `export function draw() {
      document.getElementById('square').style.background = "${
        hinted ? "#1aff1a" : "#4b0092"
      }";
    }`;
  } else if (asset === "fetch") {
    response.setHeader("Content-Type", "text/plain", false);
    content = hinted ? "hinted" : "normal";
  } else if (asset === "font") {
    response.setHeader("Content-Type", "font/svg+xml", false);
    content = '<font><font-face font-family="preloadFont" /></font>';
  }
  response.processAsync();
  setTimeout(() => {
    response.write(content);
    response.finish();
  }, 0);
  //response.write(content);
}
