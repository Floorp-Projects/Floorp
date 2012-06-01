/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCleaner.h"
#include "cert.h"

CERTVerifyLogContentsCleaner::CERTVerifyLogContentsCleaner(CERTVerifyLog *&cvl)
:m_cvl(cvl)
{
}

CERTVerifyLogContentsCleaner::~CERTVerifyLogContentsCleaner()
{
  if (!m_cvl)
    return;

  CERTVerifyLogNode *i_node;
  for (i_node = m_cvl->head; i_node; i_node = i_node->next)
  {
    if (i_node->cert)
      CERT_DestroyCertificate(i_node->cert);
  }
}

