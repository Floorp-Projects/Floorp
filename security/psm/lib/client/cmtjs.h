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
#ifndef _CMTJS_H_
#define _CMTJS_H_
#include "cmtcmn.h"
#include "ssmdefs.h"
#include "rsrcids.h"
/*
 * Define some constants.
 */

/*
 * These defines are used in conjuction with the function
 * CMT_AddNewModule.
 */
#define PUBLIC_MECH_RSA_FLAG         0x00000001ul
#define PUBLIC_MECH_DSA_FLAG         0x00000002ul
#define PUBLIC_MECH_RC2_FLAG         0x00000004ul
#define PUBLIC_MECH_RC4_FLAG         0x00000008ul
#define PUBLIC_MECH_DES_FLAG         0x00000010ul
#define PUBLIC_MECH_DH_FLAG          0x00000020ul
#define PUBLIC_MECH_FORTEZZA_FLAG    0x00000040ul
#define PUBLIC_MECH_RC5_FLAG         0x00000080ul
#define PUBLIC_MECH_SHA1_FLAG        0x00000100ul
#define PUBLIC_MECH_MD5_FLAG         0x00000200ul
#define PUBLIC_MECH_MD2_FLAG         0x00000400ul
 
#define PUBLIC_MECH_RANDOM_FLAG      0x08000000ul
#define PUBLIC_MECH_FRIENDLY_FLAG    0x10000000ul
#define PUBLIC_OWN_PW_DEFAULTS       0X20000000ul
#define PUBLIC_DISABLE_FLAG          0x40000000ul


/*
 * This is the lone supported constant for the Cipher flag
 * for CMT_AddNewModule
 */
#define PUBLIC_CIPHER_FORTEZZA_FLAG  0x00000001ul

CMT_BEGIN_EXTERN_C

/*
 * FUNCTION: CMT_GenerateKeyPair
 * -----------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    keyGenContext
 *        The Resource ID of a key gen context to use for creating the
 *        key pair.
 *    mechType
 *        A PKCS11 mechanism used to generate the key pair. Valid values are:
 *          CKM_RSA_PKCS_KEY_PAIR_GEN    0x00000000
 *          CKM_DSA_KEY_PAIR_GEN         0x00000010
 *        The definition of these values can be found at 
 *        http://www.rsa.com/rsalabs/pubs/pkcs11.html
 *        The psm module currently supports v2.01 of PKCS11
 *    params
 *        This parameter will be used to pass parameters to the Key Pair
 *        generation process.  Currently this feature is not supported, so 
 *        pass in NULL for this parameter.
 *    keySize
 *        The size (in bits) of the key to generate.
 *    keyPairId
 *        A pointer to pre-allocated memory where the function can place
 *        the value of the resource ID of the key pair that gets created.
 *
 * NOTES:
 * This function will send a message to the psm server requesting that 
 * a public/private key pair be generated. The key gen context will queue
 * the request. You can send as many key gen requests as you want with a
 * given key gen context. After sending all the key gen requests, the user
 * must call CMT_FinishGeneratingKeys so that the key gen context actually
 * generates the keys.
 *
 * RETURN:
 * A return value of CMTSuccess indicates the request for key generation
 * was queued successfully and the corresponding resource ID can be found
 * at *keyPairId.  Any other return value indicates an error and the value
 * at *keyPairId should be ignored.
 */
CMTStatus
CMT_GenerateKeyPair(PCMT_CONTROL control, CMUint32 keyGenContext, 
		    CMUint32 mechType, CMTItem *params, CMUint32 keySize, 
		    CMUint32 *keyPairId);

