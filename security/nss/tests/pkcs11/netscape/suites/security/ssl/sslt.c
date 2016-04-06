/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_POINT 7
/* NSPR header files */
#include <prinit.h>
#include <prprf.h>
#include <prsystem.h>
#include <prmem.h>
#include <plstr.h>
#include <prnetdb.h>
#include <prinrval.h>
#include <prmon.h>
#include <prlock.h>

/* Security library files */
#include "cert.h"
#include "key.h"
#include "secmod.h"
#include "secutil.h"
#include "pk11func.h"

/* SSL Header Files */
#include "ssl.h"
#include "sslproto.h"

#define EXIT_OOPS 14

#include "ssls.h"
#include "sslc.h"

#ifdef XP_PC
/* Windows VC++ 6.0 Header File required to define EXCEPTION_EXECUTE_HANDLER. */
#include "excpt.h"
#endif

#ifndef DEBUG_stevep
#define dbmsg(x) if (debug) PR_fprintf x ;
#else
#define dbmsg(x) ;
#endif

/* Prototypes */

PRInt32 ServerThread(PRInt32 argc,char **argv);
void ClientThread(void *arg);
void SetupNickNames(void );
int OpenDBs(void);
int ConfigServerSocket(void);
int DoIO(struct ThreadData *);
int Client(void);
int SetClientSecParams(void);
int CreateClientSocket(void);

#ifdef XP_PC
extern char getopt(int, char**, char*);
#endif
extern int Version2Enable(PRFileDesc *s);
extern int Version3Enable(PRFileDesc *s);
extern int Version23Clear(PRFileDesc *s);
extern void SetupNickNames();
extern int AuthCertificate(void *arg,PRFileDesc *fd,
			   PRBool checkSig, PRBool isServer);
extern char *MyPWFunc(void *slot, PRBool retry, void *arg);

extern char *nicknames[];
extern char *client_nick;
extern char *password, *nickname;

/* Shared condition variables */

int rc;            /* rc is the error the process should return */
PRMonitor *rcmon;  /* rcmon protects rc, since it can be set by the client */
                   /* or server thread */

/***** Read-only global variables (initialized in Server Thread)   ****/

PRInt32 debug   = 0;
PRInt32 verbose = 0;
CERTCertDBHandle *cert_db_handle = NULL;

struct ThreadData cl,svr;

/* Include Replacer-generated variables file */

/* INSERT_TABLES is a special parameter to sslt.h which inserts the
   replacer-generated tables. We only want this table to be included
   once in the executable, but this header file gets use in several
   places */

#define INSERT_TABLES
#include "sslt.h"
#include "nss.h"



/*
 *
 * OpenDBs()   -  open databases
 * errors(30-39)
 */

int OpenDBs() {
  int r;

  NSS_Init(".");
  return 0;
}





/* 
 * CreateServerSocket
 * errors (20-29)
 */


int CreateServerSocket(struct ThreadData *td) {
  /* Create server socket s */

  td->fd = PR_NewTCPSocket();
  if (td->fd == NULL) return Error(20);

  td->r = SSL_ImportFD(NULL, td->fd);
  if (td->r == NULL) return Error(21);

  return 0;
} 


int ConfigServerSocket() {

  /* Set up Net address to bind to 'any' */
  int r;
 
  r = PR_InitializeNetAddr(PR_IpAddrAny,0,&svr.na);
  if (PR_SUCCESS != r) return Error(2);

  
  r = PR_Bind(svr.r,&svr.na);     /* bind to an IP address */
  if (PR_SUCCESS != r) return Error(3);


  r = PR_Listen(svr.r,5);
  if (PR_SUCCESS != r) return Error(4);


  r = PR_GetSockName(svr.r,&svr.na);
  if (PR_SUCCESS != r) return Error(5);
  return r;
}


/* 
 * main 
 *   returns 255 if 'coredump'-type crash occurs on winNT
 *
 */

