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

/****************************************************************************
 *  SSL server program listens on a port, accepts client connection, reads  *
 *  request and responds to it                                              *
 ****************************************************************************/

/* Generic header files */

#include <stdio.h>
#include <string.h>

/* NSPR header files */

#include "nspr.h"
#include "plgetopt.h"
#include "prerror.h"
#include "prnetdb.h"

/* NSS header files */

#include "pk11func.h"
#include "secitem.h"
#include "ssl.h"
#include "certt.h"
#include "nss.h"
#include "secrng.h"
#include "secder.h"
#include "key.h"
#include "sslproto.h"

/* Custom header files */

#include "sslsample.h"

#ifndef PORT_Sprintf
#define PORT_Sprintf sprintf
#endif

#define REQUEST_CERT_ONCE 1
#define REQUIRE_CERT_ONCE 2
#define REQUEST_CERT_ALL  3
#define REQUIRE_CERT_ALL  4

/* Global variables */
GlobalThreadMgr   threadMGR;
char             *password = NULL;
CERTCertificate  *cert = NULL;
SECKEYPrivateKey *privKey = NULL;
int               stopping;

static void
Usage(const char *progName)
{
	fprintf(stderr, 

"Usage: %s -n rsa_nickname -p port [-3RFrf] [-w password]\n"
"					[-c ciphers] [-d dbdir] \n"
"-3 means disable SSL v3\n"
"-r means request certificate on first handshake.\n"
"-f means require certificate on first handshake.\n"
"-R means request certificate on all handshakes.\n"
"-F means require certificate on all handshakes.\n"
"-c ciphers   Letter(s) chosen from the following list\n"
"A	  SSL2 RC4 128 WITH MD5\n"
"B	  SSL2 RC4 128 EXPORT40 WITH MD5\n"
"C	  SSL2 RC2 128 CBC WITH MD5\n"
"D	  SSL2 RC2 128 CBC EXPORT40 WITH MD5\n"
"E	  SSL2 DES 64 CBC WITH MD5\n"
"F	  SSL2 DES 192 EDE3 CBC WITH MD5\n"
"\n"
"a	  SSL3 FORTEZZA DMS WITH FORTEZZA CBC SHA\n"
"b	  SSL3 FORTEZZA DMS WITH RC4 128 SHA\n"
"c	  SSL3 RSA WITH RC4 128 MD5\n"
"d	  SSL3 RSA WITH 3DES EDE CBC SHA\n"
"e	  SSL3 RSA WITH DES CBC SHA\n"
"f	  SSL3 RSA EXPORT WITH RC4 40 MD5\n"
"g	  SSL3 RSA EXPORT WITH RC2 CBC 40 MD5\n"
"h	  SSL3 FORTEZZA DMS WITH NULL SHA\n"
"i	  SSL3 RSA WITH NULL MD5\n"
"j	  SSL3 RSA FIPS WITH 3DES EDE CBC SHA\n"
"k	  SSL3 RSA FIPS WITH DES CBC SHA\n"
"l	  SSL3 RSA EXPORT WITH DES CBC SHA\t(new)\n"
"m	  SSL3 RSA EXPORT WITH RC4 56 SHA\t(new)\n",
	progName);
	exit(1);
}

/* Function:  readDataFromSocket()
 *
 * Purpose:  Parse an HTTP request by reading data from a GET or POST.
 *
 */
