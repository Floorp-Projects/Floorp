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
#include "seccomon.h"

#if defined( _WIN32 ) || defined( __WIN32__ )
#define RETURN_TYPE  extern __declspec( dllexport ) int _cdecl
#endif /* Windows */
#include "maci.h"


RETURN_TYPE
MACI_ChangePIN PROTO_LIST( (
    HSESSION       hSession,
    int            PINType,
    CI_PIN CI_FAR  pOldPIN,
    CI_PIN CI_FAR  pNewPIN ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_CheckPIN PROTO_LIST( (
    HSESSION       hSession,
    int            PINType,
    CI_PIN CI_FAR  pPIN ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Close PROTO_LIST( (
    HSESSION       hSession,
    unsigned int   Flags,
    int            SocketIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Decrypt PROTO_LIST( (
    HSESSION       hSession,
    unsigned int   CipherSize,
    CI_DATA        pCipher,
    CI_DATA        pPlain ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_DeleteCertificate PROTO_LIST( (
    HSESSION       hSession,
    int            CertificateIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_DeleteKey PROTO_LIST( (
    HSESSION       hSession,
    int            RegisterIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Encrypt PROTO_LIST( (
    HSESSION       hSession,
    unsigned int   PlainSize,
    CI_DATA        pPlain,
    CI_DATA        pCipher ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_ExtractX PROTO_LIST( (
    HSESSION             hSession,
    int                  CertificateIndex,
    int                  AlgorithmType,
    CI_PASSWORD CI_FAR   pPassword,
    unsigned int         YSize,
    CI_Y CI_FAR          pY,
    CI_WRAPPED_X CI_FAR  pX,
    CI_RA CI_FAR         pRa,
    unsigned int         PandGSize,
    unsigned int         QSize,
    CI_P CI_FAR          pP,
    CI_Q CI_FAR          pQ,
    CI_G CI_FAR          pG ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_FirmwareUpdate PROTO_LIST( (
    HSESSION       hSession,
    unsigned long  Flags,
    long           Cksum,
    unsigned int   CksumLength,
    unsigned int   DataSize,
    CI_DATA        pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GenerateIV PROTO_LIST( (
    HSESSION       hSession,
    CI_IV CI_FAR  pIV ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GenerateMEK PROTO_LIST( (
    HSESSION       hSession,
    int  RegisterIndex,
    int  Reserved ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GenerateRa PROTO_LIST( (
    HSESSION       hSession,
    CI_RA CI_FAR   pRa ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GenerateRandom PROTO_LIST( (
    HSESSION          hSession,
    CI_RANDOM CI_FAR  pRandom ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GenerateTEK PROTO_LIST( (
    HSESSION      hSession,
    int           Flags,
    int           RegisterIndex,
    CI_RA CI_FAR  pRa,
    CI_RB CI_FAR  pRb,
    unsigned int  YSize,
    CI_Y CI_FAR   pY ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GenerateX PROTO_LIST( (
    HSESSION      hSession,
    int           CertificateIndex,
    int           AlgorithmType,
    unsigned int  PandGSize,
    unsigned int  QSize,
    CI_P CI_FAR   pP,
    CI_Q CI_FAR   pQ,
    CI_G CI_FAR   pG,
    unsigned int  YSize,
    CI_Y CI_FAR   pY ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetCertificate PROTO_LIST( (
    HSESSION               hSession,
    int                    CertificateIndex,
    CI_CERTIFICATE CI_FAR  pCertificate ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetConfiguration PROTO_LIST( (
    HSESSION       hSession,
    CI_CONFIG_PTR  pConfiguration ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetHash PROTO_LIST( (
    HSESSION             hSession,
    unsigned int         DataSize,
    CI_DATA              pData,
    CI_HASHVALUE CI_FAR  pHashValue ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetPersonalityList PROTO_LIST( (
    HSESSION          hSession,
    int               EntryCount,
    CI_PERSON CI_FAR  pPersonalityList[] ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetSessionID PROTO_LIST( (
    HSESSION     *hSession ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetState PROTO_LIST( (
    HSESSION       hSession,
    CI_STATE_PTR   pState ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetStatus PROTO_LIST( (
    HSESSION       hSession,
    CI_STATUS_PTR  pStatus ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_GetTime PROTO_LIST( (
    HSESSION        hSession,
    CI_TIME CI_FAR  pTime ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Hash PROTO_LIST( (
    HSESSION      hSession,
    unsigned int  DataSize,
    CI_DATA       pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Initialize PROTO_LIST( (
    int CI_FAR  *SocketCount ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_InitializeHash PROTO_LIST( (
    HSESSION          hSession ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_InstallX PROTO_LIST( (
    HSESSION             hSession,
    int                  CertificateIndex,
    int                  AlgorithmType,
    CI_PASSWORD CI_FAR   pPassword,
    unsigned int         YSize,
    CI_Y CI_FAR          pY,
    CI_WRAPPED_X CI_FAR  pWrappedX,
    CI_RA CI_FAR         pRa,
    unsigned int         PandGSize,
    unsigned int         QSize,
    CI_P CI_FAR          pP,
    CI_Q CI_FAR          pQ,
    CI_G CI_FAR          pG ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_LoadCertificate PROTO_LIST( (
    HSESSION               hSession,
    int                    CertificateIndex,
    CI_CERT_STR CI_FAR     pCertLabel,
    CI_CERTIFICATE CI_FAR  pCertificate,
    long                   Reserved ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_LoadDSAParameters PROTO_LIST( (
    HSESSION      hSession,
    unsigned int  PandGSize,
    unsigned int  QSize,
    CI_P CI_FAR   pP,
    CI_Q CI_FAR   pQ,
    CI_G CI_FAR   pG ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_LoadInitValues PROTO_LIST( (
    HSESSION            hSession,
    CI_RANDSEED CI_FAR  pRandSeed,
    CI_KS CI_FAR        pKs ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_LoadIV PROTO_LIST( (
    HSESSION      hSession,
    CI_IV CI_FAR  pIV ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_LoadX PROTO_LIST( (
    HSESSION      hSession,
    int           CertificateIndex,
    int           AlgorithmType,
    unsigned int  PandGSize,
    unsigned int  QSize,
    CI_P CI_FAR   pP,
    CI_Q CI_FAR   pQ,
    CI_G CI_FAR   pG,
    CI_X CI_FAR   pX,
    unsigned int  YSize,
    CI_Y CI_FAR   pY ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Lock PROTO_LIST( (
    HSESSION  hSession,
    int       Flags ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Open PROTO_LIST( (
    HSESSION      hSession,
    unsigned int  Flags,
    int           SocketIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_RelayX PROTO_LIST( (
    HSESSION             hSession,
    CI_PASSWORD CI_FAR   pOldPassword,
    unsigned int         OldYSize,
    CI_Y CI_FAR          pOldY,
    CI_RA CI_FAR         pOldRa,
    CI_WRAPPED_X CI_FAR  pOldWrappedX,
    CI_PASSWORD CI_FAR   pNewPassword,
    unsigned int         NewYSize,
    CI_Y CI_FAR          pNewY,
    CI_RA CI_FAR         pNewRa,
    CI_WRAPPED_X CI_FAR  pNewWrappedX ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Reset PROTO_LIST( (
    HSESSION          hSession ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Restore PROTO_LIST( (
    HSESSION             hSession,
    int                  CryptoType,
    CI_SAVE_DATA CI_FAR  pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Save PROTO_LIST( (
    HSESSION             hSession,
    int                  CryptoType,
    CI_SAVE_DATA CI_FAR  pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Select PROTO_LIST( (
    HSESSION  hSession,
    int       SocketIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_SetConfiguration PROTO_LIST( (
    HSESSION      hSession,
    int           Type,
    unsigned int  DataSize,
    CI_DATA       pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_SetKey PROTO_LIST( (
    HSESSION     hSession,
    int  RegisterIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_SetMode PROTO_LIST( (
    HSESSION hSession,
    int      CryptoType,
    int      CryptoMode ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_SetPersonality PROTO_LIST( (
    HSESSION hSession,
    int      CertificateIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_SetTime PROTO_LIST( (
    HSESSION        hSession,
    CI_TIME CI_FAR  pTime ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Sign PROTO_LIST( (
    HSESSION             hSession,
    CI_HASHVALUE CI_FAR  pHashValue,
    CI_SIGNATURE CI_FAR  pSignature ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Terminate PROTO_LIST( (
    HSESSION          hSession ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_TimeStamp PROTO_LIST( (
    HSESSION             hSession,
    CI_HASHVALUE CI_FAR  pHashValue,
    CI_SIGNATURE CI_FAR  pSignature,
    CI_TIMESTAMP CI_FAR  pTimeStamp ) ) {
    return CI_ERROR;
}
  
RETURN_TYPE
MACI_Unlock PROTO_LIST( (
    HSESSION          hSession)  ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_UnwrapKey PROTO_LIST( (
    HSESSION       hSession,
    int            UnwrapIndex,
    int            KeyIndex,
    CI_KEY CI_FAR  pKey ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_VerifySignature PROTO_LIST( (
    HSESSION             hSession,
    CI_HASHVALUE CI_FAR  pHashValue,
    unsigned int         YSize,
    CI_Y CI_FAR          pY,
    CI_SIGNATURE CI_FAR  pSignature ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_VerifyTimeStamp PROTO_LIST( (
    HSESSION             hSession,
    CI_HASHVALUE CI_FAR  pHashValue,
    CI_SIGNATURE CI_FAR  pSignature,
    CI_TIMESTAMP CI_FAR  pTimeStamp ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_WrapKey PROTO_LIST( (
    HSESSION       hSession,
    int            WrapIndex,
    int            KeyIndex,
    CI_KEY CI_FAR  pKey ) ) {
    return CI_ERROR;
}

RETURN_TYPE
MACI_Zeroize PROTO_LIST( (
    HSESSION          hSession ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_ChangePIN PROTO_LIST( (
    int            PINType,
    CI_PIN CI_FAR  pOldPIN,
    CI_PIN CI_FAR  pNewPIN ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_CheckPIN PROTO_LIST( (
    int            PINType,
    CI_PIN CI_FAR  pPIN ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Close PROTO_LIST( (
    unsigned int   Flags,
    int            SocketIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Decrypt PROTO_LIST( (
    unsigned int   CipherSize,
    CI_DATA        pCipher,
    CI_DATA        pPlain ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_DeleteCertificate PROTO_LIST( (
    int            CertificateIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_DeleteKey PROTO_LIST( (
    int            RegisterIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Encrypt PROTO_LIST( (
    unsigned int   PlainSize,
    CI_DATA        pPlain,
    CI_DATA        pCipher ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_ExtractX PROTO_LIST( (
    int                  CertificateIndex,
    int                  AlgorithmType,
    CI_PASSWORD CI_FAR   pPassword,
    unsigned int         YSize,
    CI_Y CI_FAR          pY,
    CI_WRAPPED_X CI_FAR  pX,
    CI_RA CI_FAR         pRa,
    unsigned int         PandGSize,
    unsigned int         QSize,
    CI_P CI_FAR          pP,
    CI_Q CI_FAR          pQ,
    CI_G CI_FAR          pG ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_FirmwareUpdate PROTO_LIST( (
    unsigned long  Flags,
    long           Cksum,
    unsigned int   CksumLength,
    unsigned int   DataSize,
    CI_DATA        pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GenerateIV PROTO_LIST( (
    CI_IV CI_FAR  pIV ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GenerateMEK PROTO_LIST( (
    int  RegisterIndex,
    int  Reserved ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GenerateRa PROTO_LIST( (
    CI_RA CI_FAR   pRa ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GenerateRandom PROTO_LIST( (
    CI_RANDOM CI_FAR  pRandom ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GenerateTEK PROTO_LIST( (
    int           Flags,
    int           RegisterIndex,
    CI_RA CI_FAR  pRa,
    CI_RB CI_FAR  pRb,
    unsigned int  YSize,
    CI_Y CI_FAR   pY ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GenerateX PROTO_LIST( (
    int           CertificateIndex,
    int           AlgorithmType,
    unsigned int  PandGSize,
    unsigned int  QSize,
    CI_P CI_FAR   pP,
    CI_Q CI_FAR   pQ,
    CI_G CI_FAR   pG,
    unsigned int  YSize,
    CI_Y CI_FAR   pY ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GetCertificate PROTO_LIST( (
    int                    CertificateIndex,
    CI_CERTIFICATE CI_FAR  pCertificate ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GetConfiguration PROTO_LIST( (
    CI_CONFIG_PTR  pConfiguration ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GetHash PROTO_LIST( (
    unsigned int         DataSize,
    CI_DATA              pData,
    CI_HASHVALUE CI_FAR  pHashValue ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GetPersonalityList PROTO_LIST( (
    int               EntryCount,
    CI_PERSON CI_FAR  pPersonalityList[] ) ) {
    return CI_ERROR;
}


RETURN_TYPE
CI_GetState PROTO_LIST( (
    CI_STATE_PTR   pState ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GetStatus PROTO_LIST( (
    CI_STATUS_PTR  pStatus ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_GetTime PROTO_LIST( (
    CI_TIME CI_FAR  pTime ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Hash PROTO_LIST( (
    unsigned int  DataSize,
    CI_DATA       pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Initialize PROTO_LIST( (
    int CI_FAR  *SocketCount ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_InitializeHash PROTO_LIST( () ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_InstallX PROTO_LIST( (
    int                  CertificateIndex,
    int                  AlgorithmType,
    CI_PASSWORD CI_FAR   pPassword,
    unsigned int         YSize,
    CI_Y CI_FAR          pY,
    CI_WRAPPED_X CI_FAR  pWrappedX,
    CI_RA CI_FAR         pRa,
    unsigned int         PandGSize,
    unsigned int         QSize,
    CI_P CI_FAR          pP,
    CI_Q CI_FAR          pQ,
    CI_G CI_FAR          pG ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_LoadCertificate PROTO_LIST( (
    int                    CertificateIndex,
    CI_CERT_STR CI_FAR     pCertLabel,
    CI_CERTIFICATE CI_FAR  pCertificate,
    long                   Reserved ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_LoadDSAParameters PROTO_LIST( (
    unsigned int  PandGSize,
    unsigned int  QSize,
    CI_P CI_FAR   pP,
    CI_Q CI_FAR   pQ,
    CI_G CI_FAR   pG ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_LoadInitValues PROTO_LIST( (
    CI_RANDSEED CI_FAR  pRandSeed,
    CI_KS CI_FAR        pKs ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_LoadIV PROTO_LIST( (
    CI_IV CI_FAR  pIV ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_LoadX PROTO_LIST( (
    int           CertificateIndex,
    int           AlgorithmType,
    unsigned int  PandGSize,
    unsigned int  QSize,
    CI_P CI_FAR   pP,
    CI_Q CI_FAR   pQ,
    CI_G CI_FAR   pG,
    CI_X CI_FAR   pX,
    unsigned int  YSize,
    CI_Y CI_FAR   pY ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Lock PROTO_LIST( (
    int       Flags ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Open PROTO_LIST( (
    unsigned int  Flags,
    int           SocketIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_RelayX PROTO_LIST( (
    CI_PASSWORD CI_FAR   pOldPassword,
    unsigned int         OldYSize,
    CI_Y CI_FAR          pOldY,
    CI_RA CI_FAR         pOldRa,
    CI_WRAPPED_X CI_FAR  pOldWrappedX,
    CI_PASSWORD CI_FAR   pNewPassword,
    unsigned int         NewYSize,
    CI_Y CI_FAR          pNewY,
    CI_RA CI_FAR         pNewRa,
    CI_WRAPPED_X CI_FAR  pNewWrappedX ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Reset PROTO_LIST( ( ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Restore PROTO_LIST( (
    int                  CryptoType,
    CI_SAVE_DATA CI_FAR  pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Save PROTO_LIST( (
    int                  CryptoType,
    CI_SAVE_DATA CI_FAR  pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Select PROTO_LIST( (
    int       SocketIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_SetConfiguration PROTO_LIST( (
    int           Type,
    unsigned int  DataSize,
    CI_DATA       pData ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_SetKey PROTO_LIST( (
    int  RegisterIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_SetMode PROTO_LIST( (
    int      CryptoType,
    int      CryptoMode ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_SetPersonality PROTO_LIST( (
    int      CertificateIndex ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_SetTime PROTO_LIST( (
    CI_TIME CI_FAR  pTime ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Sign PROTO_LIST( (
    CI_HASHVALUE CI_FAR  pHashValue,
    CI_SIGNATURE CI_FAR  pSignature ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_Terminate PROTO_LIST( ( ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_TimeStamp PROTO_LIST( (
    CI_HASHVALUE CI_FAR  pHashValue,
    CI_SIGNATURE CI_FAR  pSignature,
    CI_TIMESTAMP CI_FAR  pTimeStamp ) ) {
    return CI_ERROR;
}
  
RETURN_TYPE
CI_Unlock PROTO_LIST( ()  ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_UnwrapKey PROTO_LIST( (
    int            UnwrapIndex,
    int            KeyIndex,
    CI_KEY CI_FAR  pKey ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_VerifySignature PROTO_LIST( (
    CI_HASHVALUE CI_FAR  pHashValue,
    unsigned int         YSize,
    CI_Y CI_FAR          pY,
    CI_SIGNATURE CI_FAR  pSignature ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_VerifyTimeStamp PROTO_LIST( (
    CI_HASHVALUE CI_FAR  pHashValue,
    CI_SIGNATURE CI_FAR  pSignature,
    CI_TIMESTAMP CI_FAR  pTimeStamp ) ) {
    return CI_ERROR;
}

RETURN_TYPE
CI_WrapKey PROTO_LIST( (
    int            WrapIndex,
    int            KeyIndex,
    CI_KEY CI_FAR  pKey ) ) {
    return CI_ERROR;
}

