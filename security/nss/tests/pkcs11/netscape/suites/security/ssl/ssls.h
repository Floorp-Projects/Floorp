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
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

