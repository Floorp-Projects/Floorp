/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* include replacer-generated variables file */


#include "ssl.h"
#include "sslproto.h"

#include "sslt.h"
#include "sslc.h"
#include "ssls.h"

#include "pk11func.h"

#define MAX_CIPHERS 100

struct cipherspec cipher_array[MAX_CIPHERS];
int cipher_array_size=0;
char *password = "";
char *nickname = "SSLServer";
char *client_nick = "SSLClient";

void InitCiphers() {
  int i=0;

/* These ciphers are listed in priority order. */
  DIPHER(2,SSL_ALLOWED,128,40,     "RC2-CBC-Export", EN_RC2_128_CBC_EXPORT40_WITH_MD5)
    CIPHER(2,SSL_NOT_ALLOWED,128,128,"RC4",            EN_RC4_128_WITH_MD5)
    CIPHER(2,SSL_ALLOWED,128,40,     "RC4-Export",     EN_RC4_128_EXPORT40_WITH_MD5)
    DIPHER(2,SSL_NOT_ALLOWED,128,128,"RC2-CBC",        EN_RC2_128_CBC_WITH_MD5)
    DIPHER(2,SSL_ALLOWED,128,40,     "RC2-CBC-40", EN_RC2_128_CBC_EXPORT40_WITH_MD5)
    DIPHER(2,SSL_NOT_ALLOWED,128,128,"IDEA-CBC",       EN_IDEA_128_CBC_WITH_MD5)
    DIPHER(2,SSL_NOT_ALLOWED,56,56,  "DES-CBC",        EN_DES_64_CBC_WITH_MD5)
    CIPHER(2,SSL_NOT_ALLOWED,168,168,"DES-EDE3-CBC",   EN_DES_192_EDE3_CBC_WITH_MD5)
  /* SSL 3 suites */

    CIPHER(3,SSL_RESTRICTED,128,128, "RC4",            RSA_WITH_RC4_128_MD5)
    DIPHER(3,SSL_RESTRICTED,128,128, "RC4",            RSA_WITH_RC4_128_SHA)
    CIPHER(3,SSL_RESTRICTED,168,168, "3DES-EDE-CBC",   RSA_WITH_3DES_EDE_CBC_SHA)
    CIPHER(3,SSL_NOT_ALLOWED,56,56,"DES-CBC",        RSA_WITH_DES_CBC_SHA)
    CIPHER(3,SSL_ALLOWED,128,40,     "RC4-40",         RSA_EXPORT_WITH_RC4_40_MD5)
    CIPHER(3,SSL_ALLOWED,128,40,     "RC2-CBC-40",     RSA_EXPORT_WITH_RC2_CBC_40_MD5)

    DIPHER(3,SSL_ALLOWED,0,0,        "NULL",           NULL_WITH_NULL_NULL)
    DIPHER(3,SSL_ALLOWED,0,0,        "NULL",           RSA_WITH_NULL_MD5)
    DIPHER(3,SSL_ALLOWED,0,0,        "NULL",           RSA_WITH_NULL_SHA)

#if 0
    DIPHER(3,SSL_NOT_ALLOWED,0,0,    "IDEA-CBC",       RSA_WITH_IDEA_CBC_SHA)
    DIPHER(3,SSL_ALLOWED,128,40,     "DES-CBC-40",     RSA_EXPORT_WITH_DES40_CBC_SHA)
#endif

  /*
    
  CIPHER(DH_DSS_EXPORT_WITH_DES40_CBC_SHA),
  CIPHER(DH_DSS_WITH_DES_CBC_SHA),
  CIPHER(DH_DSS_WITH_3DES_EDE_CBC_SHA),
  CIPHER(DH_RSA_EXPORT_WITH_DES40_CBC_SHA),
  CIPHER(DH_RSA_WITH_DES_CBC_SHA),
  CIPHER(DH_RSA_WITH_3DES_EDE_CBC_SHA),
  CIPHER(DHE_DSS_EXPORT_WITH_DES40_CBC_SHA),
  CIPHER(DHE_DSS_WITH_DES_CBC_SHA),
  CIPHER(DHE_DSS_WITH_3DES_EDE_CBC_SHA),
  CIPHER(DHE_RSA_EXPORT_WITH_DES40_CBC_SHA),
  CIPHER(DHE_RSA_WITH_DES_CBC_SHA),
  CIPHER(DHE_RSA_WITH_3DES_EDE_CBC_SHA),

  CIPHER(DH_ANON_EXPORT_WITH_RC4_40_MD5),
  CIPHER(DH_ANON_WITH_RC4_128_MD5),
  CIPHER(DH_ANON_WITH_DES_CBC_SHA),
  CIPHER(DH_ANON_WITH_3DES_EDE_CBC_SHA),

  CIPHER(3,SSL_NOT_ALLOWED,0,0,"Fortezza",        FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA),
  CIPHER(3,SSL_NOT_ALLOWED,0,0,"Fortezza",        FORTEZZA_DMS_WITH_RC4_128_SHA),

  */

    DIPHER(3,SSL_NOT_ALLOWED,192,192,"3DES-EDE-CBC",RSA_FIPS_WITH_3DES_EDE_CBC_SHA)
    DIPHER(3,SSL_NOT_ALLOWED,64,64,  "DES-CBC",       RSA_FIPS_WITH_DES_CBC_SHA)
    
    cipher_array_size =i;
}



/* ClearCiphers()
 *   Clear out all ciphers */