/*
 * FUNCTION: CMT_FinishGeneratingKeys
 * ----------------------------------
 * INPUTS
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    keyGenContext
 *        The resource ID of the key gen context which should finish 
 *        generating its key pairs.
 * NOTES
 * This function will send a message to the psm server notifying the key 
 * gen context with the resource ID of keyGenContext to finish generating
 * all of the key gen requests it has queued up. After each key gen has 
 * finished, the psm server will send a SSM_TASK_COMPLETED_EVENT. So in order
 * to detect when all of the key gens are done, the user should register 
 * an event handler.  See comments for CMT_RegisterEventHandler for information
 * on how to successfully register event handler callbacks. You must register
 * the event handler with keyGenContext as the target resource ID for this
 * to work correctly.
 *
 * RETURN:
 * A return value of CMTSuccess indicates the key gen context has started to
 * generate the key pairs in its queue. Any other return value indicates an
 * error and the key pairs will not be generated.
 */
CMTStatus
CMT_FinishGeneratingKeys(PCMT_CONTROL control, CMUint32 keyGenContext);

/*
 * FUNCTION: CMT_CreateNewCRMFRequest
 * ----------------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    keyPairID
 *        The resource ID of the key pair that should be associated with
 *        the CRMF request created. At the time this function is called,
 *        key pair should have already been created. 
 *    keyGenType
 *        An enumeration that explains how the key pair will be used.
 *        Look at the definition of SSMKeyGenType in ssmdefs.h for valid
 *        values and their affects on the request.
 *    reqID
 *        A pointer to a pre-allocatd chunk of memory where the library 
 *        can place the resource ID of the new CRMF request.
 * NOTES:
 * This function sends a message to the psm server requesting that a new 
 * CRMF resource object be created. Each CRMF request must be associated with 
 * a public/private key pair, that is why the keyPairID parameter exists.
 * The keyGenType parameter is used to initialize the request, eg set the
 * correct keyUsage extension.
 * 
 * Before encoding a CRMF request, the user will want to set the appropriate
 * attributes to build up the request. The supported attributes are:
 *
 * Attribute Enumeration          Attribute Type       What value means
 * ---------------------          --------------       ----------------
 * SSM_FID_CRMFREQ_REGTOKEN       String               The value to encode as 
 *                                                     the registration token
 *                                                     value for the request.
 *
 * SSM_FID_CRMFREQ_AUTHENTICATOR String                The value to encode as
 *                                                     authenticator control
 *                                                     in the request.
 * 
 * SSM_FID_DN                    String                The RFC1485 formatted
 *                                                     DN to include in the
 *                                                     CRMF request.
 *
 * For information on how to properly set the attribute of a resource, refer
 * to the comments for the functions CMT_SetNumericAttribute and 
 * CMT_SetStringAttribute.
 *
 * RETURN:
 * A return value of CMTSuccess indicates a new CRMF resource was created by
 * the psm server and has the resource ID placed at *reqID. Any other return
 * value indicates an error and the value at *reqID should be ignored.
 */
CMTStatus
CMT_CreateNewCRMFRequest(PCMT_CONTROL control, CMUint32 keyPairID, 
			 SSMKeyGenType keyGenType, CMUint32 *reqID);

/*
 * FUNCTION: CMT_EncodeCRMFRequest
 * ------------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    crmfReqID
 *        An array of resource ID's for CRMF objects to be encoded.
 *    numRequests
 *        The length of the array crmfReqID that is passed in.
 *    der
 *        A pointer to a pre-allocated pointer for a char* where the library
 *        can place the final DER-encoding of the requests.
 * NOTES
 * This function will send a message to the psm server requesting that 
 * a number of CRMF requests be encoded into their appropriate DER 
 * representation. The DER that is sent back will be of the type 
 * CertReqMessages as define in the internet draft for CRMF. To look at the
 * draft, visit the following URL: 
 * http://search.ietf.org/internet-drafts/internet-draft-ietf-pkix-crmf-01.txt
 *
 * RETURN:
 * A return value of CMTSuccess indicates psm successfully encoded the requests
 * and placed the base64 DER encoded request at *der. Any other return value
 * indicates an error and the value at *der should be ignored.
 */
CMTStatus
CMT_EncodeCRMFRequest(PCMT_CONTROL control, CMUint32 *crmfReqID, 
		      CMUint32 numRequests, char ** der);

