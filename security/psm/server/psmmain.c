/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "serv.h"
#include "secport.h"
#include "dataconn.h"
#include "ctrlconn.h"
#include "minihttp.h"
#include "ciferfam.h"
#include "secmime.h"
#include "messages.h"
#include "textgen.h"
#include "oldfunc.h"
#include "nss.h"
#include "p12plcy.h"
#include "nlslayer.h"
#include "softoken.h"

void SSM_InitLogging(void);

#ifdef TIMEBOMB
#include "timebomb.h"
#endif

#ifdef WIN32
#include <wtypes.h>
#include <winreg.h>
#include <winerror.h>
#endif

#ifdef XP_UNIX
#include <signal.h>
#include <unistd.h>

#ifndef SIG_ERR
#define SIG_ERR -1
#endif /*SIG_ERR*/

#endif /*XP_UNIX*/

#define POLICY_TYPE_INDEX 0

SSMCollection * connections       = NULL;
SSMHashTable  * tokenList         = NULL;
PRMonitor     * tokenLock         = NULL;
SSMHashTable  * ctrlConnections   = NULL;

SSMPolicyType   policyType        = ssmDomestic;

/*
 * The following is a quick write of enabling various ciphers.  This code is
 * essentially moved from the server core code.  Eventually we will need to
 * place this data and functionality in a more modular way.
 */
#define SSM_POLICY_END 0 /* end of table */
#define SSM_POLICY_SSL 1 /* SSL ciphersuites: not really used */
#define SSM_POLICY_PK12 2 /* PKCS #12 ciphersuites */
#define SSM_POLICY_SMIME 3 /* S/MIME ciphersuites */

typedef struct {
    PRInt32 policy;
    PRInt32 key;
    PRInt32 value;
} SSMPolicyEntry;

static SSMPolicyEntry ssmPK12PolicyTable[] =
{
    {SSM_POLICY_PK12, PKCS12_RC4_40,       (PRInt32)PR_TRUE},
    {SSM_POLICY_PK12, PKCS12_RC4_128,      (PRInt32)PR_TRUE},
    {SSM_POLICY_PK12, PKCS12_RC2_CBC_40,   (PRInt32)PR_TRUE},
    {SSM_POLICY_PK12, PKCS12_RC2_CBC_128,  (PRInt32)PR_TRUE},
    {SSM_POLICY_PK12, PKCS12_DES_56,       (PRInt32)PR_TRUE},
    {SSM_POLICY_PK12, PKCS12_DES_EDE3_168, (PRInt32)PR_TRUE},
    {SSM_POLICY_END, 0, 0}
};

static SSMPolicyEntry ssmSMIMEPolicyTable[] =
{
    {SSM_POLICY_SMIME, SMIME_RC2_CBC_40,       (PRInt32)PR_TRUE},
    {SSM_POLICY_SMIME, SMIME_RC2_CBC_64,       (PRInt32)PR_TRUE},
    {SSM_POLICY_SMIME, SMIME_RC2_CBC_128,      (PRInt32)PR_TRUE},
    {SSM_POLICY_SMIME, SMIME_DES_CBC_56,       (PRInt32)PR_TRUE},
    {SSM_POLICY_SMIME, SMIME_DES_EDE3_168,     (PRInt32)PR_TRUE},
    {SSM_POLICY_SMIME, SMIME_RC5PAD_64_16_40,  (PRInt32)PR_TRUE},
    {SSM_POLICY_SMIME, SMIME_RC5PAD_64_16_64,  (PRInt32)PR_TRUE},
    {SSM_POLICY_SMIME, SMIME_RC5PAD_64_16_128, (PRInt32)PR_TRUE},
    {SSM_POLICY_END, 0, 0}
};

static SSMStatus SSM_InstallPK12Policy(void)
{
    const SSMPolicyEntry* entry = ssmPK12PolicyTable;
    int i;

    for (i = 0; entry[i].policy != SSM_POLICY_END; i++) {
        if (SEC_PKCS12EnableCipher(entry[i].key, entry[i].value) != 
            SECSuccess) {
            return SSM_FAILURE;
        }
    }
    return SSM_SUCCESS;
}

