/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Javier Delgadillo <javi@netscape.com>
 */


const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var params;


function setText(id, value) {
  var element = document.getElementById(id);
  if (!element) return;
     if (element.hasChildNodes())  
       element.removeChild(element.firstChild);
  var textNode = document.createTextNode(value);
  element.appendChild(textNode);
}

function onLoad()
{
  params = window.arguments[0].QueryInterface(nsIDialogParamBlock);  
  serverName = params.GetString(1); 

  var bundle = srGetStrBundle("chrome://pippki/locale/newserver.properties");
  var gBundleBrand = srGetStrBundle("chrome://global/locale/brand.properties");

  var brandName = gBundleBrand.GetStringFromName("brandShortName");
  var message1 = bundle.formatStringFromName("newServerMessage1", 
                                             [ serverName, brandName ],
                                             2);
  var message4 = bundle.formatStringFromName("newServerMessage4", 
                                             [ serverName ],
                                              1);
  setText("message1", message1);
  setText("message4", message4);
}

function doOK()
{
  params.SetInt(1,1);
  var radioGroup = document.getElementById("trustSiteCert");
  params.SetInt(2,parseInt(radioGroup.selectedItem.data));
  window.close();
}

function doCancel()
{
  params.SetInt(1,0);
  window.close();
}
