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
#include "protocolf.h"
#include <stdio.h>
  
 
int main() 
{
  void * blob, * recvd;
  int blobSize;
  SSMPRUint32 version, flags, port, connID, keySize, secretKeySize;
  SSMPRUint32 sessionID, httpPort;
  SSMPRInt32 result;
  char *profile, * nonce, * hostIP, * hostName, * cipher, * CA;
  SSMPRStatus rv;
  
  
  /*
   * Test functions to pack and parse HelloRequest message 
   */
  version = 3;
  profile = (char *)SSMPORT_ZAlloc(strlen("profile"));
  sprintf(profile, "profile");
  printf("HelloRequest, packing version #%d, profile %s\n", 
	 version, profile);
  blobSize = SSM_PackHelloRequest(&blob, version, profile);
  if (!blobSize)
    printf("Error in PackHelloRequest: %d\n", SSMPR_GetError());
  SSMPORT_Free(profile);
  version = 0;
  recvd = (void *)SSMPORT_ZAlloc(blobSize);
  if (!recvd) printf("Can't allocate %d bytes of memory!\n", blobSize);
  memcpy(recvd, blob, blobSize);
  SSMPORT_Free(blob);
  rv = SSM_ParseHelloRequest(recvd, &version, &profile);
  if (rv != SSMPR_SUCCESS) 
    printf("Error in ParseHelloRequest: %d\n", SSMPR_GetError());
  printf("HelloRequest, parsing version #%d, profile %s\n", 
	 version, profile);


  /* 
   * Test functions to parse and pack HelloReply message
   */
  version = 5;
  result = 2;
  sessionID = 34567;
  httpPort = 87654;
  nonce = (char *)SSMPORT_ZAlloc(strlen("some secret nonce"));
  sprintf(nonce, "some secret nonce");
  printf("HelloReply, packing result %d, sessionID %d, version #%d, httpPort %d,\n nonce %s\n", 
	 result, sessionID, version, httpPort, nonce);
  blobSize = SSM_PackHelloReply(&blob, result, sessionID, version, httpPort, 
				nonce);
  if (!blobSize) 
    printf("Error in PackHelloReply: %d\n", SSMPR_GetError());
  memset(nonce, 0, strlen(nonce));
  SSMPORT_Free(nonce);
  version = result = sessionID = httpPort = 0;
  recvd = (void *)SSMPORT_ZAlloc(blobSize);
  if (!recvd) printf("Can't allocate %d bytes of memory!\n", blobSize);
  memcpy(recvd, blob, blobSize);
  SSMPORT_Free(blob);
  rv = SSM_ParseHelloReply(recvd, &result, &sessionID, &version, &httpPort, 
			   &nonce);
  if (rv != SSMPR_SUCCESS) 
    printf("Error in ParseHelloReply: %d\n", SSMPR_GetError());
  printf("HelloReply, parsing result %d, sessionID %d, version #%d, httpPort %d, \n nonce %s\n", 
	 result, sessionID, version, httpPort, nonce);
  
  /*
   * Test functions to parse and pack SSLDataConnectionRequest message
   */
  flags = 0x00044000;
  port = 34567;
  hostIP = (char *)SSMPORT_ZAlloc(strlen("somehostIP"));
  sprintf(hostIP, "somehostIP");
  hostName = (char *)SSMPORT_ZAlloc(strlen("somehostName"));
  sprintf(hostName, "somehostName");
  printf("SSLDataConnRequest, packing flags %x, port %d, hostIP %s, hostName %s\n", 
	 flags, port, hostIP, hostName);
  blobSize = SSM_PackSSLDataConnectionRequest(&blob, flags, port, hostIP, 
					  hostName);
  if (!blobSize) 
    printf("Error in PackSSLDataConnectionRequest: %d\n", SSMPR_GetError());
  SSMPORT_Free(hostIP);
  SSMPORT_Free(hostName);
  flags = port = 0;
  
  recvd = (void *)SSMPORT_ZAlloc(blobSize);
  if (!recvd) printf("Can't allocate %d bytes of memory!\n", blobSize);
  memcpy(recvd, blob, blobSize);
  SSMPORT_Free(blob);  
  
  rv = SSM_ParseSSLDataConnectionRequest(recvd, &flags, &port, &hostIP, 
					 &hostName);
  if (rv != SSMPR_SUCCESS) 
    printf("Error in ParseSSLDataConnectionRequest: %d\n", SSMPR_GetError());
  printf(
"SSLDataConnRequest, parsing flags %x, port %d, hostIP %s, hostName %s\n", 
	 flags, port, hostIP, hostName);
  SSMPORT_Free(hostIP);
  SSMPORT_Free(hostName);
  
  
  /*
   * Test functions to parse and pack SSLDataConnectionReply message 
   */
  result = 2;
  connID = 713259;
  port = 57402;
  printf("SSLDataConnReply, packing result %d, connectionID %d, port %d\n",
	 result, connID, port);
  blobSize = SSM_PackSSLDataConnectionReply(&blob, result, connID, port);
  if (!blobSize) 
    printf("Error in PackSSLDataConnReply: %d\n", SSMPR_GetError());
  result = connID = port = 0;
  recvd = (void *)SSMPORT_ZAlloc(blobSize);
  if (!recvd) printf("Can't allocate %d bytes of memory!\n", blobSize);
  memcpy(recvd, blob, blobSize);
  SSMPORT_Free(blob);
  rv = SSM_ParseSSLDataConnectionReply(recvd, &result, &connID, &port);
  if (rv != SSMPR_SUCCESS) 
    printf("Error in ParseSSLDataConnectionReply: %d\n", SSMPR_GetError());
  printf("SSLDataConnReply, parsing result %d, connectionID %d, port %d\n", 
	 result, connID, port);
  
  
  /* 
   * Test functions to parse and pack SecurityStatusRequest message
   */
  connID = 45375;
  printf("SecurityStatusRequest, packing connection ID %d\n", connID);
  blobSize = SSM_PackSecurityStatusRequest(&blob, connID);
  if (!blobSize)  
    printf("Error in PackSecurityStatusRequest: %d\n", SSMPR_GetError());
  connID = 0;
  recvd = (void *)SSMPORT_ZAlloc(blobSize);
  if (!recvd) printf("Can't allocate %d bytes of memory!\n", blobSize);
  memcpy(recvd, blob, blobSize);
  SSMPORT_Free(blob);
  rv = SSM_ParseSecurityStatusRequest(recvd, &connID);
  if (rv != SSMPR_SUCCESS) 
    printf("Error in ParseSecurityStatusRequest: %d\n", SSMPR_GetError());
  printf("SecurityStatusRequest, parsing connection ID %d\n", connID);
  

  /* 
   * Test functions to parse and pack SecurityStatusReply message 
   */
  result = 2;
  keySize = 256;
  secretKeySize = 511;
  cipher = (char *)SSMPORT_ZAlloc(strlen("My Cipher"));
  sprintf(cipher, "My Cipher");
  CA = (char *)SSMPORT_ZAlloc(strlen("My CA issuer"));
  sprintf(CA, "My CA issuer");
  printf("SecurityStatusReply, packing result %d, keysize %d, secretKeySize %d, cipher %s, CA %s\n", result, keySize, secretKeySize, cipher, CA);
  blobSize = SSM_PackSecurityStatusReply(&blob, result, keySize, secretKeySize, cipher, CA);
  if (!blobSize) 
    printf("Error in PackSecurityStatusReply: %d\n", SSMPR_GetError());
  result = keySize = secretKeySize = 0;
  SSMPORT_Free(cipher);
  SSMPORT_Free(CA);
  recvd = (void *)SSMPORT_ZAlloc(blobSize);
  if (!recvd) printf("Can't allocate %d bytes of memory!\n", blobSize);
  memcpy(recvd, blob, blobSize);
  SSMPORT_Free(blob);
  rv = SSM_ParseSecurityStatusReply(recvd, &result, &keySize, &secretKeySize, 
				    &cipher, &CA);
  if (rv != SSMPR_SUCCESS) 
    printf("Error in ParseSecurityStatusReply: %d\n", SSMPR_GetError());
  printf("SecurityStatusReply, parsing result %d, keysize %d, secretKeySize %d, cipher %s, CA %s\n", result, keySize, secretKeySize, cipher, CA);
}     