static SSMStatus SSM_InstallSMIMEPolicy(void)
{
    const SSMPolicyEntry* entry = ssmSMIMEPolicyTable;
    int i;
    
    for (i = 0; entry[i].policy != SSM_POLICY_END; i++) {
        if (SECMIME_SetPolicy(entry[i].key, entry[i].value) != SECSuccess) {
            return SSM_FAILURE;
        }
    }
    return SSM_SUCCESS;
}

/* XXX sjlee: we don't need to do a similar thing for SSL as we can call an
 *     NSS function to do it
 */

#if 0
/*
 * This function is required by svrplcy to set the
 * utility policy.  This will tell us what kind of 
 * policy we are running.
 */
SECStatus Utility_SetPolicy(long which, int policy)
{
    policyType = ssmDomestic;
    return SECSuccess;
}
#endif

void SSM_SetPolicy(void)
{
#if 0
    SVRPLCY_InstallSSLPolicy();
    SVRPLCY_InstallPK12Policy();
    SVRPLCY_InstallSMIMEPolicy();
    SVRPLCY_InstallUtilityPolicy();
#else
	/* Always domestic policy now */
	NSS_SetDomesticPolicy();
    SSM_InstallPK12Policy();
    SSM_InstallSMIMEPolicy();
#endif
}

SSMPolicyType
SSM_GetPolicy(void)
{
    return policyType;
}

static void
enable_SMIME_cipher_prefs(void)
{
    SSMPolicyType policy;

    policy = SSM_GetPolicy();

    switch (policy)
    {
    case ssmDomestic:
        SECMIME_EnableCipher(SMIME_DES_EDE3_168, 1);
        SECMIME_EnableCipher(SMIME_RC2_CBC_128, 1);
        SECMIME_EnableCipher(SMIME_RC2_CBC_64, 1);
        SECMIME_EnableCipher(SMIME_DES_CBC_56, 1);
#if 0
        SECMIME_EnableCipher(SMIME_RC5PAD_64_16_128, 1);
        SECMIME_EnableCipher(SMIME_RC5PAD_64_16_64, 1);
        SECMIME_EnableCipher(SMIME_FORTEZZA, 1);
#endif
    case ssmExport:
        SECMIME_EnableCipher(SMIME_RC2_CBC_40, 1);
#if 0
        SECMIME_EnableCipher(SMIME_RC5PAD_64_16_40, 1);
#endif
    case ssmFrance:
    default:
        break;
    }

    /* now tell secmime that we've sent it the last preference */
    SECMIME_EnableCipher(CIPHER_FAMILYID_MASK, 0);
}

#define SHORT_PK11_STRING 33
#define LONG_PK11_STRING  65

static char*
ssm_ConverToLength(char *origString, PRUint32 newLen)
{
    char *newString;
    PRUint32 origLen;
    PRUint32 copyLen;

    newString = SSM_NEW_ARRAY(char,newLen+1);
    if (newString == NULL) {
        return origString;
    }
    origLen = PL_strlen(origString);
    copyLen = (origLen > newLen) ? newLen : origLen;
    memcpy(newString, origString, copyLen);
    memset(newString+copyLen, ' ' ,newLen - copyLen);
    newString[newLen]='\0';
    PR_Free(origString);
    return newString;
}

