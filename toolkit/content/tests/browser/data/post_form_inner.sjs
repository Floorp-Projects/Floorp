/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  var body =
   '<html>\
    <body>\
    Inner POST data: ';

  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var bytes = [], avail = 0;
  while ((avail = bodyStream.available()) > 0)
   body += String.fromCharCode.apply(String, bodyStream.readByteArray(avail));

  body +=
    '<form id="postForm" action="post_form_inner.sjs" method="post">\
     <input type="text" name="inputfield" value="inner">\
     <input type="submit">\
     </form>\
     </body>\
     </html>';

  response.bodyOutputStream.write(body, body.length);
}
