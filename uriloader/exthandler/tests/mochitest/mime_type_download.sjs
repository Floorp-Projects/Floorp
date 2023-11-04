function handleRequest(request, response) {
  "use strict";

  let content = "";
  let params = new URLSearchParams(request.queryString);
  let extension = params.get("extension");
  let contentType = params.get("contentType");
  if (params.has("withHeader")) {
    response.setHeader(
      "Content-Disposition",
      `attachment; filename="mime_type_download${
        extension ? "." + extension : ""
      }";`,
      false
    );
  }
  response.setHeader("Content-Type", contentType, false);
  response.setHeader("Content-Length", "" + content.length, false);
  response.setStatusLine(request.httpVersion, 200);
  response.write(content);
}