SECStatus
ssm_InitializePKCS11Strings(void)
{
    char *manufacturerID             = NULL;
    char *libraryDescription         = NULL;
    char *tokenDescription           = NULL;
    char *privateTokenDescription    = NULL;
    char *slotDescription            = NULL;
    char *privateSlotDescription     = NULL;
    char *fipsSlotDescription        = NULL;
    char *fipsPrivateSlotDescription = NULL; 

    SSMTextGenContext *cx;
    SSMStatus rv;

    rv = SSMTextGen_NewTopLevelContext(NULL, &cx);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    rv = SSM_FindUTF8StringInBundles(cx, "manufacturerID", &manufacturerID);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(manufacturerID) != SHORT_PK11_STRING) {
        manufacturerID = ssm_ConverToLength(manufacturerID, SHORT_PK11_STRING);
    }
    rv = SSM_FindUTF8StringInBundles(cx, "libraryDescription", 
                                     &libraryDescription);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(libraryDescription) != SHORT_PK11_STRING) {
        libraryDescription = ssm_ConverToLength(libraryDescription, 
                                                SHORT_PK11_STRING);
    }
    rv = SSM_FindUTF8StringInBundles(cx, "tokenDescription", 
                                     &tokenDescription);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(tokenDescription) != SHORT_PK11_STRING) {
        tokenDescription = ssm_ConverToLength(tokenDescription, 
                                              SHORT_PK11_STRING);
    }
    rv = SSM_FindUTF8StringInBundles(cx, "privateTokenDescription",
                                     &privateTokenDescription);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(privateTokenDescription) != SHORT_PK11_STRING) {
        privateTokenDescription = ssm_ConverToLength(privateTokenDescription,
                                                     SHORT_PK11_STRING);
    }
    rv = SSM_FindUTF8StringInBundles(cx, "slotDescription", &slotDescription);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(slotDescription) != LONG_PK11_STRING) {
        slotDescription = ssm_ConverToLength(slotDescription,
                                             LONG_PK11_STRING);
    }
    rv = SSM_FindUTF8StringInBundles(cx, "privateSlotDescription",
                                     &privateSlotDescription);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(privateSlotDescription) != LONG_PK11_STRING) {
        privateSlotDescription = ssm_ConverToLength(privateSlotDescription,
                                                    LONG_PK11_STRING);
    }
    rv = SSM_FindUTF8StringInBundles(cx, "fipsSlotDescription",
                                     &fipsSlotDescription);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(fipsSlotDescription) != LONG_PK11_STRING) {
        fipsSlotDescription = ssm_ConverToLength(fipsSlotDescription,
                                                 LONG_PK11_STRING);
    }
    rv = SSM_FindUTF8StringInBundles(cx, "fipsPrivateSlotDescription",
                                     &fipsPrivateSlotDescription);
    if (rv != SSM_SUCCESS) {
        goto loser;
    }
    if (PL_strlen(fipsPrivateSlotDescription) != LONG_PK11_STRING) {
        fipsPrivateSlotDescription = 
            ssm_ConverToLength(fipsPrivateSlotDescription, LONG_PK11_STRING);
    }
    if (cx != NULL) {
        SSMTextGen_DestroyContext(cx);
    }
    PK11_ConfigurePKCS11(manufacturerID, libraryDescription, tokenDescription,
                         privateTokenDescription, slotDescription, 
                         privateSlotDescription, fipsSlotDescription, 
                         fipsPrivateSlotDescription, 0, 0);
    return SECSuccess;
 loser:
    PR_FREEIF(manufacturerID);
    PR_FREEIF(libraryDescription);
    PR_FREEIF(tokenDescription);
    PR_FREEIF(privateTokenDescription);
    PR_FREEIF(slotDescription);
    PR_FREEIF(privateSlotDescription);
    if (cx != NULL) {
        SSMTextGen_DestroyContext(cx);
    }
    return SECFailure;
}


#ifdef XP_UNIX
#define CATCH_SIGNAL_DEFAULT(SIGNAL) \
    if (((int)signal(SIGNAL, psm_signal_handler_default)) == ((int)SIG_ERR)) \
        goto loser;

#define CATCH_SIGNAL_IGNORE(SIGNAL) \
    if (((int)signal(SIGNAL, psm_signal_handler_ignore)) == ((int)SIG_ERR)) \
        goto loser;

static void
psm_signal_handler_default(int sig)
{
#ifdef DEBUG
  printf ("Trapping the signal %d\n", sig);
#endif
  SSM_ReleaseLockFile();
  kill(getpid(),SIGKILL);
}

static void
psm_signal_handler_ignore(int sig)
{
#ifdef DEBUG
  printf ("Ignoring the signal %d\n", sig);
#endif
  signal(sig,psm_signal_handler_ignore);
}