PRIntn main(PRIntn ac, char **av, char **ev) {
  int r;
  extern char *optarg;	
  extern int optind;
  int c;
  

  if( ac == 1 ) {
     PR_fprintf(PR_STDERR,
"\nSSL Test Suite Version %d.%d.%d\n\
All Rights Reserved\n\
Usage: sslt [-c client_nickname] [-n server_nickname] [-p passwd] [-d] testid\n",
VERSION_MAJOR, VERSION_MINOR, VERSION_POINT);

    exit(0);
  }

  for (c = 1; c<ac; c++) {
	if (!PL_strcmp(av[c],"-c")) {
	
		  c++;
		  if (c < ac) {
			client_nick = av[c];
		  }
		  else {
			  PR_fprintf(PR_STDOUT,"must supply argument for -c\n");
			  exit(0);
		  }
	}

	else if (!PL_strcmp(av[c],"-n")) {
	
		  c++;
		  if (c < ac) {
			nickname = av[c];
		  }
		  else {
			  PR_fprintf(PR_STDOUT,"must supply argument for -n\n");
			  exit(0);
		  }
      }
	  else if (!PL_strcmp(av[c],"-p")) {

		  c++;
		  if (c < ac) {
			password = av[c];
		  }
		  else {
			  PR_fprintf(PR_STDOUT,"must supply argument for -p\n");
			  exit(0);
		  }
      }
	else if (!PL_strcmp(av[c],"-d")) {
		  c++;
		  debug++;
      }
	else 
		testId = atoi(av[c]);
  }



#ifdef XP_PC
    __try {
#endif
 
  r = PR_Initialize(ServerThread,ac,av,400);         /* is 400 enough? */

  /* returncode 99 means 'no error' */
  if (99 == r)  r = 0;

#ifdef XP_PC
    } __except( PR_fprintf(PR_STDERR, "\nCERT-TEST crashed\n"), EXCEPTION_EXECUTE_HANDLER ) {
        r = 255;
    }
#endif

  return r;

}



/* 
 * ServerThread
 * (errors 1-9,150-159)
 */


PRInt32 ServerThread(PRInt32 argc,char **argv) {

  PRNetAddr na;

  PRStatus r;
  SECStatus rv;
  
  CERTCertDBHandle *cert_db_handle;
  PRInt32 i,j;
  struct ThreadData * td;

  
  /*   if (InvalidTestHack() == PR_TRUE) {
    return 0;
  }
  */

  rcmon = PR_NewMonitor();
  if (NULL == rcmon) return Error(140);

  PR_EnterMonitor(rcmon);
  rc = 0;
  PR_ExitMonitor(rcmon);

  InitCiphers();
  SetPolicy();
  SetupNickNames();
  
  cl.peer = &svr;
  svr.peer = &cl;


  r = OpenDBs();            /* open databases and set defaults */
  if (PR_SUCCESS != r) return r;


  r = CreateServerSocket(&svr);
  if (PR_SUCCESS != r) return r;

  r = ConfigServerSocket();
  if (PR_SUCCESS != r) return r;

  cl.peerport = svr.na.inet.port;


  r = SetServerSecParms(&svr);  /* configure server socket
                                   sid cache, certificate etc. */
  if (r) return r;

  r = SSL_HandshakeCallback(svr.r, HandshakeCallback, &svr);
  if (PR_SUCCESS != r) return Error(150);

  r = SSL_AuthCertificateHook(svr.r,AuthCertificate,&svr);
  if (PR_SUCCESS !=r ) return Error(151);

  /* The server socket is now set up. Now, we must start
     the client thread */

  svr.subthread =
    PR_CreateThread(PR_SYSTEM_THREAD,   /* Thread Type      */
		    ClientThread,       /* Start Function   */
		    NULL,               /* Argument         */
		    PR_PRIORITY_NORMAL, /* Priority         */
		    PR_GLOBAL_THREAD,   /* Scheduling scope */
		    PR_JOINABLE_THREAD, /* Thread State     */
		    0          /* Stacksize (0=use default) */
		    );
  if (svr.subthread == NULL) return Error(6);


  
  /* Wait for incoming connection from client thread */

  svr.s = PR_Accept(svr.r, NULL, PR_SecondsToInterval(100)); /* timeout */
  if (NULL == svr.s) {
    r = PR_GetError();
    if (r) {
      return Error(7);
    }
  }

  td = &svr;
  td->client = PR_FALSE;
  td->xor_reading = CLIENTXOR;
  td->xor_writing = 0;
  
  r = DoIO(td);
  dbmsg((PR_STDERR,"Server IO complete - returned %d\n",r));
  dbmsg((PR_STDERR,"PR_GetError() = %d\n",PR_GetError()));


  /* WHY IS THIS HERE???? */
  r = 0;
  if (r) return r;
  

  /* c = SSL_PeerCertificate(s); */

  r = PR_Close(svr.s);   /* close the SSL Socket */
  if (r != PR_SUCCESS) return Error(8);

  dbmsg((PR_STDERR,"PR_Close(svr.s) - returned %d\n",r));

  r = PR_Close(svr.r);  /* Close the rendezvous socket */
  if (r != PR_SUCCESS) return Error(8);

  dbmsg((PR_STDERR,"PR_Close(svr.r) - returned %d\n",r));

  r = PR_JoinThread(svr.subthread);
  if (r != PR_SUCCESS)  return Error(9);

  PR_EnterMonitor(rcmon);
  r = rc;
  PR_ExitMonitor(rcmon);
  
  dbmsg((PR_STDERR,"Client Thread Joined. client's returncode=%d\n",r));
  dbmsg((PR_STDERR,"Server Thread closing down.\n"));

  return r;
  
  }


