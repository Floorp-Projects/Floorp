/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  response.write("Hello world!");
}