/*
 * FUNCTION: CMT_ProcessCMMFResponse
 * ---------------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    nickname
 *        The nickname that should be associated with the certificate 
 *        contained in the CMMF Response.
 *    certRepString
 *        This is the base 64 encoded CertRepContent that issues a certificate.
 *        The psm server will decode the base 64 data and then parse the
 *        CertRepContent.
 *    doBackup
 *        A boolean value indicating whether or not psm should initiate the
 *        process of backing up the newly issued certificate into a PKCS-12
 *        file.
 *    clientContext
 *         Client supplied data pointer that is returned to the client during
 *         a UI event.
 * NOTES:
 * This function takes a CertRepContent as defined in the CMMF internet draft
 * (http://search.ietf.org/internet-drafts/draft-ietf-pkix-cmmf-02.txt) and
 * imports the certificate into the user's database. The certificate will have
 * the string value of nickanme as it's nickname when added to the database
 * unless another certificate with that same Distinguished Name (DN) already
 * exists in the database, in which case the nickname of the certificate that
 * already exists will be used. If the value passed in for doBackup is 
 * non-zero, then the psm server will initiate the process of backing up the
 * certificate(s) that were just imported.
 *
 * RETURN:
 * A return value of CMTSuccess indicates the certificate(s) were successfully
 * added to the database. Any other return value means the certificate(s) could
 * not be successfully added to the database.
 */
CMTStatus
CMT_ProcessCMMFResponse(PCMT_CONTROL control, char *nickname, 
			char *certRepString, CMBool doBackup,
			void *clientContext);

/*
 * FUNCTION: CMT_CreateResource
 * ----------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    resType
 *        The enumeration representing the resource type to create.
 *    params
 *        A resource dependent binary string that will be sent to the psm 
 *        server. Each resource will expect a binary string it defines.
 *    rsrcId
 *        A pointer to a pre-allocated chunk of memory where the library
 *        can place the resource ID of the newly created resource.
 *    errorCode
 *        A pointer to a pre-allocated chunk of memory where the library
 *        can place the errorCode returned by the psm server after creating
 *        the resource.
 * NOTES:
 * This function sends a message to the psm server requesting that a new 
 * resource be created.  The params parameter depends on the type of resource
 * being created.  Below is a table detailing the format of the params for 
 * a given resource type. Only the resource types listed below can be created
 * by calling this function.
 *
 * Resource Type constant                  Value for params
 * ------------------------------          ----------------
 * SSM_RESTYPE_KEYGEN_CONTEXT              NULL
 * SSM_RESTYPE_SECADVISOR_CONTEXT          NULL
 * SSM_RESTYPE_SIGNTEXT                    NULL
 *
 * RETURN
 * A return value of CMTSuccess means the psm server received the request and
 * processed the create resource create. If the value at *errorCode is zero,
 * then the value at *rsrcId is the resource ID of the newly created resource.
 * Otherwise, creating the new resource failed and *errorCode contains the
 * error code returned by the psm server. ???What are the return values and
 * what do they mean. Any other return value indicates there was an error 
 * in the communication with the psm server and the values at *rsrcId and 
 * *errorCode should be ignored.
 */
CMTStatus
CMT_CreateResource(PCMT_CONTROL control, SSMResourceType resType,
		   CMTItem *params, CMUint32 *rsrcId, CMUint32 *errorCode);

