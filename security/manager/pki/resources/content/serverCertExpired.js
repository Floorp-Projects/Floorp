/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Delgadillo <javi@netscape.com>
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


const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;
const nsIPKIParamBlock    = Components.interfaces.nsIPKIParamBlock;
const nsIX509Cert         = Components.interfaces.nsIX509Cert;

var dialogParams;
var pkiParams;
var cert=null;

function onLoad()
{
  pkiParams    = window.arguments[0].QueryInterface(nsIPKIParamBlock);
  dialogParams = pkiParams.QueryInterface(nsIDialogParamBlock);

  document.title = dialogParams.GetString(2);
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  var message1 = dialogParams.GetString(1);
  
  const nsIScriptableDateFormat = Components.interfaces.nsIScriptableDateFormat;
  var dateService = Components.classes["@mozilla.org/intl/scriptabledateformat;1"].getService(nsIScriptableDateFormat);
  var date = new Date(); 
  var dateStr = dateService.FormatDateTime("", dateService.dateFormatShort, dateService.timeFormatNoSeconds, 
                                           date.getFullYear(), date.getMonth()+1, date.getDate(), 
                                           date.getHours(), date.getMinutes(), date.getSeconds());

  var message2 = bundle.formatStringFromName("serverCertExpiredMsg2", 
                                             [ dateStr ],
                                              1);
  setText("message1", message1);
  setText("message2", message2);
  document.documentElement.getButton("accept").focus();
}

function doOK()
{
  dialogParams.SetInt(1,1);
  return true;
}

function doCancel()
{
  dialogParams.SetInt(1,0);
  return true;
}

function viewCert()
{
  if (cert == null) {
    var isupport = pkiParams.getISupportAtIndex(1);
    cert = isupport.QueryInterface(nsIX509Cert);
  }
  viewCertHelper(window, cert);
}
