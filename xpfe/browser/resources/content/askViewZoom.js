/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

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
 * The Original Code is this file as it was released upon
 * September 7, 2000.
 * 
 * The Initial Developer of the Original Code is Peter Annema.
 * Portions created by Peter Annema are Copyright (C) 2000
 * Peter Annema. All Rights Reserved.
 * 
 * Contributor(s):
 *   Peter Annema <disttsc@bart.nl> (Original Author)
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

var dialog;
var retvals;
var zoomMin;
var zoomMax;

function onLoad() {
  retvals = window.arguments[0].retvals;
  zoomMin = window.arguments[0].zoomMin;
  zoomMax = window.arguments[0].zoomMax;

  dialog = new Object();
  dialog.OKButton = document.getElementById("ok");
  dialog.OKButton.setAttribute("disabled","true");

  dialog.input = document.getElementById("zoomValue");
  dialog.input.value = retvals.zoom;
  dialog.input.select();
  doEnabling();

  retvals.zoomOK = false;
  doSetOKCancel(onOK, onCancel);
}

function onOK() {
  var zoom = parseInt(dialog.input.value);
  if (!isNaN(zoom) && zoom >= zoomMin && zoom <= zoomMax) {
    retvals.zoom = zoom;
    retvals.zoomOK = true;
  }
  return retvals.zoomOK;
}

function onCancel() {
  return true;
}

function doEnabling() {
  var enable = false;
  if (dialog.input.value!="") {
    var zoom = parseInt(dialog.input.value);
    if (!isNaN(zoom) && zoom >= zoomMin && zoom <= zoomMax)
      enable = true;
  }

  if (enable) {
    // enable OK
    if (dialog.OKButton.getAttribute("disabled"))
      dialog.OKButton.removeAttribute("disabled");
  } else {
    // disable OK
    if (!dialog.OKButton.getAttribute("disabled"))
      dialog.OKButton.setAttribute("disabled","true");
  }
}