SECStatus
readDataFromSocket(PRFileDesc *sslSocket, DataBuffer *buffer, char **fileName)
{
	char  *post;
	int    numBytes = 0;
	int    newln    = 0;  /* # of consecutive newlns */

	/* Read data while it comes in from the socket. */
	while (PR_TRUE) {
		buffer->index = 0;
		newln = 0;

		/* Read the buffer. */
		numBytes = PR_Read(sslSocket, &buffer->data[buffer->index], 
		                   buffer->remaining);
		if (numBytes <= 0) {
			errWarn("PR_Read");
			return SECFailure;
		}
		buffer->dataEnd = buffer->dataStart + numBytes;

		/* Parse the input, starting at the beginning of the buffer.
		 * Stop when we detect two consecutive \n's (or \r\n's) 
		 * as this signifies the end of the GET or POST portion.
		 * The posted data follows.
		 */
		while (buffer->index < buffer->dataEnd && newln < 2) {
			int octet = buffer->data[buffer->index++];
			if (octet == '\n') {
				newln++;
			} else if (octet != '\r') {
				newln = 0;
			}
		}

		/* Came to the end of the buffer, or second newline.
		 * If we didn't get an empty line ("\r\n\r\n"), then keep on reading.
		 */
		if (newln < 2) 
			continue;

		/* we're at the end of the HTTP request.
		 * If the request is a POST, then there will be one more
		 * line of data.
		 * This parsing is a hack, but ok for SSL test purposes.
		 */
		post = PORT_Strstr(buffer->data, "POST ");
		if (!post || *post != 'P') 
			break;

		/* It's a post, so look for the next and final CR/LF. */
		/* We should parse content length here, but ... */
		while (buffer->index < buffer->dataEnd && newln < 3) {
			int octet = buffer->data[buffer->index++];
			if (octet == '\n') {
				newln++;
			}
		}

		if (newln == 3)
			break;
	}

	/* Have either (a) a complete get, (b) a complete post, (c) EOF */

	/*  Execute a "GET " operation. */
	if (buffer->index > 0 && PORT_Strncmp(buffer->data, "GET ", 4) == 0) {
		int fnLength;

		/* File name is the part after "GET ". */
		fnLength = strcspn(buffer->data + 5, " \r\n");
		*fileName = (char *)PORT_Alloc(fnLength + 1);
		PORT_Strncpy(*fileName, buffer->data + 5, fnLength);
		(*fileName)[fnLength] = '\0';
	}

	return SECSuccess;
}

/* Function:  authenticateSocket()
 *
 * Purpose:  Configure a socket for SSL.
 *
 *
 */
PRFileDesc * 
setupSSLSocket(PRFileDesc *tcpSocket, int requestCert)
{
	PRFileDesc *sslSocket;
	SSLKEAType  certKEA;
	int         certErr = 0;
	SECStatus   secStatus;

	/* Set the appropriate flags. */

	sslSocket = SSL_ImportFD(NULL, tcpSocket);
	if (sslSocket == NULL) {
		errWarn("SSL_ImportFD");
		goto loser;
	}
   
	secStatus = SSL_OptionSet(sslSocket, SSL_SECURITY, PR_TRUE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet SSL_SECURITY");
		goto loser;
	}

	secStatus = SSL_OptionSet(sslSocket, SSL_HANDSHAKE_AS_SERVER, PR_TRUE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet:SSL_HANDSHAKE_AS_SERVER");
		goto loser;
	}

	secStatus = SSL_OptionSet(sslSocket, SSL_REQUEST_CERTIFICATE, 
	                       (requestCert >= REQUEST_CERT_ONCE));
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet:SSL_REQUEST_CERTIFICATE");
		goto loser;
	}

	secStatus = SSL_OptionSet(sslSocket, SSL_REQUIRE_CERTIFICATE, 
	                       (requestCert == REQUIRE_CERT_ONCE));
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet:SSL_REQUIRE_CERTIFICATE");
		goto loser;
	}

	/* Set the appropriate callback routines. */

	secStatus = SSL_AuthCertificateHook(sslSocket, myAuthCertificate, 
	                                    CERT_GetDefaultCertDB());
	if (secStatus != SECSuccess) {
		errWarn("SSL_AuthCertificateHook");
		goto loser;
	}

	secStatus = SSL_BadCertHook(sslSocket, 
	                            (SSLBadCertHandler)myBadCertHandler, &certErr);
	if (secStatus != SECSuccess) {
		errWarn("SSL_BadCertHook");
		goto loser;
	}

	secStatus = SSL_HandshakeCallback(sslSocket,
	                                 (SSLHandshakeCallback)myHandshakeCallback,
									  NULL);
	if (secStatus != SECSuccess) {
		errWarn("SSL_HandshakeCallback");
		goto loser;
	}

	secStatus = SSL_SetPKCS11PinArg(sslSocket, password);
	if (secStatus != SECSuccess) {
		errWarn("SSL_HandshakeCallback");
		goto loser;
	}

	certKEA = NSS_FindCertKEAType(cert);

	secStatus = SSL_ConfigSecureServer(sslSocket, cert, privKey, certKEA);
	if (secStatus != SECSuccess) {
		errWarn("SSL_ConfigSecureServer");
		goto loser;
	}

	return sslSocket;

loser:

	PR_Close(tcpSocket);
	return NULL;
}

