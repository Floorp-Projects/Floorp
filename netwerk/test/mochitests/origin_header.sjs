/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Cache-Control", "no-cache", false);

    var origin = request.hasHeader("Origin") ? request.getHeader("Origin") : "";
    response.write("Origin: " + origin);
}
