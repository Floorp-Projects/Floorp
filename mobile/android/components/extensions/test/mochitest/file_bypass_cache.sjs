/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80 ft=javascript: */
"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=UTF-8", false);

  if (request.hasHeader("pragma") && request.hasHeader("cache-control")) {
    response.write(`${request.getHeader("pragma")}:${request.getHeader("cache-control")}`);
  }
}