/*
 * Get security status for this socket
 *
 */ 

int GetSecStatus(struct ThreadData *td) {
  int r;

  r = SSL_SecurityStatus(td->s,
			 &td->status_on,
			 &td->status_cipher,
			 &td->status_keysize,
			 &td->status_skeysize,
			 &td->status_issuer,
			 &td->status_subject
			 );

  return r;
  /* SSL_PeerCertificate(); */

}
 



/* Signal an error code for the process to return.
   If the peer aborted before us, returns 0.
   If the peer did not abort before us, returns the calling argument
   (to be used as a returncode) */
int Error(int s)
{
  int r;

  PR_EnterMonitor(rcmon);
  r = rc;
  if (0 == rc) { 
    rc = s;
  }    
  PR_ExitMonitor(rcmon);

  if (r) return s;
  else return 0;
}



#define ALLOWEDBYPROTOCOL    1
#define ALLOWEDBYPOLICY      2
#define ALLOWEDBYCIPHERSUITE 4

/* This returns 0 if the status is what was expected at this point, else a returncode */


int VerifyStatus(struct ThreadData *td)
{
  int i,j;
  int matched =0;

  /* Go through all the ciphers until we find the first one that satisfies */
  /* all the criteria. The ciphers are listed in preferred order. So, the first */
  /* that matches should be the one. */

  /* because of bug 107086, I have to fudge this. If it weren't for this
     bug, SSL2 ciphers may get chosen in preference to SSL3 cipher,
     if they were stronger */


  for (i=0;i<cipher_array_size;i++) {

  /* IF */

    if (

	/* bug 107086. If SSL2 and SSL3 are enabled, ignore the SSL2 ciphers */
	(!( /* see above */
	  (REP_SSLVersion2 && REP_SSLVersion3) && cipher_array[i].sslversion == 2)
	 )

	&&


	(  /* Cipher is the right kind for the protocol? */
	 ((cipher_array[i].sslversion == 2) && REP_SSLVersion2) ||
	 ((cipher_array[i].sslversion == 3) && REP_SSLVersion3) 
	 )
	
	&&  /* Cipher is switched on */

	((cipher_array[i].on == 1) ||
	 ((cipher_array[i].on == 2) &&
	  (REP_ServerCert == SERVER_CERT_VERISIGN_STEPUP)))

	&&  /* Is this cipher enabled under this policy */
    
	(
	 (REP_Policy == POLICY_DOMESTIC) ||
	 ((REP_Policy == POLICY_EXPORT)  && 
	  (cipher_array[i].exportable == SSL_ALLOWED)))
	)

  /* THEN */
      {
	/* This is the cipher the SSL library should have chosen */
	
	matched = 1;
	break;	
      }
  }

GetSecStatus(td);


#define SSLT_STATUS_CORRECT           0 /* The status is correct. Continue with test */
#define SSLT_STATUS_WRONG_KEYSIZE     1 /* The reported keysize is incorrect. abort */
#define SSLT_STATUS_WRONG_SKEYSIZE    2 /* The reported secret keysize is incorrect. abort */
#define SSLT_STATUS_WRONG_DESCRIPTION 3 /* The reported description is incorrect. abort*/
#define SSLT_STATUS_WRONG_ERRORCODE   4 /* sec. library error - but wrong one - abort */
#define SSLT_STATUS_CORRECT_ERRORCODE 5 /* security library error - right one - abort with err 99 */

  if (matched) {
    if (td->status_keysize  != cipher_array[i].ks) {
      PR_fprintf(PR_STDERR,"wrong keysize. seclib: %d,  expected %d\n",
		 td->status_keysize,cipher_array[i].ks);
      return  SSLT_STATUS_WRONG_KEYSIZE;
    }
    if (td->status_skeysize != cipher_array[i].sks) return SSLT_STATUS_WRONG_SKEYSIZE;
    if (PL_strcmp(td->status_cipher,cipher_array[i].name)) {
      PR_fprintf(PR_STDERR,"wrong cipher description.  seclib: %s, expected: %s\n",
	     td->status_cipher,cipher_array[i].name);
      return SSLT_STATUS_WRONG_DESCRIPTION;
    }

    /* Should also check status_issuer and status_subject */
  
    return SSLT_STATUS_CORRECT;
  }

  else {
    /* if SSL wasn't enabled, security library should have returned a failure with
       SSL_ERROR_SSL_DISABLED 
       */

    /* Since we cannot set the client and server ciphersuites independently,
       there's not point in checking for NO_CYPHER_OVERLAP. That's why some
       of this is commented out.
       */

#if 0
	if (PR_FALSE == REP_SSLVersion2 &&
	    PR_FALSE == REP_SSLVersion3)
{
if ( (td->secerr_flag == PR_FALSE ) ||
          ((td->secerr_flag == PR_TRUE) && 
	     !((td->secerr == SSL_ERROR_SSL_DISABLED) ||
	      (td->secerr == SSL_ERROR_NO_CYPHER_OVERLAP))
   	)) {
       return SSLT_STATUS_WRONG_ERRORCODE;
     }
     else
  return SSLT_STATUS_CORRECT_ERRORCODE;
   }

	else {

	  /* If SSL was enabled, and we get here, then no ciphers were compatible
	     (matched == 0). So, security library should have returned the error
	     SSL_ERROR_NO_CYPHER_OVERLAP */

	  if ((td->secerr_flag == PR_FALSE) ||
	      ((td->secerr_flag == PR_TRUE) && (td->secerr != SSL_ERROR_NO_CYPHER_OVERLAP))) {
	    return SSLT_STATUS_WRONG_ERRORCODE;
	  }
	  else return SSLT_STATUS_CORRECT_ERRORCODE;
	}
#endif
  }
	return SSLT_STATUS_CORRECT_ERRORCODE;
}


