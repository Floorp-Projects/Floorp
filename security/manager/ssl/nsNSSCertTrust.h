/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSCertTrust_h
#define nsNSSCertTrust_h

#include "certdb.h"
#include "certt.h"

/*
 * Class for maintaining trust flags for an NSS certificate.
 */
class nsNSSCertTrust
{
public:
  nsNSSCertTrust();
  nsNSSCertTrust(unsigned int ssl, unsigned int email);
  explicit nsNSSCertTrust(CERTCertTrust *t);
  virtual ~nsNSSCertTrust();

  /* query */
  bool HasAnyCA();
  bool HasAnyUser();
  bool HasPeer(bool checkSSL = true, bool checkEmail = true);
  bool HasTrustedCA(bool checkSSL = true, bool checkEmail = true);
  bool HasTrustedPeer(bool checkSSL = true, bool checkEmail = true);

  /* common defaults */
  /* equivalent to "c,c,c" */
  void SetValidCA();
  /* equivalent to "p,p,p" */
  void SetValidPeer();

  /* general setters */
  /* read: "p, P, c, C, T, u, w" */
  void SetSSLTrust(bool peer, bool tPeer,
                   bool ca,   bool tCA, bool tClientCA,
                   bool user, bool warn);

  void SetEmailTrust(bool peer, bool tPeer,
                     bool ca,   bool tCA, bool tClientCA,
                     bool user, bool warn);

  /* set c <--> CT */
  void AddCATrust(bool ssl, bool email);
  /* set p <--> P */
  void AddPeerTrust(bool ssl, bool email);

  CERTCertTrust& GetTrust() { return mTrust; }

private:
  void addTrust(unsigned int *t, unsigned int v);
  void removeTrust(unsigned int *t, unsigned int v);
  bool hasTrust(unsigned int t, unsigned int v);
  CERTCertTrust mTrust;
};

#endif // nsNSSCertTrust_h
