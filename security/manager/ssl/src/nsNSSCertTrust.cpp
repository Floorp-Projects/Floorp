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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
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

#include "nsNSSCertTrust.h"

void
nsNSSCertTrust::AddCATrust(bool ssl, bool email, bool objSign)
{
  if (ssl) {
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
  if (email) {
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
  if (objSign) {
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
}

void
nsNSSCertTrust::AddPeerTrust(bool ssl, bool email, bool objSign)
{
  if (ssl)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED);
  if (email)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED);
  if (objSign)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED);
}

nsNSSCertTrust::nsNSSCertTrust()
{
  memset(&mTrust, 0, sizeof(CERTCertTrust));
}

nsNSSCertTrust::nsNSSCertTrust(unsigned int ssl, 
                               unsigned int email, 
                               unsigned int objsign)
{
  memset(&mTrust, 0, sizeof(CERTCertTrust));
  addTrust(&mTrust.sslFlags, ssl);
  addTrust(&mTrust.emailFlags, email);
  addTrust(&mTrust.objectSigningFlags, objsign);
}

nsNSSCertTrust::nsNSSCertTrust(CERTCertTrust *t)
{
  if (t)
    memcpy(&mTrust, t, sizeof(CERTCertTrust));
  else
    memset(&mTrust, 0, sizeof(CERTCertTrust)); 
}

nsNSSCertTrust::~nsNSSCertTrust()
{
}

void
nsNSSCertTrust::SetSSLTrust(bool peer, bool tPeer,
                            bool ca,   bool tCA, bool tClientCA,
                            bool user, bool warn)
{
  mTrust.sslFlags = 0;
  if (peer || tPeer)
    addTrust(&mTrust.sslFlags, CERTDB_VALID_PEER);
  if (tPeer)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    addTrust(&mTrust.sslFlags, CERTDB_VALID_CA);
  if (tClientCA)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CA);
  if (user)
    addTrust(&mTrust.sslFlags, CERTDB_USER);
  if (warn)
    addTrust(&mTrust.sslFlags, CERTDB_SEND_WARN);
}

void
nsNSSCertTrust::SetEmailTrust(bool peer, bool tPeer,
                              bool ca,   bool tCA, bool tClientCA,
                              bool user, bool warn)
{
  mTrust.emailFlags = 0;
  if (peer || tPeer)
    addTrust(&mTrust.emailFlags, CERTDB_VALID_PEER);
  if (tPeer)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    addTrust(&mTrust.emailFlags, CERTDB_VALID_CA);
  if (tClientCA)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CA);
  if (user)
    addTrust(&mTrust.emailFlags, CERTDB_USER);
  if (warn)
    addTrust(&mTrust.emailFlags, CERTDB_SEND_WARN);
}

void
nsNSSCertTrust::SetObjSignTrust(bool peer, bool tPeer,
                                bool ca,   bool tCA, bool tClientCA,
                                bool user, bool warn)
{
  mTrust.objectSigningFlags = 0;
  if (peer || tPeer)
    addTrust(&mTrust.objectSigningFlags, CERTDB_VALID_PEER);
  if (tPeer)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED);
  if (ca || tCA)
    addTrust(&mTrust.objectSigningFlags, CERTDB_VALID_CA);
  if (tClientCA)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA)
    addTrust(&mTrust.objectSigningFlags, CERTDB_TRUSTED_CA);
  if (user)
    addTrust(&mTrust.objectSigningFlags, CERTDB_USER);
  if (warn)
    addTrust(&mTrust.objectSigningFlags, CERTDB_SEND_WARN);
}

void
nsNSSCertTrust::SetValidCA()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_TRUE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_TRUE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void
nsNSSCertTrust::SetTrustedServerCA()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_TRUE, PR_TRUE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_TRUE, PR_TRUE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_TRUE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void
nsNSSCertTrust::SetTrustedCA()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_TRUE, PR_TRUE, PR_TRUE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_TRUE, PR_TRUE, PR_TRUE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_TRUE, PR_TRUE,
                  PR_FALSE, PR_FALSE);
}