/* Function:  authenticateSocket()
 *
 * Purpose:  Perform client authentication on the socket.
 *
 */
SECStatus
authenticateSocket(PRFileDesc *sslSocket, PRBool requireCert)
{
	CERTCertificate *cert;
	SECStatus secStatus;

	/* Returns NULL if client authentication is not enabled or if the
	 * client had no certificate. */
	cert = SSL_PeerCertificate(sslSocket);
	if (cert) {
		/* Client had a certificate, so authentication is through. */
		CERT_DestroyCertificate(cert);
		return SECSuccess;
	}

	/* Request client to authenticate itself. */
	secStatus = SSL_OptionSet(sslSocket, SSL_REQUEST_CERTIFICATE, PR_TRUE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet:SSL_REQUEST_CERTIFICATE");
		return SECFailure;
	}

	/* If desired, require client to authenticate itself.  Note
	 * SSL_REQUEST_CERTIFICATE must also be on, as above.  */
	secStatus = SSL_OptionSet(sslSocket, SSL_REQUIRE_CERTIFICATE, requireCert);
	if (secStatus != SECSuccess) {
		errWarn("SSL_OptionSet:SSL_REQUIRE_CERTIFICATE");
		return SECFailure;
	}

	/* Having changed socket configuration parameters, redo handshake. */
	secStatus = SSL_ReHandshake(sslSocket, PR_TRUE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_ReHandshake");
		return SECFailure;
	}

	/* Force the handshake to complete before moving on. */
	secStatus = SSL_ForceHandshake(sslSocket);
	if (secStatus != SECSuccess) {
		errWarn("SSL_ForceHandshake");
		return SECFailure;
	}

	return SECSuccess;
}

/* Function:  writeDataToSocket
 *
 * Purpose:  Write the client's request back to the socket.  If the client
 *           requested a file, dump it to the socket.
 *
 */