/*
 * FUNCTION: CMT_SignText
 * ----------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    resID
 *        The resource ID of an SSMSignTextResource.
 *    stringToSign
 *        The string that the psm server should sign.
 *    hostName
 *        The host name of the site that is requesting a string to be
 *        signed.  This is used for displaying the UI that tells the user
 *        a web site has requested the use sign some text.
 *    caOption
 *        If the value is "auto" then psm will select the certificate
 *        to use for signing automatically.
 *        If the value is "ask" then psm will display a list of 
 *        certificates for signing.
 *    numCAs
 *        The number of CA names included in the array caNames passed in as
 *        the last parameter to this function.
 *    caNames
 *        An array of CA Names to use for filtering the user certs to use
 *        for signing the text.
 * NOTES
 * This function will sign the text passed via the parameter stringToSign.
 * The function will also cause the psm server to send some UI notifying the
 * user that a site has requested the user sign some text.  The hostName 
 * parameter is used in the UI to inform the user which site is requesting
 * the signed text.  The caOption is used to determine if the psm server 
 * should automatically select which personal cert to use in signing the
 * text.  The caNames array is ussed to narrow down the field of personal
 * certs to use when signing the text. In other words, only personal certs 
 * trusted by the CA's passed in will be used.
 *
 * RETURN
 * If the function returns CMTSuccess, that indicates the psm server 
 * successfully signed the text.  The signed text can be retrieved by 
 * calling CMT_GetStringResource and passing in SSM_FID_SIGNTEXT_RESULT
 * as the field ID. Any other return value indicates an error meaning the
 * string was not signed successfully.
 */
CMTStatus
CMT_SignText(PCMT_CONTROL control, CMUint32 resID, char* stringToSign,
             char* hostName, char *caOption, CMInt32 numCAs, char** caNames);

/*
 * FUNCTION: CMT_ProcessChallengeResponse
 * --------------------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    challengeString
 *        The base64 encoded Challenge string received as the 
 *        Proof-Of-Possession Challenge in response to CRMF request that
 *        specified Challenge-Reponse as the method for Proof-Of-Possession.
 *    responseString
 *        A pointer to pre-allocated char* where the library can place a
 *        copy of the bas64 encoded response to the challenge presented.
 * NOTES
 * This function takes the a challenge--that is encrypted with the public key
 * of a certificate we created--and decrypts it with the private key we 
 * generated.  The format of the challenge is as follows:
 *
 * Challenge ::= SEQUENCE { 
 *      owf                 AlgorithmIdentifier  OPTIONAL, 
 *      -- MUST be present in the first Challenge; MAY be omitted in any 
 *      -- subsequent Challenge in POPODecKeyChallContent (if omitted, 
 *      -- then the owf used in the immediately preceding Challenge is 
 *      -- to be used). 
 *      witness             OCTET STRING, 
 *      -- the result of applying the one-way function (owf) to a 
 *      -- randomly-generated INTEGER, A.  [Note that a different 
 *      -- INTEGER MUST be used for each Challenge.] 
 *      sender              GeneralName, 
 *      -- the name of the sender. 
 *      key                 OCTET STRING, 
 *      -- the public key used to encrypt the challenge.  This will allow 
 *      -- the client to find the appropriate key to do the decryption. 
 *      challenge           OCTET STRING 
 *      -- the encryption (under the public key for which the cert. 
 *      -- request is being made) of Rand, where Rand is specified as 
 *      --   Rand ::= SEQUENCE { 
 *      --      int      INTEGER, 
 *      --       - the randomly-generated INTEGER A (above) 
 *      --      senderHash  OCTET STRING 
 *      --       - the result of applying the one-way function (owf) to 
 *      --       - the sender's general name 
 *      --   } 
 *      -- the size of "int" must be small enough such that "Rand" can be 
 *      -- contained within a single PKCS #1 encryption block. 
 *  } 
 * This challenge is based on the Challenge initially defined in the CMMF
 * internet draft, but differs in that this structure includes the sender
 * as part of the challenge along with the public key and includes a has
 * of the sender in the encrypted Rand structure.  The reason for including
 * the key is to facilitate looking up the key that should be used to 
 * decipher the challenge.  Including the hash of the sender in the encrypted
 * Rand structure makes the challenge smaller and allows it to fit in 
 * one RSA block. 
 *
 * The response is of the type POPODecKeyRespContent as defined in the CMMF
 * internet draft.
 *
 * RETURN
 * A return value of CMTSuccess indicates psm successfully parsed and processed
 * the challenge and created a response.  The base64 encoded response to the
 * challenge is placed at *responseString.  Any other return value indicates
 * an error and the value at *responseString should be ignored.
 */
