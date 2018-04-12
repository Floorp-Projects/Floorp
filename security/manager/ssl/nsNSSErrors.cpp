/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSComponent.h"
#include "pkix/pkixnss.h"
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
  const char *id_str = nullptr;

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

    case mozilla::pkix::MOZILLA_PKIX_ERROR_MITM_DETECTED:
      id_str = "certErrorTrust_MitM";
      break;
  }

  return id_str;
}