SECStatus
writeDataToSocket(PRFileDesc *sslSocket, DataBuffer *buffer, char *fileName)
{
	int headerLength;
	int numBytes;
	char messageBuffer[120];
	PRFileDesc *local_file_fd = NULL;
	char header[] = "<html><body><h1>Sample SSL server</h1><br><br>";
	char filehd[] = "<h2>The file you requested:</h2><br>";
	char reqhd[]  = "<h2>This is your request:</h2><br>";
	char link[]   = "Try getting a <a HREF=\"../testfile\">file</a><br>";
	char footer[] = "<br><h2>End of request.</h2><br></body></html>";

	headerLength = PORT_Strlen(defaultHeader);

	/* Write a header to the socket. */
	numBytes = PR_Write(sslSocket, header, PORT_Strlen(header));
	if (numBytes < 0) {
		errWarn("PR_Write");
		goto loser;
	}

	if (fileName) {
		PRFileInfo  info;
		PRStatus    prStatus;

		/* Try to open the local file named.	
		 * If successful, then write it to the client.
		 */
		prStatus = PR_GetFileInfo(fileName, &info);
		if (prStatus != PR_SUCCESS ||
		    info.type != PR_FILE_FILE ||
		    info.size < 0) {
			PORT_Free(fileName);
			/* Maybe a GET not sent from client.c? */
			goto writerequest;
			return SECSuccess;
		}

		local_file_fd = PR_Open(fileName, PR_RDONLY, 0);
		if (local_file_fd == NULL) {
			PORT_Free(fileName);
			goto writerequest;
		}

		/* Write a header to the socket. */
		numBytes = PR_Write(sslSocket, filehd, PORT_Strlen(filehd));
		if (numBytes < 0) {
			errWarn("PR_Write");
			goto loser;
		}

		/* Transmit the local file prepended by the default header
		 * across the socket.
		 */
		numBytes = PR_TransmitFile(sslSocket, local_file_fd, 
		                           defaultHeader, headerLength,
		                           PR_TRANSMITFILE_KEEP_OPEN,
		                           PR_INTERVAL_NO_TIMEOUT);

		/* Error in transmission. */
		if (numBytes < 0) {
			errWarn("PR_TransmitFile");
			/*
			i = PORT_Strlen(errString);
			PORT_Memcpy(buf, errString, i);
			*/
		/* Transmitted bytes successfully. */
		} else {
			numBytes -= headerLength;
			fprintf(stderr, "PR_TransmitFile wrote %d bytes from %s\n",
			        numBytes, fileName);
		}

		PORT_Free(fileName);
		PR_Close(local_file_fd);
	}

writerequest:

	/* Write a header to the socket. */
	numBytes = PR_Write(sslSocket, reqhd, PORT_Strlen(reqhd));
	if (numBytes < 0) {
		errWarn("PR_Write");
		goto loser;
	}

	/* Write the buffer data to the socket. */
	if (buffer->index <= 0) {
		/* Reached the EOF.  Report incomplete transaction to socket. */
		PORT_Sprintf(messageBuffer,
		             "GET or POST incomplete after %d bytes.\r\n",
		             buffer->dataEnd);
		numBytes = PR_Write(sslSocket, messageBuffer, 
		                    PORT_Strlen(messageBuffer));
		if (numBytes < 0) {
			errWarn("PR_Write");
			goto loser;
		}
	} else {
		/* Display the buffer data. */
		fwrite(buffer->data, 1, buffer->index, stdout);
		/* Write the buffer data to the socket. */
		numBytes = PR_Write(sslSocket, buffer->data, buffer->index);
		if (numBytes < 0) {
			errWarn("PR_Write");
			goto loser;
		}
		/* Display security information for the socket. */
		printSecurityInfo(sslSocket);
		/* Write any discarded data out to the socket. */
		if (buffer->index < buffer->dataEnd) {
			PORT_Sprintf(buffer->data, "Discarded %d characters.\r\n", 
			             buffer->dataEnd - buffer->index);
			numBytes = PR_Write(sslSocket, buffer->data, 
			                    PORT_Strlen(buffer->data));
			if (numBytes < 0) {
				errWarn("PR_Write");
				goto loser;
			}
		}
	}

	/* Write a footer to the socket. */
	numBytes = PR_Write(sslSocket, footer, PORT_Strlen(footer));
	if (numBytes < 0) {
		errWarn("PR_Write");
		goto loser;
	}

	/* Write a link to the socket. */
	numBytes = PR_Write(sslSocket, link, PORT_Strlen(link));
	if (numBytes < 0) {
		errWarn("PR_Write");
		goto loser;
	}

	/* Complete the HTTP transaction. */
	numBytes = PR_Write(sslSocket, "EOF\r\n\r\n\r\n", 9);
	if (numBytes < 0) {
		errWarn("PR_Write");
		goto loser;
	}

	/* Do a nice shutdown if asked. */
	if (!strncmp(buffer->data, stopCmd, strlen(stopCmd))) {
		stopping = 1;
	}
	return SECSuccess;

loser:

	/* Do a nice shutdown if asked. */
	if (!strncmp(buffer->data, stopCmd, strlen(stopCmd))) {
		stopping = 1;
	}
	return SECFailure;
}

/* Function:  int handle_connection()
 *
 * Purpose:  Thread to handle a connection to a socket.
 *
 */
SECStatus
handle_connection(void *tcp_sock, int requestCert)
{
	PRFileDesc *       tcpSocket = (PRFileDesc *)tcp_sock;
	PRFileDesc *       sslSocket = NULL;
	SECStatus          secStatus = SECFailure;
	PRStatus           prStatus;
	PRSocketOptionData socketOption;
	DataBuffer         buffer;
	char *             fileName = NULL;

	/* Initialize the data buffer. */
	memset(buffer.data, 0, BUFFER_SIZE);
	buffer.remaining = BUFFER_SIZE;
	buffer.index = 0;
	buffer.dataStart = 0;
	buffer.dataEnd = 0;

	/* Make sure the socket is blocking. */
	socketOption.option             = PR_SockOpt_Nonblocking;
	socketOption.value.non_blocking = PR_FALSE;
	PR_SetSocketOption(tcpSocket, &socketOption);

	sslSocket = setupSSLSocket(tcpSocket, requestCert);
	if (sslSocket == NULL) {
		errWarn("setupSSLSocket");
		goto cleanup;
	}

	secStatus = SSL_ResetHandshake(sslSocket, /* asServer */ PR_TRUE);
	if (secStatus != SECSuccess) {
		errWarn("SSL_ResetHandshake");
		goto cleanup;
	}

	/* Read data from the socket, parse it for HTTP content.
	 * If the user is requesting/requiring authentication, authenticate
	 * the socket.  Then write the result back to the socket.  */
	fprintf(stdout, "\nReading data from socket...\n\n");
	secStatus = readDataFromSocket(sslSocket, &buffer, &fileName);
	if (secStatus != SECSuccess) {
		goto cleanup;
	}
	if (requestCert >= REQUEST_CERT_ALL) {
		fprintf(stdout, "\nAuthentication requested.\n\n");
		secStatus = authenticateSocket(sslSocket, 
		                               (requestCert == REQUIRE_CERT_ALL));
		if (secStatus != SECSuccess) {
			goto cleanup;
		}
	}

	fprintf(stdout, "\nWriting data to socket...\n\n");
	secStatus = writeDataToSocket(sslSocket, &buffer, fileName);

cleanup:

	/* Close down the socket. */
	prStatus = PR_Close(tcpSocket);
	if (prStatus != PR_SUCCESS) {
		errWarn("PR_Close");
	}

	return secStatus;
}

