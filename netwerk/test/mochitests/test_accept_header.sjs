function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, "200", "OK");
  dump(`test_accept_header ${request.path}?${request.queryString}\n`);

  if (request.queryString == "worker") {
    response.setHeader("Content-Type", "text/javascript", false);
    response.write("postMessage(42)");

    setState(
      "data",
      JSON.stringify({ type: "worker", accept: request.getHeader("Accept") })
    );
    return;
  }

  if (request.queryString == "image") {
    // A 1x1 PNG image.
    // Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
    const IMAGE = atob(
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
        "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="
    );

    response.setHeader("Content-Type", "image/png", false);
    response.write(IMAGE);

    setState(
      "data",
      JSON.stringify({ type: "image", accept: request.getHeader("Accept") })
    );
    return;
  }

  if (request.queryString == "style") {
    response.setHeader("Content-Type", "text/css", false);
    response.write("");

    setState(
      "data",
      JSON.stringify({ type: "style", accept: request.getHeader("Accept") })
    );
    return;
  }

  if (request.queryString == "iframe") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("<h1>Hello world!</h1>");

    setState(
      "data",
      JSON.stringify({ type: "iframe", accept: request.getHeader("Accept") })
    );
    return;
  }

  if (request.queryString == "get") {
    response.setHeader("Content-Type", "application/json", false);
    response.write(getState("data"));

    setState("data", "");
  }
}