void 
nsNSSCertTrust::SetValidPeer()
{
  SetSSLTrust(PR_TRUE, PR_FALSE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_TRUE, PR_FALSE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_TRUE, PR_FALSE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void 
nsNSSCertTrust::SetValidServerPeer()
{
  SetSSLTrust(PR_TRUE, PR_FALSE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void 
nsNSSCertTrust::SetTrustedPeer()
{
  SetSSLTrust(PR_TRUE, PR_TRUE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE);
  SetEmailTrust(PR_TRUE, PR_TRUE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE);
  SetObjSignTrust(PR_TRUE, PR_TRUE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE);
}

void
nsNSSCertTrust::SetUser()
{
  SetSSLTrust(PR_FALSE, PR_FALSE,
              PR_FALSE, PR_FALSE, PR_FALSE,
              PR_TRUE, PR_FALSE);
  SetEmailTrust(PR_FALSE, PR_FALSE,
                PR_FALSE, PR_FALSE, PR_FALSE,
                PR_TRUE, PR_FALSE);
  SetObjSignTrust(PR_FALSE, PR_FALSE,
                  PR_FALSE, PR_FALSE, PR_FALSE,
                  PR_TRUE, PR_FALSE);
}

bool
nsNSSCertTrust::HasAnyCA()
{
  if (hasTrust(mTrust.sslFlags, CERTDB_VALID_CA) ||
      hasTrust(mTrust.emailFlags, CERTDB_VALID_CA) ||
      hasTrust(mTrust.objectSigningFlags, CERTDB_VALID_CA))
    return PR_TRUE;
  return PR_FALSE;
}

bool
nsNSSCertTrust::HasCA(bool checkSSL, 
                      bool checkEmail,  
                      bool checkObjSign)
{
  if (checkSSL && !hasTrust(mTrust.sslFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  if (checkEmail && !hasTrust(mTrust.emailFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  if (checkObjSign && !hasTrust(mTrust.objectSigningFlags, CERTDB_VALID_CA))
    return PR_FALSE;
  return PR_TRUE;
}

bool
nsNSSCertTrust::HasPeer(bool checkSSL, 
                        bool checkEmail,  
                        bool checkObjSign)
{
  if (checkSSL && !hasTrust(mTrust.sslFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  if (checkEmail && !hasTrust(mTrust.emailFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  if (checkObjSign && !hasTrust(mTrust.objectSigningFlags, CERTDB_VALID_PEER))
    return PR_FALSE;
  return PR_TRUE;
}

bool
nsNSSCertTrust::HasAnyUser()
{
  if (hasTrust(mTrust.sslFlags, CERTDB_USER) ||
      hasTrust(mTrust.emailFlags, CERTDB_USER) ||
      hasTrust(mTrust.objectSigningFlags, CERTDB_USER))
    return PR_TRUE;
  return PR_FALSE;
}

bool
nsNSSCertTrust::HasUser(bool checkSSL, 
                        bool checkEmail,  
                        bool checkObjSign)
{
  if (checkSSL && !hasTrust(mTrust.sslFlags, CERTDB_USER))
    return PR_FALSE;
  if (checkEmail && !hasTrust(mTrust.emailFlags, CERTDB_USER))
    return PR_FALSE;
  if (checkObjSign && !hasTrust(mTrust.objectSigningFlags, CERTDB_USER))
    return PR_FALSE;
  return PR_TRUE;
}

bool
nsNSSCertTrust::HasTrustedCA(bool checkSSL, 
                             bool checkEmail,  
                             bool checkObjSign)
{
  if (checkSSL && !(hasTrust(mTrust.sslFlags, CERTDB_TRUSTED_CA) ||
                    hasTrust(mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  if (checkEmail && !(hasTrust(mTrust.emailFlags, CERTDB_TRUSTED_CA) ||
                      hasTrust(mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  if (checkObjSign && 
       !(hasTrust(mTrust.objectSigningFlags, CERTDB_TRUSTED_CA) ||
         hasTrust(mTrust.objectSigningFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return PR_FALSE;
  return PR_TRUE;
}

bool
nsNSSCertTrust::HasTrustedPeer(bool checkSSL, 
                               bool checkEmail,  
                               bool checkObjSign)
{
  if (checkSSL && !(hasTrust(mTrust.sslFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  if (checkEmail && !(hasTrust(mTrust.emailFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  if (checkObjSign && 
       !(hasTrust(mTrust.objectSigningFlags, CERTDB_TRUSTED)))
    return PR_FALSE;
  return PR_TRUE;
}

void
nsNSSCertTrust::addTrust(unsigned int *t, unsigned int v)
{
  *t |= v;
}

bool
nsNSSCertTrust::hasTrust(unsigned int t, unsigned int v)
{
  return !!(t & v);
}

