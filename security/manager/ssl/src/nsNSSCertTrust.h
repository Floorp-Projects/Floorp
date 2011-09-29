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

#ifndef _NSNSSCERTTRUST_H_
#define _NSNSSCERTTRUST_H_

#include "certt.h"
#include "certdb.h"

/*
 * nsNSSCertTrust
 * 
 * Class for maintaining trust flags for an NSS certificate.
 */
class nsNSSCertTrust
{
public:
  nsNSSCertTrust();
  nsNSSCertTrust(unsigned int ssl, unsigned int email, unsigned int objsign);
  nsNSSCertTrust(CERTCertTrust *t);
  virtual ~nsNSSCertTrust();

  /* query */
  bool HasAnyCA();
  bool HasAnyUser();
  bool HasCA(bool checkSSL = true, 
               bool checkEmail = true,  
               bool checkObjSign = true);
  bool HasPeer(bool checkSSL = true, 
                 bool checkEmail = true,  
                 bool checkObjSign = true);
  bool HasUser(bool checkSSL = true, 
                 bool checkEmail = true,  
                 bool checkObjSign = true);
  bool HasTrustedCA(bool checkSSL = true, 
                      bool checkEmail = true,  
                      bool checkObjSign = true);
  bool HasTrustedPeer(bool checkSSL = true, 
                        bool checkEmail = true,  
                        bool checkObjSign = true);

  /* common defaults */
  /* equivalent to "c,c,c" */
  void SetValidCA();
  /* equivalent to "C,C,C" */
  void SetTrustedServerCA();
  /* equivalent to "CT,CT,CT" */
  void SetTrustedCA();
  /* equivalent to "p,," */
  void SetValidServerPeer();
  /* equivalent to "p,p,p" */
  void SetValidPeer();
  /* equivalent to "P,P,P" */
  void SetTrustedPeer();
  /* equivalent to "u,u,u" */
  void SetUser();

  /* general setters */
  /* read: "p, P, c, C, T, u, w" */
  void SetSSLTrust(bool peer, bool tPeer,
                   bool ca,   bool tCA, bool tClientCA,
                   bool user, bool warn); 

  void SetEmailTrust(bool peer, bool tPeer,
                     bool ca,   bool tCA, bool tClientCA,
                     bool user, bool warn);

  void SetObjSignTrust(bool peer, bool tPeer,
                       bool ca,   bool tCA, bool tClientCA,
                       bool user, bool warn);

  /* set c <--> CT */
  void AddCATrust(bool ssl, bool email, bool objSign);
  /* set p <--> P */
  void AddPeerTrust(bool ssl, bool email, bool objSign);

  /* get it (const?) (shallow?) */
  CERTCertTrust * GetTrust() { return &mTrust; }

private:
  void addTrust(unsigned int *t, unsigned int v);
  void removeTrust(unsigned int *t, unsigned int v);
  bool hasTrust(unsigned int t, unsigned int v);
  CERTCertTrust mTrust;
};

#endif
