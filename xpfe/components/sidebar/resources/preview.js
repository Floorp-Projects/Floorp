/* -*- Mode: Java -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corp. Portions created by Netscape Communications
 * Corp. are Copyright (C) 1999 Netscape Communications Corp. All
 * Rights Reserved.
 *
 * Contributor(s): Stephen Lamm <slamm@netscape.com>
 */

function Init()
{
  var panel_name = window.arguments[0];
  var panel_URL = window.arguments[1];

  var panel_title = document.getElementById('paneltitle');
  var preview_frame = document.getElementById('previewframe');
  panel_title.setAttribute('label', panel_name);
  preview_frame.setAttribute('src', panel_URL);
}