static SSMStatus
psm_catch_signals(void)
{
  CATCH_SIGNAL_IGNORE(SIGHUP);
  CATCH_SIGNAL_DEFAULT(SIGINT);
  CATCH_SIGNAL_DEFAULT(SIGQUIT);
  CATCH_SIGNAL_DEFAULT(SIGILL);
  CATCH_SIGNAL_DEFAULT(SIGTRAP);
  CATCH_SIGNAL_DEFAULT(SIGABRT);
  CATCH_SIGNAL_DEFAULT(SIGIOT);
#ifdef SIGEMT
  CATCH_SIGNAL_DEFAULT(SIGEMT);
#endif
  CATCH_SIGNAL_DEFAULT(SIGFPE);
  CATCH_SIGNAL_DEFAULT(SIGBUS);
  CATCH_SIGNAL_DEFAULT(SIGSEGV);
#ifdef SIGSYS
  CATCH_SIGNAL_DEFAULT(SIGSYS);
#endif
  CATCH_SIGNAL_IGNORE(SIGPIPE);
  CATCH_SIGNAL_DEFAULT(SIGTERM);
#ifndef LINUX
  CATCH_SIGNAL_DEFAULT(SIGUSR1);
#endif
  CATCH_SIGNAL_DEFAULT(SIGUSR2);
#ifdef SIGXCPU
  CATCH_SIGNAL_DEFAULT(SIGXCPU);
#endif
#ifdef SIGDANGER
  CATCH_SIGNAL_DEFAULT(SIGDANGER);
#endif
  return SSM_SUCCESS;
 loser:
  return SSM_FAILURE;
}
#endif

/*#if defined(XP_PC) && !defined(DEBUG)*/
#if 0
int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrevInst,
                   LPSTR lpszLine, int nShow)
#elif defined(XP_MAC)
/*
	We run RunMacPSM in a separate thread off the main thread. 
	This is because we can't do any blocking I/O routines on the main
	thread, because Mac NSPR doesn't own the original thread used to
	run the app.
*/

static void* glue_component = NULL;

void RunMacPSM(void *arg)
#else
int psm_main(int argc, char ** argv)
#endif
{
    /*#if (defined(XP_PC) && !defined(DEBUG)) || (defined(XP_MAC))*/
#ifdef XP_MAC
    /* substitute argc and argv for NSPR */
    int argc = 0;
    char *argv[] = {"", NULL};
#endif
    PRIntn result = 0;

#ifdef XP_MAC
    glue_component = arg;
#endif

#ifdef DEBUG
    PR_STDIO_INIT();
#endif

#ifdef DEBUG
    /* Initialize logging. */
    SSM_InitLogging();
#endif

#ifdef TIMEBOMB
    SSM_CheckTimebomb();
#endif

#ifdef XP_UNIX
    if (psm_catch_signals() != SSM_SUCCESS) {
      SSM_DEBUG("Couldn't set signal handlers.  Quitting\n");
      exit(1);
    }
#endif

    SSM_SetPolicy();
    enable_SMIME_cipher_prefs();
    if (SSM_GetPolicy() == ssmDomestic) {
      SSM_EnableHighGradeKeyGen();
    }

	/* Initialize NLS layer */
	if (nlsInit() != PR_TRUE) {
		SSM_DEBUG("Failed to initialize the NLS layer\n");
		exit(1);
	}

    /* Initialize global list of control connections. */
    connections = SSM_NewCollection();
    if (connections == NULL)
    {
        SSM_DEBUG("Can't initialize! (%ld)\n", (long) result);
        exit(1);
    }
    /* Initialize global list of tokens */
    result = SSM_HashCreate(&tokenList);
    if (result != PR_SUCCESS || !tokenList) {
       SSM_DEBUG("Can't initialize - tokenList \n");
       exit(result);
    }
    tokenLock = PR_NewMonitor();
    if (!tokenLock) {
        SSM_DEBUG("Can't initialize - tokenLock\n");
	exit(1);
    }
    /* Initialize hash table of control connections */
    result = SSM_HashCreate(&ctrlConnections);
    if (result != PR_SUCCESS || !ctrlConnections) {
        SSM_DEBUG("Can't initialize global table for control connections \n");
        exit(result);
    }

    /* Initialize resource table */
    SSM_ResourceInit();

    if (SSM_InitPolicyHandler() != PR_SUCCESS) {
        SSM_DEBUG("Couldn't initialize the Policy Handler.\n");
        exit (1);
    }

    /* initialize random number generator */
    SSM_DEBUG("Initializing random number generator.\n");
    RNG_RNGInit();
    RNG_SystemInfoForRNG();
   
    /*
     * All the ciphers except SSL_RSA_WITH_NULL_MD5 are on by default.
     * Enable encryption, enable NULL cipher. 
     */
#ifdef XP_MAC
	result = mainLoop(argc, argv);
#else
    result = PR_Initialize(mainLoop, argc, argv, 0);
#endif

#ifdef DEBUG
    printf("main: Finishing (%ld)\n", (long) result);
#endif
#ifdef XP_UNIX
    SSM_ReleaseLockFile();
#endif
#ifndef XP_MAC
    return result;
#endif
}