/* 
 *  DoRedoHandshake()
 *
 * errors(90-99)
 *  99 means exit gracefully
 */

int DoRedoHandshake(struct ThreadData *td) {
  int r;


  /* figure out if we really should do the RedoHandshake */
  if ((td->client  && (PR_TRUE== REP_ClientRedoHandshake)) ||
	(!td->client && (PR_TRUE== REP_ServerRedoHandshake))) {

    if ((!td->client && (SSLT_CLIENTAUTH_REDO==REP_ServerDoClientAuth))) {
       r = SSL_Enable(td->s, SSL_REQUEST_CERTIFICATE, 1);
    }

    r = SSL_RedoHandshake(td->s);          /* .. and redo the handshake */      
    if (PR_SUCCESS == r) {                  /* If the handshake succeeded,  */
                                            /* make sure that shouldn't have failed... */

	/*** 
	   If the server is doing ClientAuth
           and the wrong certificate in the
	   client, then the handshake should fail (but it succeeded)
	   ***/
      
#if 0
      if (SSLT_CLIENTAUTH_INITIAL == REP_ServerDoClientAuth) {
	if ((CLIENT_CERT_SPARK == REP_ClientCert) ||
	    (SERVER_CERT_HARDCOREII_512       == REP_ClientCert) ||
	    (NO_CERT                           == REP_ClientCert)
	    ) 
	  return Error(90);

      }
#endif
      
    }
    
    else {  /* PR_FAILURE:            Make sure the handshake shouldn't have succeeded */
      
      /* First, abort the peer, since it cannot continue */
      r = Error(91);
      if (0==r) return 0;  /* peer aborted first */
      else {
      /***
	If the server is doing clientauth and
	a valid certificate was presented, the handshake
	should have succeeded (but it failed)
	***/
      
	if (PR_TRUE == REP_ServerDoClientAuth) {
	  if ((CLIENT_CERT_HARDCOREII_512         == REP_ClientCert) ||
	      (CLIENT_CERT_HARDCOREII_1024        == REP_ClientCert) ||
	      (CLIENT_CERT_VERISIGN               == REP_ClientCert) ||
	      (SERVER_CERT_HARDCOREII_512         == REP_ClientCert)
	      ) 
	    return Error(91);
	}
      }
    }
  }
}



