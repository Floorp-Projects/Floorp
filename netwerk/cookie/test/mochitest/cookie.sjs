function handleRequest(aRequest, aResponse) {
  let parts = aRequest.queryString.split("&");
  if (parts.includes("window")) {
    aResponse.setStatusLine(aRequest.httpVersion, 200);
    aResponse.setHeader("Content-Type", "text/html");
    aResponse.setHeader("Clear-Site-Data", '"cache", "cookies", "storage"');
    aResponse.write("<body><h1>Welcome</h1></body>");
    return;
  }

  if (parts.includes("fetch")) {
    setState(
      "data",
      JSON.stringify({ type: "fetch", hasCookie: aRequest.hasHeader("Cookie") })
    );
    aResponse.write("Hello world!");
    return;
  }

  if (parts.includes("xhr")) {
    setState(
      "data",
      JSON.stringify({ type: "xhr", hasCookie: aRequest.hasHeader("Cookie") })
    );
    aResponse.write("Hello world!");
    return;
  }

  if (parts.includes("image")) {
    setState(
      "data",
      JSON.stringify({ type: "image", hasCookie: aRequest.hasHeader("Cookie") })
    );

    // A 1x1 PNG image.
    // Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
    const IMAGE = atob(
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
        "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="
    );

    aResponse.setHeader("Content-Type", "image/png", false);
    aResponse.write(IMAGE);
    return;
  }

  if (parts.includes("script")) {
    setState(
      "data",
      JSON.stringify({
        type: "script",
        hasCookie: aRequest.hasHeader("Cookie"),
      })
    );

    aResponse.setHeader("Content-Type", "text/javascript", false);
    aResponse.write("window.scriptLoaded();");
    return;
  }

  if (parts.includes("worker")) {
    setState(
      "data",
      JSON.stringify({
        type: "worker",
        hasCookie: aRequest.hasHeader("Cookie"),
      })
    );

    function w() {
      onmessage = e => {
        if (e.data == "subworker") {
          importScripts("cookie.sjs?subworker&" + Math.random());
          postMessage(42);
          return;
        }

        if (e.data == "fetch") {
          fetch("cookie.sjs?fetch&" + Math.random())
            .then(r => r.text())
            .then(_ => postMessage(42));
          return;
        }

        if (e.data == "xhr") {
          let xhr = new XMLHttpRequest();
          xhr.open("GET", "cookie.sjs?xhr&" + Math.random());
          xhr.send();
          xhr.onload = _ => postMessage(42);
        }
      };
      postMessage(42);
    }

    aResponse.setHeader("Content-Type", "text/javascript", false);
    aResponse.write(w.toString() + "; w();");
    return;
  }

  if (parts.includes("subworker")) {
    setState(
      "data",
      JSON.stringify({
        type: "subworker",
        hasCookie: aRequest.hasHeader("Cookie"),
      })
    );
    aResponse.setHeader("Content-Type", "text/javascript", false);
    aResponse.write("42");
    return;
  }

  if (parts.includes("sharedworker")) {
    setState(
      "data",
      JSON.stringify({
        type: "sharedworker",
        hasCookie: aRequest.hasHeader("Cookie"),
      })
    );

    // This function is exported as a string.
    /* eslint-disable no-undef */
    function w() {
      onconnect = e => {
        e.ports[0].onmessage = evt => {
          if (evt.data == "subworker") {
            importScripts("cookie.sjs?subworker&" + Math.random());
            e.ports[0].postMessage(42);
            return;
          }

          if (evt.data == "fetch") {
            fetch("cookie.sjs?fetch&" + Math.random())
              .then(r => r.text())
              .then(_ => e.ports[0].postMessage(42));
            return;
          }

          if (evt.data == "xhr") {
            let xhr = new XMLHttpRequest();
            xhr.open("GET", "cookie.sjs?xhr&" + Math.random());
            xhr.send();
            xhr.onload = _ => e.ports[0].postMessage(42);
          }
        };
        e.ports[0].postMessage(42);
      };
    }
    /* eslint-enable no-undef */

    aResponse.setHeader("Content-Type", "text/javascript", false);
    aResponse.write(w.toString() + "; w();");
    return;
  }

  if (parts.includes("last")) {
    let data = getState("data");
    setState("data", "");
    aResponse.write(data);
    return;
  }

  aResponse.setStatusLine(aRequest.httpVersion, 400);
  aResponse.write("Invalid request");
}