#ifdef XP_MAC
void *
psm_malloc(unsigned long numbytes)
{
	return NewPtrClear(numbytes);
}

void
psm_free(void *ptr)
{
	DisposePtr((char *) ptr);
}
#endif


PRIntn mainLoop(PRIntn argc, char ** argv)
{
    static PRFileDesc       *socket = NULL;
    PRFileDesc              *respsocket;    
    PRNetAddr               clientaddr;
    SSMControlConnection    *curconnect;
    SSMResourceID           ctrlID;
    SSMStatus                status = PR_FAILURE;
    PRBool                  alive = PR_TRUE;

    /* Register ourselves so that logs, etc can identify us */
    SSM_RegisterThread("main", NULL);

    /* Open NLS stuff */
    SSM_DEBUG("Initializing NLS.\n");
#ifdef XP_MAC
	SSM_InitNLS(":ui:");
#else
    SSM_InitNLS("ui");
#endif
    ssm_InitializePKCS11Strings();

    /* Initialize the protocol */
#ifdef XP_MAC
	CMT_Init(malloc, free);
#else
    CMT_Init(PR_Malloc, PR_Free);
#endif

    /* Open the HTTP listener */
    SSM_DEBUG("Opening HTTP thread.\n");
	status = SSM_InitHTTP();
    if (status != SSM_SUCCESS) 
    {
        SSM_DEBUG("Couldn't open web port. Exiting.\n");
        goto loser;
    }

    /* open a port for control connections, with well-known port# */
    if (!socket)
    	socket = SSM_OpenControlPort();
    if (!socket) 
    {
        SSM_DEBUG("Couldn't open control port. Exiting.\n");
        goto loser;
    }

#ifdef XP_MAC
    if (PR_CEnterMonitor(glue_component) != NULL) {
        PR_CNotify(glue_component);
        PR_CExitMonitor(glue_component);
    }
#endif
    
    while (alive) 
    {
        /* wait until there is incoming request */
  
        respsocket = PR_Accept(socket, &clientaddr, PR_INTERVAL_NO_TIMEOUT); 
        /*      while (respsocket == NULL) 
                { 
                if (PR_GetError() != PR_WOULD_BLOCK_ERROR) 
                goto loser;
                PR_Sleep(CARTMAN_SPINTIME);
#ifdef DEBUG
                printf("master: Still ready for client connection on port %d\n", CARTMAN_PORT); fflush(stdout);
#endif
                respsocket = PR_Accept(socket, &clientaddr, 
                PR_SecondsToInterval(2));
                } */
        if (!respsocket) {
            /* accept failed: abort */
            status = PR_GetError();
            SSM_DEBUG("Error %d accepting control connection.  Exiting.\n",
                      status);
            goto loser;
        }

        if (SSM_SocketPeerCheck(respsocket, PR_TRUE)) {
            SSM_DEBUG("creating control connection.\n");
            status = SSM_CreateResource(SSM_RESTYPE_CONTROL_CONNECTION,
                                        respsocket,
                                        NULL,
                                        &ctrlID,
                                        (SSMResource **) &curconnect);
            if (status != PR_SUCCESS)
                break;

            PR_ASSERT(RESOURCE_CLASS(curconnect) == SSM_RESTYPE_CONTROL_CONNECTION);
        }
        else {
            /* connection did not come from localhost: shut down the 
             * connection and continue to loop
             */
            SSM_DEBUG("Connection attempt from a non-local host!\n");
            status = PR_Shutdown(respsocket, PR_SHUTDOWN_BOTH);
            respsocket = NULL;
        }
    } /* end while(true) */
 loser:
    /* Shut down the HTTP thread. */
    if (httpListenThread != NULL){
      PR_Interrupt(httpListenThread);
    }
#ifndef XP_MAC    
    exit(1);
#endif    
    /* ### mwelch - should we return meaningful error code? */

    return 0;
}
