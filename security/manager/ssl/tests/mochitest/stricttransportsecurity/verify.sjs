/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// SJS file that serves un-cacheable responses for STS tests that postMessage
// to the parent saying whether or not they were loaded securely.

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
      var [name, value] = val.split('=');
      query[name] = unescape(value);
  });

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  if ('id' in query) {
    var outstr = [
       " <!DOCTYPE html>",
       " <html> <head> <title>subframe for STS</title>",
       " <script type='text/javascript'>",
       " var self = window;",
       " window.addEventListener('load', function() {",
       "     if (document.location.protocol === 'https:') {",
       "       self.parent.postMessage('SECURE " + query['id'] + "',",
       "                               'http://mochi.test:8888');",
       "     } else {",
       "       self.parent.postMessage('INSECURE " + query['id'] + "',",
       "                               'http://mochi.test:8888');",
       "     }",
       "   }, false);",
       "   </script>",
       "   </head>",
       " <body>",
       "   STS state verification frame loaded via",
       " <script>",
       "   document.write(document.location.protocol);",
       " </script>",
       " </body>",
       " </html>"].join("\n");
    response.write(outstr);
  } else {
    response.write("ERROR: no id provided");
  }
}