CMTStatus
CMT_ProcessChallengeResponse(PCMT_CONTROL control, char *challengeString,
			     char **responseString);

/*
 * FUNCTION: CMT_GetLocalizedString
 * --------------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    whichString
 *        The enumerated value corresponding to the localized string to 
 *        retrieve from the psm server
 *    localizedString
 *        A pointer to a pre-allocated char* where the library can place 
 *        copy of the localized string retrieved from the psm server.
 * NOTES
 * This function retrieves a localized string from the psm server.  These 
 * strings are useful for strings that aren't localized in the client 
 * making use of the psm server, but need to be displayed by the user. Look
 * in protocol.h for the enumerations of the localized strings that can 
 * be fetched from psm via this method.
 *
 * RETURN
 * A return value of CMTSuccess indicates the localized string was retrieved
 * successfully and the localized value is located at *localizedString.  Any
 * other return value indicates an error and the value at *localizedString
 * should be ignored.
 */
CMTStatus 
CMT_GetLocalizedString(PCMT_CONTROL        control, 
                       SSMLocalizedString  whichString,
                       char              **localizedString); 

/*
 * FUNCTION: CMT_DeleteModule
 * --------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    moduleName
 *        The name of the PKCS11 module to delete.
 *    moduleType
 *        A pointer to a pre-allocated integer where the library can place
 *        a value that tells what the type of module was deleted.
 * NOTES
 * This function will send a message to the psm server requesting the server 
 * delete a PKCS-11 module stored in psm's security module database. moduleName
 * is the value passed in as moduleName when the module was added to the 
 * security module database of psm.
 * The values that may be returned by psm for moduleType are:
 *
 *    0      The module was an external module developped by a third party
 *           that was added to the psm security module.
 *
 *    1      The module deleted was the internal PKCS-11 module that comes
 *           built in with the psm server.
 *
 *    2      The module that was deleted was the FIPS  internal module.
 *
 * RETURN
 * A return value of CMTSuccess indicates the security module was successfully
 * delete from the psm security module database and the value at *moduleType
 * will tell what type of module was deleted.
 * Any other return value indicates an error and the value at *moduleType 
 * should be ignored.
 */
CMTStatus
CMT_DeleteModule(PCMT_CONTROL  control,
                 char         *moduleName,
                 int          *moduleType);


/*
 * FUNCTION: CMT_AddNewModule
 * --------------------------
 * INPUTS:
 *    control
 *        The Control Connection that has already established a connection
 *        with the psm server.
 *    moduleName
 *        The name to be associated with the module once it is added to 
 *        the psm security module database.
 *    libraryPath
 *        The path to the library to be  loaded.  The library should be 
 *        loadable at run-time.
 *    pubMechFlags
 *        A bit vector indicating all cryptographic mechanisms that should
 *        be turned on by default.  This module will become the default 
 *        handler for the mechanisms that are set by this bit vector.
 *    pubCipherFlags
 *        A bit vector indicating all SSL or S/MIME cipher functions
 *        supported by the module. Most modules will pas in 0x0 for this
 *        parameter.
 * NOTES:
 * This function sends a message to the psm server and requests the .so
 * file on UNIX or .dll file on Windows be loaded as a PKCS11 module and 
 * be stored in the psm security module database.  The module will be stored
 * with the name moduleName that is passed in and will always expect the 
 * library to live at the path passed in via the parameter libraryPath.
 * The pubMechFlags tell the psm server how this module should be used.
 * Valid values are the #define constants defined at the beginning of
 * this file.
 * 
 * RETURN
 * A return value of CMTSuccess indicates the module was successfully loaded
 * and placed in the security module database of psm.  Any other return value
 * indicates an error and means the module was not loaded successfully and
 * not stored in the psm server's security module database.
 */
CMTStatus
CMT_AddNewModule(PCMT_CONTROL  control,
                 char         *moduleName,
                 char         *libraryPath,
                 unsigned long pubMechFlags,
                 unsigned long pubCipherFlags);

CMT_END_EXTERN_C

#endif /*_CMTJS_H_*/
