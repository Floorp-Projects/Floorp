/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifdef SSLTELNET
#include <termios.h>
#endif

/* Portable layer header files */
#include "prinit.h"
#include "prprf.h"
#include "prsystem.h"
#include "prmem.h"
#include "plstr.h"
#include "prnetdb.h"
#include "prinrval.h"

#include "secutil.h"

/* Security library files */
#include "cert.h"
#include "ssl.h"
#include "sslproto.h"
#include "secmod.h"
#include "nss.h"

/* define this if you want telnet capability! */

/* #define SSLTELNET 1 */

PRInt32 debug;

#ifdef DEBUG_stevep
#define dbmsg(x) if (verbose) PR_fprintf(PR_STDOUT,x);
#else
#define dbmsg(x) ;
#endif


/* Set SSL Policy to Domestic (strong=1) or Export (strong=0) */

#define ALLOW(x) SSL_CipherPolicySet(x,SSL_ALLOWED); SSL_CipherPrefSetDefault(x,1);
#define DISALLOW(x) SSL_CipherPolicySet(x,SSL_NOT_ALLOWED); SSL_CipherPrefSetDefault(x,0);
#define MAYBEALLOW(x) SSL_CipherPolicySet(x,SSL_RESTRICTED); SSL_CipherPrefSetDefault(x,1);

struct CipherPolicy {
  char number;
  long id;
  char *name;
  PRInt32 pref;
  PRInt32 domestic;
  PRInt32 export;
};

struct CipherPolicy ciphers[] = {
  { 'a',SSL_EN_RC4_128_WITH_MD5,              "SSL_EN_RC4_128_WITH_MD5              (ssl2)",1, SSL_ALLOWED,SSL_NOT_ALLOWED },
  { 'b',SSL_EN_RC2_128_CBC_WITH_MD5,          "SSL_EN_RC2_128_CBC_WITH_MD5          (ssl2)",1, SSL_ALLOWED,SSL_NOT_ALLOWED },  
  { 'c',SSL_EN_DES_192_EDE3_CBC_WITH_MD5,     "SSL_EN_DES_192_EDE3_CBC_WITH_MD5     (ssl2)",1, SSL_ALLOWED,SSL_NOT_ALLOWED },
  { 'd',SSL_EN_DES_64_CBC_WITH_MD5,           "SSL_EN_DES_64_CBC_WITH_MD5           (ssl2)",1, SSL_ALLOWED,SSL_NOT_ALLOWED },
  { 'e',SSL_EN_RC4_128_EXPORT40_WITH_MD5,     "SSL_EN_RC4_128_EXPORT40_WITH_MD5     (ssl2)",1, SSL_ALLOWED,SSL_ALLOWED },
  { 'f',SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5, "SSL_EN_RC2_128_CBC_EXPORT40_WITH_MD5 (ssl2)",1, SSL_ALLOWED,SSL_ALLOWED },
#ifdef FORTEZZA
  { 'g',SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA, "SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA",1,SSL_ALLOWED,SSL_NOT_ALLOWED },
  { 'h',SSL_FORTEZZA_DMS_WITH_RC4_128_SHA, "SSL_FORTEZZA_DMS_WITH_RC4_128_SHA",1,          SSL_ALLOWED,SSL_NOT_ALLOWED },
#endif
  { 'i',SSL_RSA_WITH_RC4_128_MD5,             "SSL_RSA_WITH_RC4_128_MD5             (ssl3)",1, SSL_ALLOWED,SSL_RESTRICTED },
  { 'j',SSL_RSA_WITH_3DES_EDE_CBC_SHA,        "SSL_RSA_WITH_3DES_EDE_CBC_SHA        (ssl3)",1, SSL_ALLOWED,SSL_RESTRICTED },
  { 'k',SSL_RSA_WITH_DES_CBC_SHA,             "SSL_RSA_WITH_DES_CBC_SHA             (ssl3)",1, SSL_ALLOWED,SSL_NOT_ALLOWED },
  { 'l',SSL_RSA_EXPORT_WITH_RC4_40_MD5,       "SSL_RSA_EXPORT_WITH_RC4_40_MD5       (ssl3)",1, SSL_ALLOWED,SSL_ALLOWED },
  { 'm',SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,   "SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5   (ssl3)",1, SSL_ALLOWED,SSL_ALLOWED },
#ifdef FORTEZZA
  { 'n',SSL_FORTEZZA_DMS_WITH_NULL_SHA, "SSL_FORTEZZA_DMS_WITH_NULL_SHA",1, SSL_ALLOWED,SSL_NOT_ALLOWED },
#endif
  { 'o',SSL_RSA_WITH_NULL_MD5,                "SSL_RSA_WITH_NULL_MD5                (ssl3)",1, SSL_ALLOWED,SSL_ALLOWED },
  { 'p',SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA,   "SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA   (ssl3)",1, SSL_ALLOWED,SSL_NOT_ALLOWED },
  { 'q',SSL_RSA_FIPS_WITH_DES_CBC_SHA,        "SSL_RSA_FIPS_WITH_DES_CBC_SHA        (ssl3)",1, SSL_ALLOWED,SSL_NOT_ALLOWED }

};