/* Function:  int accept_connection()
 *
 * Purpose:  Thread to accept a connection to the socket.
 *
 */
SECStatus
accept_connection(void *listener, int requestCert)
{
	PRFileDesc *listenSocket = (PRFileDesc*)listener;
	PRNetAddr   addr;
	PRStatus    prStatus;

	/* XXX need an SSL socket here? */
	while (!stopping) {
		PRFileDesc *tcpSocket;
		SECStatus	result;

		fprintf(stderr, "\n\n\nAbout to call accept.\n");

		/* Accept a connection to the socket. */
		tcpSocket = PR_Accept(listenSocket, &addr, PR_INTERVAL_NO_TIMEOUT);
		if (tcpSocket == NULL) {
			errWarn("PR_Accept");
			break;
		}

		/* Accepted the connection, now handle it. */
		result = launch_thread(&threadMGR, handle_connection, 
		                       tcpSocket, requestCert);

		if (result != SECSuccess) {
			prStatus = PR_Close(tcpSocket);
			if (prStatus != PR_SUCCESS) {
				exitErr("PR_Close");
			}
			break;
		}
	}

	fprintf(stderr, "Closing listen socket.\n");

	prStatus = PR_Close(listenSocket);
	if (prStatus != PR_SUCCESS) {
		exitErr("PR_Close");
	}
	return SECSuccess;
}

/* Function:  void server_main()
 *
 * Purpose:  This is the server's main function.  It configures a socket
 *			 and listens to it.
 *
 */
void
server_main(
	unsigned short      port, 
	int                 requestCert, 
	SECKEYPrivateKey *  privKey,
	CERTCertificate *   cert, 
	PRBool              disableSSL3)
{
	SECStatus           secStatus;
	PRStatus            prStatus;
	PRFileDesc *        listenSocket;
	PRNetAddr           addr;
	PRSocketOptionData  socketOption;

	/* Create a new socket. */
	listenSocket = PR_NewTCPSocket();
	if (listenSocket == NULL) {
		exitErr("PR_NewTCPSocket");
	}

	/* Set socket to be blocking -
	 * on some platforms the default is nonblocking.
	 */
	socketOption.option = PR_SockOpt_Nonblocking;
	socketOption.value.non_blocking = PR_FALSE;

	prStatus = PR_SetSocketOption(listenSocket, &socketOption);
	if (prStatus != PR_SUCCESS) {
		exitErr("PR_SetSocketOption");
	}

	/* This cipher is not on by default. The Acceptance test
	 * would like it to be. Turn this cipher on.
	 */
	secStatus = SSL_CipherPrefSetDefault(SSL_RSA_WITH_NULL_MD5, PR_TRUE);
	if (secStatus != SECSuccess) {
		exitErr("SSL_CipherPrefSetDefault:SSL_RSA_WITH_NULL_MD5");
	}

	/* Configure the network connection. */
	addr.inet.family = PR_AF_INET;
	addr.inet.ip	 = PR_INADDR_ANY;
	addr.inet.port	 = PR_htons(port);

	/* Bind the address to the listener socket. */
	prStatus = PR_Bind(listenSocket, &addr);
	if (prStatus != PR_SUCCESS) {
		exitErr("PR_Bind");
	}

	/* Listen for connection on the socket.  The second argument is
	 * the maximum size of the queue for pending connections.
	 */
	prStatus = PR_Listen(listenSocket, 5);
	if (prStatus != PR_SUCCESS) {
		exitErr("PR_Listen");
	}

	/* Launch thread to handle connections to the socket. */
	secStatus = launch_thread(&threadMGR, accept_connection, 
                              listenSocket, requestCert);
	if (secStatus != SECSuccess) {
		PR_Close(listenSocket);
	} else {
		reap_threads(&threadMGR);
		destroy_thread_data(&threadMGR);
	}
}