/* There is a independent State Machine for each of client and server.
   They have the following states:

   1. STATE_BEFORE_INITIAL_HANDSHAKE
      In this state at the very start. No I/O has been done on the socket,
      and no status has been collected. Once I/O has been done, we move on
      to state 2.

   2. STATE_BEFORE_REDO_HANDSHAKE
      If we won't be doing a redohandshake, move immediately to state3.
      Check security status to make sure selected cipher is correct.
      If we are doing a redohandshake, adjust the security parameters for
      the redo, and move to state 3.
   3. STATE_STATUS_COLLECTED
      When we move to this state, check security status.
      Remain in this state until either reading or writing is complete
   4. STATE_DONE_WRITING
      Come here when writing is complete. When reading is complete move
      to state 6.
   5. STATE_DONE_READING
      Come here when reading is complete. When writing is complete move
      to state 6.
   6. STATE_DONE
      We're done. Check that the appropriate callbacks were called at the
      appropriate times.
      */
    
/* 
 * State Machine
 *
 * errors(80-89)
 */

int NextState(struct ThreadData *td,
	       int finishedReading,
	       int finishedWriting) {
  int r;



  /* if we were in STATE_BEFORE_INITIAL_HANDSHAKE, and we came here, we must
     have just completed a handshake, so we can get status and move on
     to next state.  */

  if (STATE_BEFORE_INITIAL_HANDSHAKE == td->state ) {
    
    td->state = STATE_BEFORE_REDO_HANDSHAKE;  /* first set next state */
    
    r = GetSecStatus(td);
    if (PR_SUCCESS != r) {
      return Error(80);
    }
    
#if 0
    r = VerifyStatus(td);   /* Call VerifyStatus to make sure that the connection is
			       what was expected */
    if (PR_SUCCESS != r) return r;
#endif

      
  }
  
  if (STATE_BEFORE_REDO_HANDSHAKE == td->state) {
    /* If we're not going to do a redohandshake, we can just skip over this state */
    if (td->client) {
      if (PR_FALSE  == REP_ClientRedoHandshake) td->state = STATE_STATUS_COLLECTED;
    }
    else {
      if (PR_FALSE == REP_ServerRedoHandshake) td->state = STATE_STATUS_COLLECTED;
    }
    r = DoRedoHandshake(td);
    if (PR_SUCCESS != r) return r;
    td->state = STATE_STATUS_COLLECTED;
  }
		  

  switch (td->state) {
  case STATE_STATUS_COLLECTED:
    if (finishedWriting) td->state = STATE_DONE_WRITING;
    if (finishedReading) td->state = STATE_DONE_READING;
    break;
  case STATE_DONE_WRITING:
    if (finishedReading) td->state = STATE_DONE;
    break;
  case STATE_DONE_READING:
    if (finishedWriting) td->state = STATE_DONE;
    break;
  default:
    return PR_SUCCESS;
  }
}


