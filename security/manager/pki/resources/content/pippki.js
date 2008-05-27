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
 *   Kaspar Brand <mozcontrib@velox.ch>
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

/*
 * These are helper functions to be included
 * pippki UI js files.
 */

function setText(id, value) {
  var element = document.getElementById(id);
  if (!element) return;
     if (element.hasChildNodes())
       element.removeChild(element.firstChild);
  var textNode = document.createTextNode(value);
  element.appendChild(textNode);
}

const nsICertificateDialogs = Components.interfaces.nsICertificateDialogs;
const nsCertificateDialogs = "@mozilla.org/nsCertificateDialogs;1"

function viewCertHelper(parent, cert) {
  if (!cert)
    return;

  var cd = Components.classes[nsCertificateDialogs].getService(nsICertificateDialogs);
  cd.viewCert(parent, cert);
}

function getDERString(cert)
{
  var length = {};
  var derArray = cert.getRawDER(length);
  var derString = '';
  for (var i = 0; i < derArray.length; i++) {
    derString += String.fromCharCode(derArray[i]);
  }
  return derString;
}

function getPKCS7String(cert, chainMode)
{
  var length = {};
  cert.QueryInterface(Components.interfaces.nsIX509Cert3);
  var pkcs7Array = cert.exportAsCMS(chainMode, length);
  var pkcs7String = '';
  for (var i = 0; i < pkcs7Array.length; i++) {
    pkcs7String += String.fromCharCode(pkcs7Array[i]);
  }
  return pkcs7String;
}

function getPEMString(cert)
{
  var derb64 = btoa(getDERString(cert));
  // Wrap the Base64 string into lines of 64 characters, 
  // with CRLF line breaks (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return "-----BEGIN CERTIFICATE-----\r\n"
         + wrapped
         + "\r\n-----END CERTIFICATE-----\r\n";
}
 
function alertPromptService(title, message)
{
  var ps = null;
  var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
           getService(Components.interfaces.nsIPromptService);
  ps.alert(window, title, message);
}

function exportToFile(parent, cert)
{
  var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
  if (!cert)
    return;

  var nsIFilePicker = Components.interfaces.nsIFilePicker;
  var fp = Components.classes["@mozilla.org/filepicker;1"].
           createInstance(nsIFilePicker);
  fp.init(parent, bundle.GetStringFromName("SaveCertAs"),
          nsIFilePicker.modeSave);
  var filename = cert.commonName;
  if (!filename.length)
    filename = cert.windowTitle;
  // remove all whitespace from the default filename
  fp.defaultString = filename.replace(/\s*/g,'');
  fp.defaultExtension = "crt";
  fp.appendFilter(bundle.GetStringFromName("CertFormatBase64"), "*.crt; *.pem");
  fp.appendFilter(bundle.GetStringFromName("CertFormatBase64Chain"), "*.crt; *.pem");
  fp.appendFilter(bundle.GetStringFromName("CertFormatDER"), "*.der");
  fp.appendFilter(bundle.GetStringFromName("CertFormatPKCS7"), "*.p7c");
  fp.appendFilter(bundle.GetStringFromName("CertFormatPKCS7Chain"), "*.p7c");
  fp.appendFilters(nsIFilePicker.filterAll);
  var res = fp.show();
  if (res != nsIFilePicker.returnOK && res != nsIFilePicker.returnReplace)
    return;

  var content = '';
  switch (fp.filterIndex) {
    case 1:
      content = getPEMString(cert);
      var chain = cert.getChain();
      for (var i = 1; i < chain.length; i++)
        content += getPEMString(chain.queryElementAt(i, Components.interfaces.nsIX509Cert));
      break;
    case 2:
      content = getDERString(cert);
      break;
    case 3:
      content = getPKCS7String(cert, Components.interfaces.nsIX509Cert3.CMS_CHAIN_MODE_CertOnly);
      break;
    case 4:
      content = getPKCS7String(cert, Components.interfaces.nsIX509Cert3.CMS_CHAIN_MODE_CertChainWithRoot);
      break;
    case 0:
    default:
      content = getPEMString(cert);
      break;
  }
  var msg;
  var written = 0;
  try {
    var file = Components.classes["@mozilla.org/file/local;1"].
               createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(fp.file.path);
    var fos = Components.classes["@mozilla.org/network/file-output-stream;1"].
              createInstance(Components.interfaces.nsIFileOutputStream);
    // flags: PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
    fos.init(file, 0x02 | 0x08 | 0x20, 00644, 0);
    written = fos.write(content, content.length);
    fos.close();
  }
  catch(e) {
    switch (e.result) {
      case Components.results.NS_ERROR_FILE_ACCESS_DENIED:
        msg = bundle.GetStringFromName("writeFileAccessDenied");
        break;
      case Components.results.NS_ERROR_FILE_IS_LOCKED:
        msg = bundle.GetStringFromName("writeFileIsLocked");
        break;
      case Components.results.NS_ERROR_FILE_NO_DEVICE_SPACE:
      case Components.results.NS_ERROR_FILE_DISK_FULL:
        msg = bundle.GetStringFromName("writeFileNoDeviceSpace");
        break;
      default:
        msg = e.message;
        break;
    }
  }
  if (written != content.length) {
    if (!msg.length)
      msg = bundle.GetStringFromName("writeFileUnknownError");
    alertPromptService(bundle.GetStringFromName("writeFileFailure"),
                       bundle.formatStringFromName("writeFileFailed",
                         [ fp.file.path, msg ], 2));
  }
}
