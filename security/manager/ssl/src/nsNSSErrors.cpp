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
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kai Engert <kengert@redhat.com>
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

#include "nsNSSComponent.h"
#include "secerr.h"
#include "sslerr.h"

const char *
nsNSSErrors::getDefaultErrorStringName(PRErrorCode err)
{
  return PR_ErrorToName(err);
}

const char *
nsNSSErrors::getOverrideErrorStringName(PRErrorCode aErrorCode)
{
  const char *id_str = nsnull;

  switch (aErrorCode) {
    case SSL_ERROR_SSL_DISABLED:
      id_str = "PSMERR_SSL_Disabled";
      break;
  
    case SSL_ERROR_SSL2_DISABLED:
      id_str = "PSMERR_SSL2_Disabled";
      break;
  
    case SEC_ERROR_REUSED_ISSUER_AND_SERIAL:
      id_str = "PSMERR_HostReusedIssuerSerial";
      break;
  }

  return id_str;
}

nsresult
nsNSSErrors::getErrorMessageFromCode(PRErrorCode err,
                                     nsINSSComponent *component,
                                     nsString &returnedMessage)
{
  NS_ENSURE_ARG_POINTER(component);
  returnedMessage.Truncate();

  const char *nss_error_id_str = getDefaultErrorStringName(err);
  const char *id_str = getOverrideErrorStringName(err);

  if (id_str || nss_error_id_str)
  {
    nsString defMsg;
    nsresult rv;
    if (id_str)
    {
      rv = component->GetPIPNSSBundleString(id_str, defMsg);
    }
    else
    {
      rv = component->GetNSSBundleString(nss_error_id_str, defMsg);
    }

    if (NS_SUCCEEDED(rv))
    {
      returnedMessage.Append(defMsg);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
  }
  
  if (returnedMessage.IsEmpty())
  {
    // no localized string available, use NSS' internal
    returnedMessage.AppendASCII(PR_ErrorToString(err, PR_LANGUAGE_EN));
    returnedMessage.Append(NS_LITERAL_STRING("\n"));
  }
  
  if (nss_error_id_str)
  {
    nsresult rv;
    nsCString error_id(nss_error_id_str);
    ToLowerCase(error_id);
    NS_ConvertASCIItoUTF16 idU(error_id);

    const PRUnichar *params[1];
    params[0] = idU.get();

    nsString formattedString;
    rv = component->PIPBundleFormatStringFromName("certErrorCodePrefix", 
                                                  params, 1, 
                                                  formattedString);
    if (NS_SUCCEEDED(rv)) {
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
      returnedMessage.Append(formattedString);
      returnedMessage.Append(NS_LITERAL_STRING("\n"));
    }
    else {
      returnedMessage.Append(NS_LITERAL_STRING("("));
      returnedMessage.Append(idU);
      returnedMessage.Append(NS_LITERAL_STRING(")"));
    }
  }

  return NS_OK;
}