/* CheckSSLEnabled:
   If there was an I/O, and SSL was disabled, then check the error
   code to make sure that the correct error was returned.
   The parameter passed in is the returncode from PR_Read or PR_Write
   */

int CheckSSLEnabled(int j) {
  if (PR_FALSE == REP_SSLVersion2 &&
      PR_FALSE == REP_SSLVersion3) {
    if (( -1 != j ) ||
	(( -1 == j) && (PR_GetError() != SSL_ERROR_SSL_DISABLED))) {
      return 52;
    }
    else return 99;
  }
  else return 0;
}



/* 
 * Do I/O
 *
 * Errors 50-69
 */
 
int DoIO(struct ThreadData *td) {

int i,j,r;

  td->pd.fd        = td->s;
  td->pd.in_flags  = PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT;
  td->data_read    = 0;
  td->data_sent    = 0;

  td->data_tosend = REP_ServerIOSessionLength;
  
  td->state = STATE_BEFORE_INITIAL_HANDSHAKE;


  while (PR_TRUE) {
    dbmsg((PR_STDERR,"%s: DoIO loop\n",
	       &svr==td ? "Server" : "Client"));

    /* pd = polldescriptor, 1 = number of descriptors, 5 = timeout in seconds */
    r = PR_Poll(&td->pd,1,PR_SecondsToInterval(5));

    /* Check if peer has already signalled an error condition */ 

    PR_EnterMonitor(rcmon);
    if (0 != rc) {
      /* got here? - means peer wants to stop. It has set the
	 exit code */
      PR_ExitMonitor(rcmon);
      dbmsg((PR_STDERR,"%s: Peer has aborted (error code %d). We should too\n",
	     &svr==td ? "Server" : "Client",rc));
      
      return 0;
    }
    else {
      PR_ExitMonitor(rcmon);
    }

    if (0 == r) ;   /* timeout occurred */

    if (td->pd.out_flags & PR_POLL_EXCEPT) return Error(50);

    /*******   Process incoming data   *******/
    
    if (! (STATE_DONE == td->state || STATE_DONE_READING == td->state)) {
      if (td->pd.out_flags & PR_POLL_READ) {

	td->secerr = 0;
	i = PR_Read(td->s, td->recvbuf, BUFSIZE);

	if (i < 0) {
	  td->secerr_flag = 1;
	  td->secerr = PR_GetError();
	}
	else td->secerr_flag =0;

	r = VerifyStatus(td);

	switch (r) {
	case SSLT_STATUS_CORRECT:
	  break;
	case SSLT_STATUS_CORRECT_ERRORCODE:
	  return Error(99);
	default:
	  return Error(60+r);
	}
	
	r = VerifyBuffer(td->recvbuf, i, td->data_read, td->xor_reading);
	if (r) return r;
	td->data_read += i;
	
	/* Invoke State Machine */

	NextState(td, 0==i, 0);  /* if i is zero, signal 'finishedreading' */

      }
    }
    
    if (! (STATE_DONE == td->state || STATE_DONE_WRITING == td->state)) {
      if (td->pd.out_flags & PR_POLL_WRITE) {
	FillBuffer(td->sendbuf,BUFSIZE,td->data_sent,td->xor_writing);

	i = td->data_tosend - td->data_sent;
	if (i > BUFSIZE) i = BUFSIZE;  /* figure out how much
					  data to send */
	td->secerr = 0;
	j = PR_Write(td->s, td->sendbuf, i);


	if (j < 0) {
	  td->secerr_flag = 1;
	  td->secerr = PR_GetError();
	}
	else td->secerr_flag =0;

	r = VerifyStatus(td);

	switch (r) {
	case SSLT_STATUS_CORRECT:
	  break;
	case SSLT_STATUS_CORRECT_ERRORCODE:
	  return Error(99);
	default:
	  return Error(60+r);
	}

      }
      if (j == -1) return Error(53);        /* Error on socket (Not an error
				        if nonblocking IO enabled, and
				        Error is Would Block */
      
      if (j != i) return Error(54);         /* We didn't write the
                                        amount we should have */
      
      td->data_sent += j;
      
      if (td->data_sent == td->data_tosend) {
	PR_Shutdown(td->s,PR_SHUTDOWN_SEND);
      }
      
      /* next state of state machine */

      NextState(td,
		0,
		td->data_sent == td->data_tosend  /* finishedwriting */
		);      
    }



    if (STATE_DONE == td->state) break;

  } /* while (1) */
    
    dbmsg((PR_STDERR,"%s: DoIO loop:returning 0\n",
	       &svr==td ? "Server" : "Client"));

    return 0;
    
}




