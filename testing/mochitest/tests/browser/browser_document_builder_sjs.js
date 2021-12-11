/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Checks that document-builder.sjs works as expected
add_task(async function assertHtmlParam() {
  const html = "<main><h1>I'm built different</h1></main>";
  const delay = 5000;

  const params = new URLSearchParams({
    delay,
    html,
  });
  params.append("headers", "x-header-1:a");
  params.append("headers", "x-header-2:b");

  const startTime = performance.now();
  const request = new Request(
    `https://example.com/document-builder.sjs?${params}`
  );
  info("Do a fetch request to document-builder.sjs");
  const response = await fetch(request);
  const duration = performance.now() - startTime;

  is(response.status, 200, "Response is a 200");
  ok(
    duration > delay,
    `The delay parameter works as expected (took ${duration}ms)`
  );

  const responseText = await response.text();
  is(responseText, html, "The response has the expected content");

  is(
    response.headers.get("content-type"),
    "text/html",
    "response has the expected content-type"
  );
  is(
    response.headers.get("x-header-1"),
    "a",
    "first header was set as expected"
  );
  is(
    response.headers.get("x-header-2"),
    "b",
    "second header was set as expected"
  );
});

add_task(async function assertFileParam() {
  const file = `browser/testing/mochitest/tests/browser/dummy.html`;
  const request = new Request(
    `https://example.com/document-builder.sjs?file=${file}`
  );

  info("Do a fetch request to document-builder.sjs with a `file` parameter");
  const response = await fetch(request);
  is(response.status, 200, "Response is a 200");
  is(
    response.headers.get("content-type"),
    "text/html",
    "response has the expected content-type"
  );

  const responseText = await response.text();
  const parser = new DOMParser();
  const doc = parser.parseFromString(responseText, "text/html");
  is(
    doc.body.innerHTML.trim(),
    "This is a dummy page",
    "The response has the file content"
  );
});