void PrintErrString(char *progName,char *msg) {
  
  PRErrorCode e = PORT_GetError();
  char *s=NULL;


  if ((e >= PR_NSPR_ERROR_BASE) && (e < PR_MAX_ERROR)) {
    
    if (e == PR_DIRECTORY_LOOKUP_ERROR) 
      s = PL_strdup("Hostname Lookup Failed");
    else if (e == PR_NETWORK_UNREACHABLE_ERROR)
      s = PL_strdup("Network Unreachable");
    else if (e == PR_CONNECT_TIMEOUT_ERROR)
      s = PL_strdup("Connection Timed Out");
    else s = PR_smprintf("%d",e);

    if (!s) return;
  }
  else {
    s = PL_strdup(SECU_ErrorString(e));
  }
    
  PR_fprintf(PR_STDOUT,"%s: ",progName);
  if (s) {
    if (*s) 
      PR_fprintf(PR_STDOUT, "%s\n", s);
    else
      PR_fprintf(PR_STDOUT, "\n");
    
    PR_Free(s);
  }
  
}

void PrintCiphers(int onlyenabled) {
  int ciphercount,i;
  
  if (onlyenabled) {
    PR_fprintf(PR_STDOUT,"Your Cipher preference:\n");
  }

  ciphercount = sizeof(ciphers)/sizeof(struct CipherPolicy);
    PR_fprintf(PR_STDOUT,
	       " %s    %-45s  %-12s %-12s\n","id","CipherName","Domestic","Export");

  for (i=0;i<ciphercount;i++) {
    if ( (onlyenabled ==0) || ((onlyenabled==1)&&(ciphers[i].pref))) {
      PR_fprintf(PR_STDOUT,
		 " %c     %-45s  %-12s %-12s\n",ciphers[i].number,ciphers[i].name,
		 (ciphers[i].domestic==SSL_ALLOWED)?"Yes":
		 ( (ciphers[i].domestic==SSL_NOT_ALLOWED)?"No":"Step-up only"),
		 (ciphers[i].export==SSL_ALLOWED)?"Yes":
		 ( (ciphers[i].export==SSL_NOT_ALLOWED)?"No":"Step-up only"));
	}
  }
}


void SetPolicy(char *c,int policy) {  /* policy==1 : domestic,   policy==0, export */
  int i,j,cpolicy;
  /* first, enable all relevant ciphers according to policy */
  for (j=0;j<(sizeof(ciphers)/sizeof(struct CipherPolicy));j++) {
    SSL_CipherPolicySet(ciphers[j].id,policy?ciphers[j].domestic:ciphers[j].export);
    SSL_CipherPrefSetDefault(ciphers[j].id, PR_FALSE);
    ciphers[j].pref =0;
  }
  

  for (i=0;i<(int)PL_strlen(c);i++) {
    for (j=0;j<(sizeof(ciphers)/sizeof(struct CipherPolicy));j++) {
      if (ciphers[j].number == c[i]) {
	cpolicy = policy?ciphers[j].domestic:ciphers[j].export;
	if (cpolicy == SSL_NOT_ALLOWED) {
	  PR_fprintf(PR_STDOUT, "You're trying to enable a cipher (%c:%s) outside of your policy. ignored\n",
		     c[i],ciphers[j].name);
	}
	else {
	  ciphers[j].pref=1;
	  SSL_CipherPrefSetDefault(ciphers[j].id, PR_TRUE);
	}
      }
    }
  }
}


int MyAuthCertificateHook(void *arg, PRFileDesc *fd, PRBool checksig, PRBool isserver) {
  return SECSuccess;
}


void Usage() {
#ifdef SSLTELNET
    PR_fprintf(PR_STDOUT,"SSLTelnet ");
#else
    PR_fprintf(PR_STDOUT,"SSLStrength (No telnet functionality) ");
#endif
    PR_fprintf(PR_STDOUT,"Version 1.5\n");
 
    PR_fprintf(PR_STDOUT,"Usage:\n   sslstrength hostname[:port] [ciphers=xyz] [certdir=x] [debug] [verbose] "
#ifdef SSLTELNET
"[telnet]|[servertype]|[querystring=<string>] "
#endif
"[policy=export|domestic]\n   sslstrength ciphers\n");
}