/* Function: int main()
 *
 * Purpose:  Parses command arguments and configures SSL server.
 *
 */
int
main(int argc, char **argv)
{
	char *              progName      = NULL;
	char *              nickName      = NULL;
	char *              cipherString  = NULL;
	char *              dir           = ".";
	int                 requestCert   = 0;
	unsigned short      port          = 0;
	SECStatus           secStatus;
	PRBool              disableSSL3   = PR_FALSE;
	PLOptState *        optstate;
	PLOptStatus         status;

	/* Zero out the thread manager. */
	PORT_Memset(&threadMGR, 0, sizeof(threadMGR));

	progName = PL_strdup(argv[0]);

	optstate = PL_CreateOptState(argc, argv, "3FRc:d:fp:n:rw:");
	while ((status = PL_GetNextOpt(optstate)) == PL_OPT_OK) {
		switch(optstate->option) {
		case '3': disableSSL3 = PR_TRUE;                      break;
		case 'F': requestCert = REQUIRE_CERT_ALL;             break;
		case 'R': requestCert = REQUEST_CERT_ALL;             break;
		case 'c': cipherString = PL_strdup(optstate->value);  break;
		case 'd': dir = PL_strdup(optstate->value);           break;
		case 'f': requestCert = REQUIRE_CERT_ONCE;            break;
		case 'n': nickName = PL_strdup(optstate->value);      break;
		case 'p': port = PORT_Atoi(optstate->value);          break;
		case 'r': requestCert = REQUEST_CERT_ONCE;            break;
		case 'w': password = PL_strdup(optstate->value);      break;
		default:
		case '?': Usage(progName);
		}
	}

	if (nickName == NULL || port == 0)
		Usage(progName);

	/* Call the NSPR initialization routines. */
	PR_Init( PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

	/* Set the cert database password callback. */
	PK11_SetPasswordFunc(myPasswd);

	/* Initialize NSS. */
	secStatus = NSS_Init(dir);
	if (secStatus != SECSuccess) {
		exitErr("NSS_Init");
	}

	/* Set the policy for this server (REQUIRED - no default). */
	secStatus = NSS_SetDomesticPolicy();
	if (secStatus != SECSuccess) {
		exitErr("NSS_SetDomesticPolicy");
	}

	/* XXX keep this? */
	/* all the SSL2 and SSL3 cipher suites are enabled by default. */
	if (cipherString) {
	    int ndx;

	    /* disable all the ciphers, then enable the ones we want. */
	    disableAllSSLCiphers();

	    while (0 != (ndx = *cipherString++)) {
		int *cptr;
		int  cipher;

		if (! isalpha(ndx))
			Usage(progName);
		cptr = islower(ndx) ? ssl3CipherSuites : ssl2CipherSuites;
		for (ndx &= 0x1f; (cipher = *cptr++) != 0 && --ndx > 0; ) 
		    /* do nothing */;
		if (cipher) {
		    SECStatus status;
		    status = SSL_CipherPrefSetDefault(cipher, PR_TRUE);
		    if (status != SECSuccess) 
			errWarn("SSL_CipherPrefSetDefault()");
		}
	    }
	}

	/* Get own certificate and private key. */
	cert = PK11_FindCertFromNickname(nickName, password);
	if (cert == NULL) {
		exitErr("PK11_FindCertFromNickname");
	}

	privKey = PK11_FindKeyByAnyCert(cert, password);
	if (privKey == NULL) {
		exitErr("PK11_FindKeyByAnyCert");
	}

	/* Configure the server's cache for a multi-process application
	 * using default timeout values (24 hrs) and directory location (/tmp). 
	 */
	SSL_ConfigMPServerSIDCache(256, 0, 0, NULL);

	/* Launch server. */
	server_main(port, requestCert, privKey, cert, disableSSL3);

	/* Shutdown NSS and exit NSPR gracefully. */
	NSS_Shutdown();
	PR_Cleanup();
	return 0;
}