/* This is the start of the client thread code */
/* Client Thread errors(100-200) */


/* 
 * CreateClientSocket()
 * errors (120-129)
 */


int CreateClientSocket() {
  /* Create client socket s */

  cl.fd = PR_NewTCPSocket();
  if (cl.fd == NULL) return Error(120);  

  cl.s = SSL_ImportFD(NULL, cl.fd);
  if (cl.s == NULL) return Error(121);

  return 0;
}  



/* 
 * SetClientSecParms
 * errors(130-139)
 */

int SetClientSecParams()  {
  int rv;
  /* SSL Enables */
  
  rv = SSL_Enable(cl.s, SSL_SECURITY, 1);
  if (rv < 0)  return Error(130);

  rv = Version23Clear(cl.s);
  if (rv) return rv;

  if (REP_SSLVersion2) {
    rv = Version2Enable(cl.s);
    if (rv) return rv;
  }
  if (REP_SSLVersion3) {
    rv = Version3Enable(cl.s);
    if (rv) return rv;
  }

  SSL_SetPKCS11PinArg(cl.s,(void*)MyPWFunc);

  if (REP_ClientCert == NO_CERT) {
    return 0;
  }
  else {
    cl.cert = PK11_FindCertFromNickname(client_nick,NULL);
  }
  if (cl.cert == NULL) return Error(131);
  
  return 0;
}


/*
 * Client()
 * errors (100-120)
 */