PRInt32 debug = 0;
PRInt32 verbose = 0;

PRInt32 main(PRInt32 argc,char **argv, char **envp)
{

  
  /* defaults for command line arguments */
  char *hostnamearg=NULL;
  char *portnumarg=NULL;
  char *sslversionarg=NULL;
  char *keylenarg=NULL;
  char *certdir=NULL;
  char *hostname;
  char *nickname=NULL;
  char *progname=NULL;
  /*  struct sockaddr_in addr; */
  PRNetAddr addr;

  int ss_on;
  char *ss_cipher;
  int ss_keysize;
  int ss_secretsize;
  char *ss_issuer;
  char *ss_subject;
  int policy=1;
  char *set_ssl_policy=NULL;
  int print_ciphers=0;
  
  char buf[10];
  char netdbbuf[PR_NETDB_BUF_SIZE];
  PRHostEnt hp;
  PRStatus r;
  PRNetAddr na;
  SECStatus rv;
  int portnum=443;   /* default https: port */
  PRFileDesc *s,*fd;
  
  CERTCertDBHandle *handle;
  CERTCertificate *c;
  PRInt32 i;
#ifdef SSLTELNET
  struct termios tmp_tc;
  char cb;
  int prev_lflag,prev_oflag,prev_iflag;
  int t_fin,t_fout;
  int servertype=0, telnet=0;
  char *querystring=NULL;
#endif

  debug = 0;

  progname = (char *)PL_strrchr(argv[0], '/');
  progname = progname ? progname+1 : argv[0];

  /* Read in command line args */
  if (argc == 1) {
    Usage();
    return(0);
  }
  
  if (! PL_strcmp("ciphers",argv[1])) {
    PrintCiphers(0);
    exit(0);
  }

  hostname = argv[1];

  if (!PL_strcmp(hostname , "usage") || !PL_strcmp(hostname, "-help") ) {
    Usage();
    exit(0);
    }

  if ((portnumarg = PL_strchr(hostname,':'))) {
    *portnumarg = 0;
    portnumarg = &portnumarg[1];
  }
  
  if (portnumarg) {
    if (*portnumarg == 0) {
      PR_fprintf(PR_STDOUT,"malformed port number supplied\n");
      return(1);
    }
    portnum = atoi(portnumarg);
  }
  
  for (i = 2  ; i < argc; i++)
    {
      if (!PL_strncmp(argv[i] , "sslversion=",11) )
	sslversionarg=&(argv[i][11]);
      else if (!PL_strncmp(argv[i], "certdir=",8) )
	certdir = &(argv[i][8]);
      else if (!PL_strncmp(argv[i], "ciphers=",8) )
	{
	  set_ssl_policy=&(argv[i][8]);
	}
      else if (!PL_strncmp(argv[i], "policy=",7) ) {
	if (!PL_strcmp(&(argv[i][7]),"domestic")) policy=1;
	else if (!PL_strcmp(&(argv[i][7]),"export")) policy=0;
	else {
	  PR_fprintf(PR_STDOUT,"sslstrength: invalid argument. policy must be one of (domestic,export)\n");
	}
      }
      else if (!PL_strcmp(argv[i] , "debug") )
	debug = 1;
#ifdef SSLTELNET
      else if (!PL_strcmp(argv[i] , "telnet") )
	telnet = 1;
      else if (!PL_strcmp(argv[i] , "servertype") )
	servertype = 1;
      else if (!PL_strncmp(argv[i] , "querystring=",11) )
	querystring = &argv[i][12];
#endif
      else if (!PL_strcmp(argv[i] , "verbose") )
	verbose = 1;
    }
  
#ifdef SSLTELNET
  if (telnet && (servertype || querystring)) {
    PR_fprintf(PR_STDOUT,"You can't use telnet and (server or querystring) options at the same time\n");
    exit(1);
  }
#endif

  PR_fprintf(PR_STDOUT,"Using %s policy\n",policy?"domestic":"export");
  
  /* allow you to set env var SSLDIR to set the cert directory */
  if (! certdir) certdir = SECU_DefaultSSLDir();  

  /* if we don't have one still, initialize with no databases */
  if (!certdir) {
    rv = NSS_NoDB_Init(NULL);

    (void) SECMOD_AddNewModule("Builtins", DLL_PREFIX"nssckbi."DLL_SUFFIX,0,0);
  } else {
    rv = NSS_Init(certdir);
    SECU_ConfigDirectory(certdir);
  }
  
  /* Lookup host */
  r = PR_GetHostByName(hostname,netdbbuf,PR_NETDB_BUF_SIZE,&hp);
  
  if (r) {
    PrintErrString(progname,"Host Name lookup failed");
    return(1);
  }
  
  /* should the third field really be 0? */

  PR_EnumerateHostEnt(0,&hp,0,&na);
  PR_InitializeNetAddr(PR_IpAddrNull,portnum,&na);

  PR_fprintf(PR_STDOUT,"Connecting to %s:%d\n",hostname, portnum);
  
  /* Create socket */

  fd = PR_NewTCPSocket();
  if (fd == NULL) {
    PrintErrString(progname, "error creating socket");
    return -1;
  }

  s = SSL_ImportFD(NULL,fd);
  if (s == NULL) {
    PrintErrString(progname, "error creating socket");
    return -1;
  }
  
  dbmsg("10: About to enable security\n");
  
  rv = SSL_OptionSet(s, SSL_SECURITY, PR_TRUE);
  if (rv < 0) {
    PrintErrString(progname, "error enabling socket");
    return -1;
  }
  
  if (set_ssl_policy) {
    SetPolicy(set_ssl_policy,policy);
  }
  else {
    PR_fprintf(PR_STDOUT,"Using all ciphersuites usually found in client\n");
    if (policy) {
      SetPolicy("abcdefghijklmnopqrst",policy);
    }
    else {
      SetPolicy("efghijlmo",policy);
    }
  }

  PrintCiphers(1);

  rv = SSL_OptionSet(s, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
  if (rv < 0) {
    PrintErrString(progname, "error enabling client handshake");
    return -1;
  }
  
  dbmsg("30: About to set AuthCertificateHook\n");
  
  
  SSL_AuthCertificateHook(s, MyAuthCertificateHook, (void *)handle);
  /* SSL_AuthCertificateHook(s, SSL_AuthCertificate, (void *)handle); */
  /* SSL_GetClientAuthDataHook(s, GetClientAuthDataHook, (void *)nickname);*/
  
  
  dbmsg("40: About to SSLConnect\n");
  
  /* Try to connect to the server */
  /* now SSL_Connect takes new arguments. */
  
  
  r = PR_Connect(s, &na, PR_TicksPerSecond()*5);
  if (r < 0) {
    PrintErrString(progname, "unable to connect");
    return -1;
  }
  
  rv = SSL_ForceHandshake(s);
  
  if (rv) {
    PrintErrString(progname,"SSL Handshake failed. ");
    exit(1);
  }

  rv = SSL_SecurityStatus(s, &ss_on, &ss_cipher,
			  &ss_keysize, &ss_secretsize,
			  &ss_issuer, &ss_subject);

  
  dbmsg("60:  done with security status, about to print\n");
  
  c = SSL_PeerCertificate(s);
  if (!c) PR_fprintf(PR_STDOUT,"Couldn't retrieve peers Certificate\n");
  PR_fprintf(PR_STDOUT,"SSL Connection Status\n",rv);
  
  PR_fprintf(PR_STDOUT,"   Cipher:          %s\n",ss_cipher);
  PR_fprintf(PR_STDOUT,"   Key Size:        %d\n",ss_keysize);
  PR_fprintf(PR_STDOUT,"   Secret Key Size: %d\n",ss_secretsize);
  PR_fprintf(PR_STDOUT,"   Issuer:          %s\n",ss_issuer);
  PR_fprintf(PR_STDOUT,"   Subject:         %s\n",ss_subject);

  PR_fprintf(PR_STDOUT,"   Valid:           from %s to %s\n",
	     c==NULL?"???":DER_TimeChoiceDayToAscii(&c->validity.notBefore),
	     c==NULL?"???":DER_TimeChoiceDayToAscii(&c->validity.notAfter));

#ifdef SSLTELNET

 


  if (servertype || querystring) {
    char buffer[1024];
    char ch;
    char qs[] = "HEAD / HTTP/1.0";




    if (!querystring) querystring = qs;
    PR_fprintf(PR_STDOUT,"\nServer query mode\n>>Sending:\n%s\n",querystring);

    PR_fprintf(PR_STDOUT,"\n*** Server said:\n");
    ch = querystring[PL_strlen(querystring)-1];
    if (ch == '"' || ch == '\'') {
      PR_fprintf(PR_STDOUT,"Warning: I'm not smart enough to cope with quotes mid-string like that\n");
    }
    
    rv = PR_Write(s,querystring,PL_strlen(querystring));
    if ((rv < 1) ) {
      PR_fprintf(PR_STDOUT,"Oh dear - couldn't send servertype query\n");
      goto closedown;
    }

    rv = PR_Write(s,"\r\n\r\n",4);
    rv = PR_Read(s,buffer,1024);
    if ((rv < 1) ) {
      PR_fprintf(PR_STDOUT,"Oh dear - couldn't read server repsonse\n");
      goto closedown;
    }
      PR_Write(PR_STDOUT,buffer,rv);
  }

    
  if (telnet) {

    PR_fprintf(PR_STDOUT,"---------------------------\n"
	       "telnet mode. CTRL-C to exit\n"
	       "---------------------------\n");
    


    /* fudge terminal attributes */
    t_fin = PR_FileDesc2NativeHandle(PR_STDIN);
    t_fout = PR_FileDesc2NativeHandle(PR_STDOUT);
    
    tcgetattr(t_fin,&tmp_tc);
    prev_lflag = tmp_tc.c_lflag;
    prev_oflag = tmp_tc.c_oflag;
    prev_iflag = tmp_tc.c_iflag;
    tmp_tc.c_lflag &= ~ECHO;
    /*    tmp_tc.c_oflag &= ~ONLCR; */
    tmp_tc.c_lflag &= ~ICANON;
    tmp_tc.c_iflag &= ~ICRNL;
    tmp_tc.c_cflag |= CS8;
    tmp_tc.c_cc[VMIN] = 1;
    tmp_tc.c_cc[VTIME] = 0;
    
    tcsetattr(t_fin, TCSANOW, &tmp_tc);
    /*   ioctl(tin, FIONBIO, (char *)&onoff); 
	 ioctl(tout, FIONBIO, (char *)&onoff);*/
    
    
    {
      PRPollDesc pds[2];
      char buffer[1024];
      int amt,amtwritten;
      char *x;
      
      /* STDIN */
      pds[0].fd = PR_STDIN;
      pds[0].in_flags = PR_POLL_READ;
      pds[1].fd = s;
      pds[1].in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
      
      while (1) {
	int nfds;

	nfds = PR_Poll(pds,2,PR_SecondsToInterval(2));
	if (nfds == 0) continue;

	/** read input from keyboard*/
	/*  note: this is very inefficient if reading from a file */
	
	if (pds[0].out_flags & PR_POLL_READ) {
	  amt = PR_Read(PR_STDIN,&buffer,1);
	  /*	PR_fprintf(PR_STDOUT,"fd[0]:%d=%d\r\n",amt,buffer[0]); */
	  if (amt == 0) {
	    PR_fprintf(PR_STDOUT,"\n");
	    goto loser;
	  }
	  
	  if (buffer[0] == '\r') {
	    buffer[0] = '\r';
	    buffer[1] = '\n';
	    amt = 2;
	  }
	  rv = PR_Write(PR_STDOUT,buffer,amt);
	  
	  
	  rv = PR_Write(s,buffer,amt);
	  if (rv == -1) {
	    PR_fprintf(PR_STDOUT,"Error writing to socket: %d\n",PR_GetError());
	  }
	}
	
	/***/
	
	
	/***/
	if (pds[1].out_flags & PR_POLL_EXCEPT) {
	  PR_fprintf(PR_STDOUT,"\r\nServer closed connection\r\n");
	  goto loser;
	}
	if (pds[1].out_flags & PR_POLL_READ) {
	  amt = PR_Read(s,&buffer,1024);
	  
	  if (amt == 0) {
	    PR_fprintf(PR_STDOUT,"\r\nServer closed connection\r\n");
	    goto loser;
	  }
	  rv = PR_Write(PR_STDOUT,buffer,amt);
	}
	/***/
	
      }
    }
  loser:
    
    /* set terminal back to normal */
    tcgetattr(t_fin,&tmp_tc);
    
    tmp_tc.c_lflag = prev_lflag;
    tmp_tc.c_oflag = prev_oflag;
    tmp_tc.c_iflag = prev_iflag;
    tcsetattr(t_fin, TCSANOW, &tmp_tc);
    
    /*   ioctl(tin, FIONBIO, (char *)&onoff);
	 ioctl(tout, FIONBIO, (char *)&onoff); */
  }

#endif
  /* SSLTELNET */

 closedown:

  PR_Close(s);

  if (NSS_Shutdown() != SECSuccess) {
    exit(1);
  }

  return(0);

} /* main */

/*EOF*/

