/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SSLS_H
#define SSLS_H

#include <prinit.h>
#include <prprf.h>
#include <prsystem.h>
#include <prmem.h>
#include <plstr.h>
#include <prnetdb.h>
#include <prinrval.h>


#include <cert.h>

extern struct CipherPolicy ciphers[];
extern struct CipherPair policy[];

extern unsigned char data[];

#define BUFSIZE 3955   /* some arbitrary size not a multiple of 2^x */

struct ThreadData {     /* place to put thread-local data. */

  PRFileDesc *fd;             /* NSPR File Desc        */
  PRFileDesc *s;             /* The secure File Desc  */
  PRFileDesc *r;             /* Rendezvous socket (not used right now */
  PRPollDesc pd;
  CERTCertificate  *cert;
  CERTCertificate  *peercert;

  struct ThreadData *peer;

  PRNetAddr  na;
  PRThread   *subthread;
  
  int   peerport;
  int   client;

  char  sendbuf[BUFSIZE];
  char  recvbuf[BUFSIZE];
  int   data_read;
  int   data_sent;
  int   data_tosend;
  int   state;
  unsigned char  xor_reading;
  unsigned char  xor_writing;
  
  int   exit_code;
  int   secerr_flag;
  int   secerr;


#define SSLT_INITIAL_FORCE 1
#define SSLT_FIRST_IO      2
#define SSLT_REDO          4

  int   status_on;
  char *status_cipher;
  int   status_keysize;
  int   status_skeysize;
  char *status_issuer;
  char *status_subject;

};


#define POLICY_DOMESTIC 0
#define POLICY_EXPORT 1


extern int VerifyBuffer(char *recvbuf,int bufsize,int done, char xor);
extern void FillBuffer(char *sendbuf,int bufsize, int offset, char xor);
extern void HandshakeCallback(PRFileDesc *s, void *td);


#define DATABUFSIZE 168
#define CLIENTXOR   0xA5

#define BLOCKING      0
#define NON_BLOCKING  1

#define STATE_BEFORE_INITIAL_HANDSHAKE  0
#define STATE_BEFORE_REDO_HANDSHAKE     1
#define STATE_STATUS_COLLECTED          2
#define STATE_DONE_WRITING              3
#define STATE_DONE_READING              4
#define STATE_DONE                      5

#define SSLT_CLIENTAUTH_OFF     1
#define SSLT_CLIENTAUTH_REDO    2
#define SSLT_CLIENTAUTH_INITIAL 3


#endif

