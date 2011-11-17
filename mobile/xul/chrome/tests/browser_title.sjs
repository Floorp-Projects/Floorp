/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aditya Rao <adicoolrao@gmail.com>
 *   Vivien Nicolas <21@vingtetun.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);

  let action = "";
  let query = decodeURIComponent(request.queryString || "");
  response.write("<html xmlns=\"http://www.w3.org/1999/xhtml\"><head>");

  switch (query) {
    case "no_title":
      break;
    case "english_title":
      response.write("<title>English Title Page</title>");
      break;
    case "dynamic_title":
      response.write("<title>This is an english title page</title>");
      response.write("<script>window.addEventListener('load', function() { window.removeEventListener('load', arguments.callee, false); document.title = 'This is not a french title'; }, false);</script>");
      break;
    case "redirect":
      response.write("<meta http-equiv='refresh' content='1;url=http://mochi.test:8888/browser/mobile/chrome/tests/browser_title.sjs?no_title'></meta>");
      break;
    case "location":
      response.write("<script>window.addEventListener('load', function() { window.removeEventListener('load', arguments.callee, false); document.location = 'http://mochi.test:8888/browser/mobile/chrome/tests/browser_title.sjs?no_title' ; }, false);</script>");
      break;
    default:
      break;
  }
  response.write("</head><body>" + query + "</body></html>");
}