int Client() {
  int r;

  r = CreateClientSocket();
  if (r) return r;

  r = SetClientSecParams();
  if (r) return r;

  /* Set address to connect to: localhost */

  r = PR_InitializeNetAddr(PR_IpAddrLoopback,0,&cl.na);
  cl.na.inet.port = cl.peerport;
  if (PR_FAILURE == r) return Error(101);

  r = SSL_AuthCertificateHook(cl.s,AuthCertificate,&cl);
  if (r) return Error(102);
  r = SSL_HandshakeCallback(cl.s,HandshakeCallback,&cl);
  if (r) return Error(103);

  r = PR_Connect(cl.s, &cl.na, PR_SecondsToInterval(50));
  if (PR_FAILURE == r) {
    dbmsg((PR_STDERR, "Client: Seclib error: %s\n",SECU_Strerror(PR_GetError())));
    return Error(104);
  }


  if (PR_TRUE == REP_ClientForceHandshake) {
    r = SSL_ForceHandshake(cl.s);
    if (PR_FAILURE == r) {
      dbmsg((PR_STDERR, "Client: Seclib error: %s\n",
	SECU_Strerror(PR_GetError())));
      return Error(105);
    }
  }

  cl.client = PR_TRUE;
  cl.xor_reading = 0;
  cl.xor_writing = CLIENTXOR;
  
  r = DoIO(&cl);

  dbmsg((PR_STDERR,"Client Thread done with IO. Returned %d\n",r));


  if (PR_SUCCESS != r) return r;

  r = PR_Close(cl.s);

  dbmsg((PR_STDERR,"Client Socket closing. Returned %d\n",r));

  return Error(r);
  
}



 void ClientThread(void *arg) {
   int r;

   Error(Client());

   dbmsg((PR_STDERR,"Client Thread returning %d\n",r));
   
   
 }

   




 /* VerifyBuffer() */

/* verify the data in the buffer. Returns 0 if valid  */
/* recvbuf = start of data to verify
 * bufsize = amount of data to verify
 * done    = how to offset the reference data. How much 
             data we have done in previous sessions
 * xor     = xor character 

 * errors 70-79

 */
 
 int VerifyBuffer(char *recvbuf,int bufsize,int done, char xor) {
  int i,j,k;

  while (bufsize) {
    i = done % DATABUFSIZE;

    k = DATABUFSIZE;
    if (bufsize < k) {
      k = bufsize;
    }
    for (j = i; j < k ; j++) {
      if ((data[j] ^ xor) != (*recvbuf)) {
	return 71;
      }
      
      recvbuf++;
    }
    done += k-i;
    bufsize -= (k - i);
    if (bufsize < 0) return 73;
  }
  return (0);
}


/* fill the buffer.  */

 void FillBuffer(char *sendbuf,int bufsize, int offset, char xor) {
   int done=0,i,j;
   
   while (done < bufsize) {
    i = offset % DATABUFSIZE;
    for (j = i; j < DATABUFSIZE ; j++) {
      *sendbuf = (data[j] ^ xor);
      sendbuf++;
    }
    done += (DATABUFSIZE - i);
    offset += (DATABUFSIZE - i);
   }
 }
 
 
 
 
/******     CALLBACKS      *******/
 


/* HandshakeCallback
   This function gets called when a handshake has just completed.
   (maybe gets called more than once for example if we RedoHandshake)
   */

 void HandshakeCallback(PRFileDesc *s, void *td)   {
   int r;

   /*   1. Get status of connection */

   r = GetSecStatus(td);
   if (PR_SUCCESS != r) {
     /* Abort */
   }
   else {

   /*   2. Verify status of connection */

#if 0   
  r =VerifyStatus(td); 
     if (PR_SUCCESS != r) {
       /* Abort */
     }
#endif
   }

 }
 


/* This function gets called by the client thread's SSL code to verify
   the server's certificate. We cannot use the default AuthCertificate
   code because the certificates are used on multiple hosts, so
   CERT_VerifyCertNow() would fail with an IP address mismatch error
   */

int
AuthCertificate(void *arg, PRFileDesc *fd, PRBool checkSig, PRBool isServer)
{
    SECStatus rv;
    CERTCertDBHandle *handle;
    /*    PRFileDesc *ss; */
    SECCertUsage certUsage;
    
    /*     ss = ssl_FindSocket(fd);
    PORT_Assert(ss != NULL); */

    handle = (CERTCertDBHandle *)arg;

    if ( isServer ) {
	certUsage = certUsageSSLClient;
    } else {
	certUsage = certUsageSSLServer;
    }
    
    /*     rv = CERT_VerifyCertNow(handle, ss->sec->peerCert, checkSig, certUsage, arg); */

    return((int)PR_SUCCESS);
}