void ClearCiphers(struct ThreadData *td) {
int i;

for (i=0;i<cipher_array_size;i++) {
SSL_EnableCipher(cipher_array[i].enableid,0);
}
}


/* EnableCiphers
 *   enable only those ciphers set for this test */

void EnableCiphers(struct ThreadData *td) {
  int i;

  for (i=0;i<cipher_array_size;i++) {
    if (cipher_array[i].on) {
      SSL_EnableCipher(cipher_array[i].enableid,1);
    }
  }
}

/* SetPolicy */

void SetPolicy() {
  int i;

  for (i=0;i<cipher_array_size;i++) {
    if (REP_Policy == POLICY_DOMESTIC) {
      SSL_SetPolicy(cipher_array[i].enableid,SSL_ALLOWED);
    }
    else {
      SSL_SetPolicy(cipher_array[i].enableid,cipher_array[i].exportable);
    }
  }
}

char *MyPWFunc(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    static PRBool called=PR_FALSE;
    if(called) {
	return NULL;
    } else {
	called = PR_TRUE;
	return PL_strdup(password);
    }
}

/* 
 * VersionEnables
 *  errors (40-49)
 */

int Version2Enable(PRFileDesc *s, int v) {
  if (SSL_Enable(s, SSL_ENABLE_SSL2, 1) <0) return Error(43);
  else return 0;
}

int Version3Enable(PRFileDesc *s) {
    if (SSL_Enable(s, SSL_ENABLE_SSL3, 1) <0) return Error(42);
    else return 0;
}

int Version23Clear(PRFileDesc *s) {
  if (SSL_Enable(s,SSL_ENABLE_SSL2,0) <0) return Error(40);
  if (SSL_Enable(s,SSL_ENABLE_SSL3,0) <0) return Error(41);
  return 0;
}



char *nicknames[MAX_NICKNAME];

void SetupNickNames() {
  nicknames[CLIENT_CERT_VERISIGN]        = "CLIENT_CERT_VERISIGN";
  nicknames[CLIENT_CERT_HARDCOREII_1024] = "CLIENT_CERT_HARDCOREII_1024";
  nicknames[CLIENT_CERT_HARDCOREII_512]  = "CLIENT_CERT_HARDCOREII_512";
  nicknames[CLIENT_CERT_SPARK]           = "CLIENT_CERT_SPARK";
  nicknames[SERVER_CERT_HARDCOREII_512]  = nickname;
  /* nicknames[SERVER_CERT_HARDCOREII_512]  = "SERVER_CERT_HARDCOREII_512"; */
  nicknames[SERVER_CERT_VERISIGN_REGULAR]= "SERVER_CERT_VERISIGN_REGULAR";
  nicknames[SERVER_CERT_VERISIGN_STEPUP] = "SERVER_CERT_VERISIGN_STEPUP";
  nicknames[SERVER_CERT_SPARK]           = "SERVER_CERT_SPARK";
}







/* 
 * SetServerSecParms
 * errors(10-19)
 */

int SetServerSecParms(struct ThreadData *td) {
  int rv;
  SECKEYPrivateKey *privKey;
  PRFileDesc *s;

  s = td->r;

  rv = SSL_Enable(s, SSL_SECURITY, 1);     /* Enable security on this socket */
  if (rv < 0)  return Error(10);

  if (SSLT_CLIENTAUTH_INITIAL == REP_ServerDoClientAuth) {
    rv = SSL_Enable(s, SSL_REQUEST_CERTIFICATE, 1);
    if (rv < 0)  return Error(11);
    }

  ClearCiphers(td);
  EnableCiphers(td);

  PK11_SetPasswordFunc(MyPWFunc);
  SSL_SetPKCS11PinArg(s,(void*) MyPWFunc);


  /* Find the certificates we are going to use from the database */


  /* Test for dummy certificate, which shouldn't exist */
  td->cert = PK11_FindCertFromNickname("XXXXXX_CERT_HARDCOREII_1024",NULL);
  if (td->cert != NULL) return Error(16);


  td->cert = NULL;
  if (NO_CERT != REP_ServerCert) {
    td->cert = PK11_FindCertFromNickname(nicknames[REP_ServerCert],NULL);
  }


  /* Note: if we're set to use NO_CERT as the server cert, then we'll
   * just essentially skip the rest of this (except for session ID cache setup)
   */

  
  if ( (NULL == td->cert)  && ( NO_CERT != REP_ServerCert )) {
    PR_fprintf(PR_STDERR, "Can't find certificate %s\n", nicknames[REP_ServerCert]);
    PR_fprintf(PR_STDERR, "Server: Seclib error: %s\n", SECU_Strerror(PR_GetError()));
    return Error(12);
  }
  

  if ((NO_CERT != REP_ServerCert)) {
    privKey = PK11_FindKeyByAnyCert(td->cert, NULL);
    if (privKey == NULL) {
      dbmsg((PR_STDERR, "Can't find key for this certificate\n"));
      return Error(13);
    }
    
    rv = SSL_ConfigSecureServer(s,td->cert,privKey, kt_rsa);
    if (rv != PR_SUCCESS) {
      dbmsg((PR_STDERR, "Can't config server error(%d) \n",rv));
      return Error(14);
    }
  }
  
  rv = SSL_ConfigServerSessionIDCache(10, 0, 0, ".");
  if (rv != 0) {    
    dbmsg((PR_STDERR, "Can't config server session ID cache (%d) \n",rv));
    return Error(15);
  }

  return 0;
}







