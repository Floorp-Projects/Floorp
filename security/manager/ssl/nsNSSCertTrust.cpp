/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSCertTrust.h"

#include "certdb.h"

void nsNSSCertTrust::AddCATrust(bool ssl, bool email) {
  if (ssl) {
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
  if (email) {
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CA);
    addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  }
}

void nsNSSCertTrust::AddPeerTrust(bool ssl, bool email) {
  if (ssl) addTrust(&mTrust.sslFlags, CERTDB_TRUSTED);
  if (email) addTrust(&mTrust.emailFlags, CERTDB_TRUSTED);
}

nsNSSCertTrust::nsNSSCertTrust() { memset(&mTrust, 0, sizeof(CERTCertTrust)); }

nsNSSCertTrust::nsNSSCertTrust(unsigned int ssl, unsigned int email) {
  memset(&mTrust, 0, sizeof(CERTCertTrust));
  addTrust(&mTrust.sslFlags, ssl);
  addTrust(&mTrust.emailFlags, email);
}

nsNSSCertTrust::nsNSSCertTrust(CERTCertTrust* t) {
  if (t)
    memcpy(&mTrust, t, sizeof(CERTCertTrust));
  else
    memset(&mTrust, 0, sizeof(CERTCertTrust));
}

nsNSSCertTrust::~nsNSSCertTrust() {}

void nsNSSCertTrust::SetSSLTrust(bool peer, bool tPeer, bool ca, bool tCA,
                                 bool tClientCA, bool user, bool warn) {
  mTrust.sslFlags = 0;
  if (peer || tPeer) addTrust(&mTrust.sslFlags, CERTDB_TERMINAL_RECORD);
  if (tPeer) addTrust(&mTrust.sslFlags, CERTDB_TRUSTED);
  if (ca || tCA) addTrust(&mTrust.sslFlags, CERTDB_VALID_CA);
  if (tClientCA) addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA) addTrust(&mTrust.sslFlags, CERTDB_TRUSTED_CA);
  if (user) addTrust(&mTrust.sslFlags, CERTDB_USER);
  if (warn) addTrust(&mTrust.sslFlags, CERTDB_SEND_WARN);
}

void nsNSSCertTrust::SetEmailTrust(bool peer, bool tPeer, bool ca, bool tCA,
                                   bool tClientCA, bool user, bool warn) {
  mTrust.emailFlags = 0;
  if (peer || tPeer) addTrust(&mTrust.emailFlags, CERTDB_TERMINAL_RECORD);
  if (tPeer) addTrust(&mTrust.emailFlags, CERTDB_TRUSTED);
  if (ca || tCA) addTrust(&mTrust.emailFlags, CERTDB_VALID_CA);
  if (tClientCA) addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA);
  if (tCA) addTrust(&mTrust.emailFlags, CERTDB_TRUSTED_CA);
  if (user) addTrust(&mTrust.emailFlags, CERTDB_USER);
  if (warn) addTrust(&mTrust.emailFlags, CERTDB_SEND_WARN);
}

void nsNSSCertTrust::SetValidCA() {
  SetSSLTrust(false, false, true, false, false, false, false);
  SetEmailTrust(false, false, true, false, false, false, false);
}

void nsNSSCertTrust::SetValidPeer() {
  SetSSLTrust(true, false, false, false, false, false, false);
  SetEmailTrust(true, false, false, false, false, false, false);
}

bool nsNSSCertTrust::HasAnyCA() {
  if (hasTrust(mTrust.sslFlags, CERTDB_VALID_CA) ||
      hasTrust(mTrust.emailFlags, CERTDB_VALID_CA) ||
      hasTrust(mTrust.objectSigningFlags, CERTDB_VALID_CA))
    return true;
  return false;
}

bool nsNSSCertTrust::HasPeer(bool checkSSL, bool checkEmail) {
  if (checkSSL && !hasTrust(mTrust.sslFlags, CERTDB_TERMINAL_RECORD))
    return false;
  if (checkEmail && !hasTrust(mTrust.emailFlags, CERTDB_TERMINAL_RECORD))
    return false;
  return true;
}

bool nsNSSCertTrust::HasAnyUser() {
  if (hasTrust(mTrust.sslFlags, CERTDB_USER) ||
      hasTrust(mTrust.emailFlags, CERTDB_USER) ||
      hasTrust(mTrust.objectSigningFlags, CERTDB_USER))
    return true;
  return false;
}

bool nsNSSCertTrust::HasTrustedCA(bool checkSSL, bool checkEmail) {
  if (checkSSL && !(hasTrust(mTrust.sslFlags, CERTDB_TRUSTED_CA) ||
                    hasTrust(mTrust.sslFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return false;
  if (checkEmail && !(hasTrust(mTrust.emailFlags, CERTDB_TRUSTED_CA) ||
                      hasTrust(mTrust.emailFlags, CERTDB_TRUSTED_CLIENT_CA)))
    return false;
  return true;
}

bool nsNSSCertTrust::HasTrustedPeer(bool checkSSL, bool checkEmail) {
  if (checkSSL && !(hasTrust(mTrust.sslFlags, CERTDB_TRUSTED))) return false;
  if (checkEmail && !(hasTrust(mTrust.emailFlags, CERTDB_TRUSTED)))
    return false;
  return true;
}

void nsNSSCertTrust::addTrust(unsigned int* t, unsigned int v) { *t |= v; }

bool nsNSSCertTrust::hasTrust(unsigned int t, unsigned int v) {
  return !!(t & v);
}
