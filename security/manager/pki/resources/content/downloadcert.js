/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Bob Lord <lord@netscape.com>
 *  Terry Hayes <thayes@netscape.com>
 */

const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var params;

function onLoad()
{
  params = window.arguments[0].QueryInterface(nsIDialogParamBlock);
  caName = params.GetString(1); 

  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");

  var message2 = bundle.formatStringFromName("newCAMessage1",
                                             [ caName ],
                                              1);
  setText("message2", message2);
}

function viewCert()
{
}

function viewPolicy()
{
}

function doOK()
{
  var checkSSL = document.getElementById("trustSSL");
  var checkEmail = document.getElementById("trustEmail");
  var checkObjSign = document.getElementById("trustObjSign");
  if (checkSSL.checked)
    params.SetInt(2,1);
  else
    params.SetInt(2,0);
  if (checkEmail.checked)
    params.SetInt(3,1);
  else
    params.SetInt(3,0);
  if (checkObjSign.checked)
    params.SetInt(4,1);
  else
    params.SetInt(4,0);
  params.SetInt(1,1);
  window.close();
}

function doCancel()
{
  params.SetInt(1,0);
  window.close();
}
