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
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
#include "_jni/org_mozilla_jss_CryptoManager.h"

#include <plarena.h>
#include <secmodt.h>
#include <pk11func.h>
#include <secerr.h>
#include <nspr.h>
#include <cert.h>
#include <certdb.h>
#include <key.h>
#include <secpkcs7.h>

#include <jssutil.h>

#include <jss_exceptions.h>
#include "pk11util.h"
#include <java_ids.h>

/*
 * This is a semi-private NSS function, exposed only for JSS.
 */
SECStatus
CERT_ImportCAChainTrusted(SECItem *certs, int numcerts, SECCertUsage certUsage);

/*****************************************************************
 *
 * CryptoManager. f i n d C e r t B y N i c k n a m e N a t i v e
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_findCertByNicknameNative
  (JNIEnv *env, jobject this, jstring nickname)
{
    char *nick=NULL;
    jobject certObject=NULL;
    CERTCertificate *cert=NULL;
    PK11SlotInfo *slot=NULL;

    PR_ASSERT(env!=NULL && this!=NULL && nickname!=NULL);

    nick = (char*) (*env)->GetStringUTFChars(env, nickname, NULL);
    PR_ASSERT(nick!=NULL);

    cert = JSS_PK11_findCertAndSlotFromNickname(nick, NULL, &slot);

    if(cert == NULL) {
        JSS_nativeThrow(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    }

    certObject = JSS_PK11_wrapCertAndSlot(env, &cert, &slot);

finish:
    if(nick != NULL) {
        (*env)->ReleaseStringUTFChars(env, nickname, nick);
    }
    if(cert != NULL) {
        CERT_DestroyCertificate(cert);
    }
    if(slot != NULL) {
        PK11_FreeSlot(slot);
    }
    return certObject;
}

/*****************************************************************
 *
 * CryptoManager. f i n d C e r t s B y N i c k n a m e N a t i v e
 *
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_findCertsByNicknameNative
  (JNIEnv *env, jobject this, jstring nickname)
{
    CERTCertList *list =NULL;
    PK11SlotInfo *slot =NULL;
    jobjectArray certArray=NULL;
    CERTCertListNode *node;
    const char *nickChars=NULL;
    jboolean charsAreCopied;
    jclass certClass;
    int count;
    int i;

    /* convert the nickname string */
    nickChars = (*env)->GetStringUTFChars(env, nickname, &charsAreCopied);
    if( nickChars == NULL ) {
        goto finish;
    }

    /* get the list of certs with the given nickname */
    list = JSS_PK11_findCertsAndSlotFromNickname( (char*)nickChars,
        NULL /*wincx*/, &slot);
    if( list == NULL ) {
        count = 0;
    } else {
        /* Since this structure changed in NSS_2_7_RTM (the reference */
        /* to "count" was removed from the "list" structure) we must  */
        /* now count up the number of nodes manually!                 */
        for( node = CERT_LIST_HEAD(list), count=0;
             ! CERT_LIST_END(node, list);
             node = CERT_LIST_NEXT(node), count++ );
    }
    PR_ASSERT(count >= 0);

    /* create the cert array */
    certClass = (*env)->FindClass(env, X509_CERT_CLASS);
    if( certClass == NULL ) {
        goto finish;
    }
    certArray = (*env)->NewObjectArray(env, count, certClass, NULL);
    if( certArray == NULL ) {
        /* exception was thrown */
        goto finish;
    }

    if( list == NULL ) {
        goto finish;
    }

    /* traverse the list, placing each cert into the array */
    for(    node = CERT_LIST_HEAD(list), i=0;
            ! CERT_LIST_END(node, list);
            node = CERT_LIST_NEXT(node), i++       )     {

        CERTCertificate *cert;
        PK11SlotInfo *slotCopy;
        jobject certObj;

        /* Create a Java certificate object from the current CERTCertificate */
        cert = CERT_DupCertificate(node->cert);
        slotCopy = PK11_ReferenceSlot(slot);
        certObj = JSS_PK11_wrapCertAndSlot(env, &cert, &slotCopy);
        if( certObj == NULL ) {
            goto finish;
        }

        /* put the Java certificate into the next element in the array */
        (*env)->SetObjectArrayElement(env, certArray, i, certObj);

        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        }
    }

    /* sanity check */
    PR_ASSERT( i == count );

finish:
    if(list) {
        CERT_DestroyCertList(list);
    }
    if(slot) {
        PK11_FreeSlot(slot);
    }
    if( nickChars && charsAreCopied ) {
        (*env)->ReleaseStringUTFChars(env, nickname, nickChars);
    }
    return certArray;
}

/*****************************************************************
 *
 * CryptoManager.findCertByIssuerAndSerialNumberNative
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_findCertByIssuerAndSerialNumberNative
  (JNIEnv *env, jobject this, jbyteArray issuerBA, jbyteArray serialNumBA)
{
    jobject certObject=NULL;
    CERTCertificate *cert=NULL;
    SECItem *issuer=NULL, *serialNum=NULL;
    CERTIssuerAndSN issuerAndSN;
    PK11SlotInfo *slot=NULL;

    PR_ASSERT(env!=NULL && this!=NULL);

    /* validate args */
    if( issuerBA == NULL || serialNumBA == NULL ) {
        JSS_throwMsg(env, ILLEGAL_ARGUMENT_EXCEPTION,
            "NULL parameter passed to CryptoManager.findCertByIssuer"
            "AndSerialNumberNative");
        goto finish;
    }

    /* convert byte arrays to SECItems */
    issuer = JSS_ByteArrayToSECItem(env, issuerBA);
    if( issuer == NULL ) {
        goto finish; }
    serialNum = JSS_ByteArrayToSECItem(env, serialNumBA);
    if( serialNum == NULL ) {
        goto finish; }
    issuerAndSN.derIssuer = *issuer;
    issuerAndSN.serialNumber = *serialNum;

    /* lookup with PKCS #11 first, then use cert database */
    cert = PK11_FindCertByIssuerAndSN(&slot, &issuerAndSN, NULL /*wincx*/);
    if( cert == NULL ) {
        JSS_nativeThrow(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    }

    certObject = JSS_PK11_wrapCertAndSlot(env, &cert, &slot);

finish:
    if(slot) {
        PK11_FreeSlot(slot);
    }
    if(cert != NULL) {
        CERT_DestroyCertificate(cert);
    }
    if(issuer) {
        SECITEM_FreeItem(issuer, PR_TRUE /*freeit*/);
    }
    if(serialNum) {
        SECITEM_FreeItem(serialNum, PR_TRUE /*freeit*/);
    }
    return certObject;
}

/*****************************************************************
 *
 * CryptoManager. f i n d P r i v K e y B y C e r t N a t i v e
 *
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_findPrivKeyByCertNative
  (JNIEnv *env, jobject this, jobject Cert)
{
    PRThread *pThread;
    CERTCertificate *cert;
    PK11SlotInfo *slot;
    SECKEYPrivateKey *privKey=NULL;
    jobject Key = NULL;

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT( pThread != NULL);
    PR_ASSERT( env!=NULL && this!=NULL && Cert!=NULL);

    if( JSS_PK11_getCertPtr(env, Cert, &cert) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    if(cert==NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throw(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    }
    if( JSS_PK11_getCertSlotPtr(env, Cert, &slot) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    if(slot==NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throw(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    }

    privKey = PK11_FindPrivateKeyFromCert(slot, cert, NULL);
    if(privKey == NULL) {
        JSS_throw(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    }

    Key = JSS_PK11_wrapPrivKey(env, &privKey);

finish:
    if(privKey != NULL) {
        SECKEY_DestroyPrivateKey(privKey);
    }
    PR_DetachThread();
    return Key;
}


/***********************************************************************
 * Node in linked list of certificates
 */
typedef struct JSScertNode {
    struct JSScertNode *next;
    CERTCertificate *cert;
} JSScertNode;


/***********************************************************************
 *
 * c e r t _ c h a i n _ f r o m _ c e r t
 *
 * Builds a certificate chain from a certificate. Returns a Java array
 * of PK11Certs.
 *
 * INPUTS:
 *      env
 *          The JNI environment. Must not be NULL.
 *      handle
 *          The certificate database in which to search for the certificate
 *          chain.  This should usually be the default cert db. Must not
 *          be NULL.
 *      leaf
 *          A CERTCertificate that is the leaf of the cert chain. Must not
 *          be NULL.
 * RETURNS:
 *      NULL if an exception was thrown, or
 *      A Java array of PK11Cert objects which constitute the chain of
 *      certificates. The chains starts with the one passed in and
 *      continues until either a self-signed root is found or the next
 *      certificate in the chain cannot be found. At least one cert will
 *      be in the chain: the leaf certificate passed in.
 */
static jobjectArray
cert_chain_from_cert(JNIEnv *env, CERTCertDBHandle *handle,
    CERTCertificate *leaf)
{
    CERTCertificate *c;
    int i, len = 0;
    JSScertNode *head=NULL, *tail, *node;
    jobjectArray certArray = NULL;
    jclass certClass;

    PR_ASSERT(env!=NULL && handle!=NULL && leaf!=NULL);

    head = tail = (JSScertNode*) PR_CALLOC( sizeof(JSScertNode) );
    if (head == NULL) goto no_memory;

    /* put primary cert first in the linked list */
    head->cert = c = CERT_DupCertificate(leaf);
    head->next = NULL;
    PR_ASSERT(c != NULL); /* CERT_DupCertificate really can't return NULL */
    len++;

    /*
     * add certs until we come to a self-signed one
     */
    while(SECITEM_CompareItem(&c->derIssuer, &c->derSubject) != SECEqual) {
        c = CERT_FindCertByName(handle, &tail->cert->derIssuer);
        if (c == NULL) break;

        tail->next = (JSScertNode*) PR_CALLOC( sizeof(JSScertNode) );
        tail = tail->next;
        if (tail == NULL) goto no_memory;

        tail->cert = c;
        len++;
    }

    /*
     * Turn the cert chain into a Java array of certificates
     */
    certClass = (*env)->FindClass(env, CERT_CLASS_NAME);
    if(certClass==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    certArray = (*env)->NewObjectArray(env, len, certClass, (jobject)NULL);
    if(certArray==NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    /* convert linked list to array, freeing the linked list as we go */
    for( i=0; head != NULL; ++i ) {
        jobject certObj;

        node = head;

        PR_ASSERT(i < len);
        PR_ASSERT(node->cert != NULL);

        /* Convert C cert to Java cert */
        certObj = JSS_PK11_wrapCert(env, &node->cert);
        PR_ASSERT( node->cert == NULL );
        if(certObj == NULL) {
            PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL );
            goto finish;
        }

        /* Insert Java cert into array */
        (*env)->SetObjectArrayElement(env, certArray, i, certObj);
        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        }

        /* Free this list element */
        head = head->next;
        PR_Free(node);
    }

    goto finish;
no_memory:
    JSS_throw(env, OUT_OF_MEMORY_ERROR);
finish:
    /* Free the linked list of certs if it hasn't been deleted already */
    while(head != NULL) {
        node = head;
        head = head->next;
        if (node->cert != NULL) {
            CERT_DestroyCertificate(node->cert);
        }
        PR_Free(node);
    }

    return certArray;
}

/*****************************************************************
 *
 * CryptoManager. b u i l d C e r t i f i c a t e C h a i n N a t i v e
 *
 * INPUTS:
 *      env
 *          The JNI environment. Must not be NULL.
 *      this
 *          The PK11Finder object. Must not be NULL.
 *      leafCert
 *          A PK11Cert object from which a cert chain will be built.
 *          Must not be NULL.
 * RETURNS:
 *      NULL if an exception occurred, or
 *      An array of PK11Certs, the cert chain, with the leaf at the bottom.
 *      There will always be at least one element in the array (the leaf).
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_buildCertificateChainNative
    (JNIEnv *env, jobject this, jobject leafCert)
{
    PRThread *pThread;
    CERTCertificate *leaf;
    jobjectArray chainArray=NULL;
    CERTCertDBHandle *certdb;

    pThread = PR_AttachThread(PR_SYSTEM_THREAD, 0, NULL);
    PR_ASSERT(pThread != NULL);

    PR_ASSERT(env!=NULL && this!=NULL && leafCert!=NULL);

    if( JSS_PK11_getCertPtr(env, leafCert, &leaf) != PR_SUCCESS) {
        JSS_throwMsgPrErr(env, CERTIFICATE_EXCEPTION,
            "Could not extract pointer from PK11Cert");
        goto finish;
    }
    PR_ASSERT(leaf!=NULL);

    certdb = CERT_GetDefaultCertDB();
    if(certdb == NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "No default certificate database has been registered");
        goto finish;
    }

    /* Get the cert chain */
    chainArray = cert_chain_from_cert(env, certdb, leaf);
    if(chainArray == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }

finish:

    PR_DetachThread();
    return chainArray;
}

/***********************************************************************
 * DERCertCollection
 */
typedef struct {
    SECItem *derCerts;
    int numCerts;
} DERCertCollection;

/***********************************************************************
 * c o l l e c t _ c e r t s
 *
 * Copies certs into a new array.
 *
 * 'arg' is a pointer to a DERCertCollection structure, which will be filled in.
 * 'certs' is an array of pointers to SECItems.
 */
static SECStatus
collect_der_certs(void *arg, SECItem **certs, int numcerts)
{
    int itemsCopied=0;
    SECItem *certCopies; /* array of SECItem */
    SECStatus rv;

    PR_ASSERT(arg!=NULL);

    certCopies = PR_MALLOC( sizeof(SECItem) * numcerts);
    ((DERCertCollection*)arg)->derCerts = certCopies;
    ((DERCertCollection*)arg)->numCerts = numcerts;
    if(certCopies == NULL) {
        return SECFailure;
    }
    for(itemsCopied=0; itemsCopied < numcerts; itemsCopied++) {
        rv=SECITEM_CopyItem(NULL, &certCopies[itemsCopied], certs[itemsCopied]);
        if( rv == SECFailure ) {
            goto loser;
        }
    }
    PR_ASSERT(itemsCopied == numcerts);

    return SECSuccess;

loser:
    for(; itemsCopied >= 0; itemsCopied--) {
        SECITEM_FreeItem( &certCopies[itemsCopied], PR_FALSE /*freeit*/);
    }
    PR_Free( certCopies );
    ((DERCertCollection*)arg)->derCerts = NULL;
    ((DERCertCollection*)arg)->numCerts = 0;
    return SECFailure;
}

/***********************************************************************
 * CryptoManager.importCertToPerm
 *  - add the certificate to the permanent database
 *
 * throws TOKEN_EXCEPTION
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_importCertToPermNative
    (JNIEnv *env, jobject this, jobject cert, jstring nickString)
{
    SECStatus rv;
    CERTCertificate *oldCert;
    jobject          result=NULL;
    char *nickname=NULL;
    CERTCertificate **certArray = NULL;
    SECItem *derCertArray[1];
    PK11SlotInfo *slot;

    /* first, get the NSS cert pointer from the 'cert' object */

    if ( JSS_PK11_getCertPtr(env, cert, &oldCert) != PR_SUCCESS) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) != NULL);
        goto finish;
    }
    PR_ASSERT(oldCert != NULL);

    if (nickString != NULL) {
        nickname = (char*) (*env)->GetStringUTFChars(env, nickString, NULL);
    }
    /* Then, add to permanent database */

    derCertArray[0] = &oldCert->derCert;
    rv = CERT_ImportCerts(CERT_GetDefaultCertDB(), -1 /* usage */,
            1, derCertArray, &certArray, PR_TRUE /*keepCerts*/,
            PR_FALSE /*caOnly*/, nickname);
    if( rv != SECSuccess || certArray == NULL || certArray[0] == NULL) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION, "Unable to insert certificate"
                " into permanent database");
        goto finish;
    }
    slot = PK11_GetInternalKeySlot();  /* the permanent database token */
    result = JSS_PK11_wrapCertAndSlot(env, &certArray[0], &slot);

finish:
    /* this checks for NULL */
    CERT_DestroyCertArray(certArray, 1);
    if (nickname != NULL) {
        (*env)->ReleaseStringUTFChars(env, nickString, nickname);
    }
    return result;
}

static unsigned char*
data_start(unsigned char *buf, int length, unsigned int *data_length,
    PRBool includeTag)
{
    unsigned char tag;
    int used_length= 0;

    tag = buf[used_length++];

    /* blow out when we come to the end */
    if (tag == 0) {
        return NULL;
    }

    *data_length = buf[used_length++];

    if (*data_length&0x80) {
        int  len_count = *data_length & 0x7f;

        *data_length = 0;

        while (len_count-- > 0) {
            *data_length = (*data_length << 8) | buf[used_length++];
        }
    }

    if (*data_length > (length-used_length) ) {
        *data_length = length-used_length;
        return NULL;
    }
    if (includeTag) *data_length += used_length;

    return (buf + (includeTag ? 0 : used_length));
}

static PRStatus
getCertFields(SECItem *derCert, SECItem *issuer,
                     SECItem *serial, SECItem *subject)
{
    unsigned char *buf;
    unsigned int buf_length;
    unsigned char *date;
    unsigned int datelen;
    unsigned char *cert = derCert->data;
    unsigned int cert_length = derCert->len;

    /* get past the signature wrap */
    buf = data_start(cert,cert_length,&buf_length,PR_FALSE);
    if (buf == NULL) return PR_FAILURE;

    /* get into the raw cert data */
    buf = data_start(buf,buf_length,&buf_length,PR_FALSE);
    if (buf == NULL) return PR_FAILURE;

    /* skip past any optional version number */
    if ((buf[0] & 0xa0) == 0xa0) {
        date = data_start(buf,buf_length,&datelen,PR_FALSE);
        if (date == NULL) return PR_FAILURE;
        buf_length -= (date-buf) + datelen;
        buf = date + datelen;
    }

    /* serial number */
    serial->data = data_start(buf,buf_length,&serial->len,PR_FALSE);
    if (serial->data == NULL) return PR_FAILURE;
    buf_length -= (serial->data-buf) + serial->len;
    buf = serial->data + serial->len;

    /* skip the OID */
    date = data_start(buf,buf_length,&datelen,PR_FALSE);
    if (date == NULL) return PR_FAILURE;
    buf_length -= (date-buf) + datelen;
    buf = date + datelen;

    /* issuer */
    issuer->data = data_start(buf,buf_length,&issuer->len,PR_TRUE);
    if (issuer->data == NULL) return PR_FAILURE;
    buf_length -= (issuer->data-buf) + issuer->len;
    buf = issuer->data + issuer->len;

    /* skip the date */
    date = data_start(buf,buf_length,&datelen,PR_FALSE);
    if (date == NULL) return PR_FAILURE;
    buf_length -= (date-buf) + datelen;
    buf = date + datelen;

    /*subject */
    subject->data=data_start(buf,buf_length,&subject->len,PR_TRUE);
    if (subject->data == NULL) return PR_FAILURE;
    buf_length -= (subject->data-buf) + subject->len;
    buf = subject->data +subject->len;

    /*subject */
    return PR_SUCCESS;
}


/**
 * Returns
 *   -1 if operation error.
 *    0 if no leaf found.
 *    1 if leaf is found
 */
static int find_child_cert(
  CERTCertDBHandle *certdb,
  SECItem *derCerts,
  int numCerts,
  int *linked,
  int cur_link,
  int *leaf_link
)
{
    int i;
    int status = 0;
    SECItem parentIssuer, parentSerial, parentSubject;
    PRStatus decodeStatus;

    decodeStatus = getCertFields(&derCerts[cur_link], &parentIssuer,
        &parentSerial, &parentSubject);
    if( decodeStatus != PR_SUCCESS ) {
        status = -1;
        goto finish;
    }

    for (i=0; i<numCerts; i++) {
        SECItem childIssuer, childSerial, childSubject;

        if (linked[i] == 1) {
            continue;
        }
        status = getCertFields(&derCerts[i], &childIssuer, &childSerial,
                                &childSubject);
        if( status != PR_SUCCESS ) {
            status = -1;
            goto finish;
        }
        if (SECITEM_CompareItem(&parentSubject, &childIssuer) == SECEqual) {
            linked[i] = 1;
            *leaf_link = i;
            status = 1; /* got it */
            goto finish;
        }
      }

finish:
    /* nothing allocated, nothing freed */
    return status;
}

/**
 * This function handles unordered certificate chain also.
 * Return:
 *   1 on success
 *   0 otherwise
 */
static int find_leaf_cert(
  CERTCertDBHandle *certdb,
  SECItem *derCerts,
  int numCerts,
  SECItem *theDerCert
)
{
    int status = 1;
    int found;
    int i;
    int cur_link, leaf_link;
    int *linked = NULL;

    linked = PR_Malloc( sizeof(int) * numCerts );

    /* initialize the bitmap */
    for (i = 0; i < numCerts; i++) {
      linked[i] = 0;
    }

    /* pick the first cert to start with */
    leaf_link = 0;
    cur_link = leaf_link;
    linked[leaf_link] = 1;

    while (((found = find_child_cert(certdb,
       derCerts, numCerts, linked, cur_link, &leaf_link)) == 1))
    {
        cur_link = leaf_link;
    }
    if (found == -1) {
        /* the certificate chain is problemtic! */
        status = 0;
        goto finish;
    }

    *theDerCert = derCerts[leaf_link];

finish:

    if (linked != NULL) {
        PR_Free(linked);
    }

    return status;
} /* find_leaf_cert */

/***********************************************************************
 * CryptoManager.importCertPackage
 */
JNIEXPORT jobject JNICALL
Java_org_mozilla_jss_CryptoManager_importCertPackageNative
    (JNIEnv *env, jobject this, jbyteArray packageArray, jstring nickString,
     jboolean noUser, jboolean leafIsCA)
{
    SECItem *derCerts=NULL;
    int certi= -1;
    SECItem theDerCert;
    int numCerts;
    jbyte *packageBytes=NULL;
    jsize packageLen;
    SECStatus status;
    int i, userCertFound = 0;
    DERCertCollection collection;
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    CK_OBJECT_HANDLE keyID;
    PK11SlotInfo *slot=NULL;
    char *nickChars = NULL;
    jobject leafObject=NULL;
    CERTIssuerAndSN issuerAndSN;
    PRStatus decodeStatus;
    SECItem leafSubject;
    CERTCertificate *leafCert = NULL;

    /***************************************************
     * Validate arguments
     ***************************************************/
    PR_ASSERT( env!=NULL && this!=NULL );
    if(packageArray == NULL) {
        PR_ASSERT(PR_FALSE);
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "Certificate package is NULL");
        goto finish;
    }
    PR_ASSERT(certdb != NULL);

    /***************************************************
     * Convert package from byte array to jbyte*
     ***************************************************/
    packageBytes = (*env)->GetByteArrayElements(env, packageArray, NULL);
    if(packageBytes == NULL) {
        PR_ASSERT( (*env)->ExceptionOccurred(env) );
        goto finish;
    }
    packageLen = (*env)->GetArrayLength(env, packageArray);

    /***************************************************
     * Decode package with NSS function
     ***************************************************/
    status = CERT_DecodeCertPackage((char*) packageBytes,
                                    (int) packageLen,
                                    collect_der_certs,
                                    (void*) &collection);
    if( status != SECSuccess  || collection.numCerts < 1 ) {
        if( (*env)->ExceptionOccurred(env) == NULL) {
            JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                "Security library failed to decode certificate package");
        }
        goto finish;
    }
    derCerts = collection.derCerts;
    numCerts = collection.numCerts;

    /***************************************************
     * convert nickname to char*
     ***************************************************/
    if(nickString == NULL) {
        nickChars = NULL;
    } else {
        nickChars = (char*) (*env)->GetStringUTFChars(env, nickString, NULL);
    }

    /***************************************************
     * user cert can be anywhere in the cert chain. loop and find it.
     * The point is to find the user cert with keys on the db, then
     * treat the other certs in the chain as CA certs to import.
     * The real order of the cert chain shouldn't matter, and shouldn't
     * be assumed, and the real location of this user cert in the chain,
     * if present, shouldn't be assumed either.
     ***************************************************/
    if (numCerts > 1) {
        for (certi=0; certi<numCerts; certi++) {
            slot = PK11_KeyForDERCertExists(&derCerts[certi], &keyID, NULL);
            if (slot != NULL) { /* found the user cert */
                theDerCert = derCerts[certi];
                /*certi now indicates the location of our user cert in chain*/
                break;
            }

        } /* end for */

        /* (NO_USER_CERT_HANDLING)
         Handles the case when the user certificate is not in
         the certificate chain.
        */
        if ((slot == NULL)) { /* same as "noUser = 1" */
            /* #397713 */
            if (!find_leaf_cert(certdb, derCerts,
                    numCerts, &theDerCert))
            {
                JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                    "Failed to locate leaf certificate in chain");
                goto finish;
            }
        }

    } else {/* numCerts <= 1 */
        theDerCert = derCerts[0];
        certi = 0;
    }

    /***************************************************
     * Create a new cert structure for the leaf cert
     ***************************************************/

    /* get issuer and serial number of leaf certificate */
    decodeStatus = getCertFields(&theDerCert, &issuerAndSN.derIssuer,
        &issuerAndSN.serialNumber, &leafSubject);
    if( decodeStatus != PR_SUCCESS ) {
        JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
            "Failed to extract issuer and serial number from certificate");
        goto finish;
    }

    /***************************************************
     * Is this a user cert?
     ***************************************************/
    if(noUser) {
        slot = NULL;
    } else {
        slot = PK11_KeyForDERCertExists(&theDerCert, &keyID, NULL);
    }
    if( slot == NULL ) {
        if( !noUser && !CERT_IsCADERCert(&theDerCert, NULL)) {
            /*****************************************
             * This isn't a CA cert, but it also doesn't have a matching
             * key, so it's supposed to be a user cert but it has failed
             * miserably at its calling.
             *****************************************/
            JSS_throwMsgPrErr(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION,
                    "Expected user cert but no matching key?");             
            goto finish;
        }
    } else {
        /***************************************************
         * We have a user cert, import it
         ***************************************************/

        /***************************************************
         * Check for nickname conflicts
         ***************************************************/
        if( SEC_CertNicknameConflict(nickChars, &leafSubject, certdb))
        {
            JSS_throw(env, NICKNAME_CONFLICT_EXCEPTION);
            goto finish;
        }

        /***************************************************
         * Import this certificate onto its token.
         ***************************************************/
        PK11_FreeSlot(slot);
        slot = PK11_ImportDERCertForKey(&theDerCert, nickChars, NULL);
        if( slot == NULL ) {
            /* We already checked for this, shouldn't fail here */
            if(PR_GetError() == SEC_ERROR_ADDING_CERT) {
                PR_ASSERT(PR_FALSE);
                JSS_throwMsgPrErr(env, NO_SUCH_ITEM_ON_TOKEN_EXCEPTION, 
                     "PK11_ImportDERCertForKey SEC_ERROR_ADDING_CERT");
            } else {
                JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
                        "PK11_ImportDERCertForKey Unable to "
                        "import certificate to its token");
            }
            goto finish;
        }

        if( ! leafIsCA ) {
            ++userCertFound;
        }
    }

    /***************************************************
     * Now add the rest of the certs (which should all be CAs)
     ***************************************************/
    if( numCerts-userCertFound>= 1 ) {

      if (certi == 0) {
        status = CERT_ImportCAChainTrusted(derCerts+userCertFound,
                                    numCerts-userCertFound,
                                    certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChainTrusted returned an error");
            goto finish;
        }
      } else if (certi == numCerts) {
        status = CERT_ImportCAChainTrusted(derCerts,
                                    numCerts-userCertFound,
                                    certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChainTrusted returned an error");
            goto finish;
        }
      } else {
        status = CERT_ImportCAChainTrusted(derCerts,
                   certi,
                   certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChainTrusted returned an error");
            goto finish;
        }

        status = CERT_ImportCAChainTrusted(derCerts+certi+1,
                   numCerts-certi-1,
                   certUsageUserCertImport);
        if(status != SECSuccess) {
            JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                "CERT_ImportCAChainTrusted returned an error");
            goto finish;
        }

      }

    }

    /***************************************************
     * Now lookup the leaf cert and make it into a Java object.
     ***************************************************/
    if(slot) {
        PK11_FreeSlot(slot);
    }
    leafCert = PK11_FindCertByIssuerAndSN(&slot, &issuerAndSN, NULL);
    if( leafCert == NULL ) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION,
            "Failed to find certificate that was just imported");
        goto finish;
    }
    leafObject = JSS_PK11_wrapCertAndSlot(env, &leafCert, &slot);

finish:
    if(slot!=NULL) {
        PK11_FreeSlot(slot);
    }
    if(derCerts != NULL) {
        for(i=0; i < numCerts; i++) {
            SECITEM_FreeItem(&derCerts[i], PR_FALSE /*freeit*/);
        }
        PR_Free(derCerts);
    }
    if(packageBytes != NULL) {
        (*env)->ReleaseByteArrayElements(env, packageArray, packageBytes,
                                            JNI_ABORT); /* don't copy back */
    }
    if(leafCert != NULL) {
        CERT_DestroyCertificate(leafCert);
    }

    return leafObject;
}

/**********************************************************************
 * PKCS #7 Encoding data structures
 */
typedef struct BufferNodeStr {
    char *data;
    unsigned long len;
    struct BufferNodeStr *next;
} BufferNode;

typedef struct {
    BufferNode *head;
    BufferNode *tail;
    unsigned long totalLen;
} EncoderCallbackInfo;

/**********************************************************************
 * c r e a t e E n c o d e r C a l l b a c k I n f o
 *
 * Constructor for EncoderCallbackInfo structure.
 * Returns NULL if it runs out of memory, otherwise a new EncoderCallbackInfo.
 */
static EncoderCallbackInfo*
createEncoderCallbackInfo()
{
    EncoderCallbackInfo *info;

    info = PR_Malloc( sizeof(EncoderCallbackInfo) );
    if( info == NULL ) {
        return NULL;
    }
    info->head = info->tail = NULL;
    info->totalLen = 0;

    return info;
}

/***********************************************************************
 * d e s t r o y E n c o d e r C a l l b a c k I n f o
 *
 * Destructor for EncoderCallbackInfo structure.
 */
static void
destroyEncoderCallbackInfo(EncoderCallbackInfo *info)
{
    BufferNode *node;

    PR_ASSERT(info != NULL);

    while(info->head != NULL) {
        node = info->head;
        info->head = info->head->next;

        if(node->data) {
            PR_Free(node->data);
        }
        PR_Free(node);
    }
    PR_Free(info);
}

/***********************************************************************
 * e n c o d e r O u t p u t C a l l b a c k
 *
 * Called by the PKCS #7 encoder whenever output is available.
 * Appends the output to a linked list.
 */
static void
encoderOutputCallback( void *arg, const char *buf, unsigned long len)
{
    BufferNode *node;
    EncoderCallbackInfo *info;

    /***************************************************
     * validate arguments
     ***************************************************/
    PR_ASSERT(arg!=NULL);
    info = (EncoderCallbackInfo*) arg;
    if(len == 0) {
        return;
    }
    PR_ASSERT(buf != NULL);

    /***************************************************
     * Create a new node to store this information
     ***************************************************/
    node = PR_NEW( BufferNode );
    if( node == NULL ) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
    node->len = len;
    node->data = PR_Malloc( len );
    if( node->data == NULL ) {
        PR_ASSERT(PR_FALSE);
        goto finish;
    }
    memcpy( node->data, buf, len );
    node->next = NULL;

    /***************************************************
     * Stick the new node onto the end of the list
     ***************************************************/
    if( info->head == NULL ) {
        PR_ASSERT(info->tail == NULL);

        info->head = info->tail = node;
    } else {
        PR_ASSERT(info->tail != NULL);
        info->tail->next = node;
        info->tail = node;
    }
    node = NULL;

    info->totalLen += len;

finish:
    if(node != NULL) {
        if( node->data != NULL) {
            PR_Free(node->data);
        }
        PR_Free(node);
    }
    return;
}

/***********************************************************************
 * CryptoManager.exportCertsToPKCS7
 */
JNIEXPORT jbyteArray JNICALL
Java_org_mozilla_jss_CryptoManager_exportCertsToPKCS7
    (JNIEnv *env, jobject this, jobjectArray certArray)
{
    int i, certcount;
    SEC_PKCS7ContentInfo *cinfo=NULL;
    CERTCertificate *cert;
    jclass certClass;
    jbyteArray pkcs7ByteArray=NULL;
    jbyte *pkcs7Bytes=NULL;
    EncoderCallbackInfo *info=NULL;
    SECStatus status;

    /**************************************************
     * Validate arguments
     **************************************************/
    PR_ASSERT(env!=NULL && this!=NULL);
    if(certArray == NULL) {
        JSS_throw(env, NULL_POINTER_EXCEPTION);
        goto finish;
    }

    certcount = (*env)->GetArrayLength(env, certArray);
    if(certcount < 1) {
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "At least one certificate must be passed to exportCertsToPKCS7");
        goto finish;
    }

    /*
     * JNI ID lookup
     */
    certClass = (*env)->FindClass(env, CERT_CLASS_NAME);
    if(certClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /***************************************************
     * Add each cert to the PKCS #7 context.  Create the context
     * for the first cert.
     ***************************************************/
    for(i=0; i < certcount; i++) {
        jobject certObject;

        certObject = (*env)->GetObjectArrayElement(env, certArray, i);
        if( (*env)->ExceptionOccurred(env) != NULL) {
            goto finish;
        }
        PR_ASSERT( certObject != NULL );

        /*
         * Make sure this is a PK11Cert
         */
        if( ! (*env)->IsInstanceOf(env, certObject, certClass) ) {
            JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                "Certificate was not a PK11 Certificate");
            goto finish;
        }

        /*
         * Convert it to a CERTCertificate
         */
        if( JSS_PK11_getCertPtr(env, certObject, &cert) != PR_SUCCESS) {
            JSS_trace(env, JSS_TRACE_ERROR,
                "Unable to convert Java certificate to CERTCertificate");
            goto finish;
        }
        PR_ASSERT(cert != NULL);

        if( i == 0 ) {
            /*
             * First certificate: create a new PKCS #7 cert-only context
             */
            PR_ASSERT(cinfo == NULL);
            cinfo = SEC_PKCS7CreateCertsOnly(cert,
                                         PR_FALSE, /* don't include chain */
                                         NULL /* cert db */ );
            if(cinfo == NULL) {
                JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                    "Failed to create PKCS #7 encoding context");
                goto finish;
            }
        } else {
            /*
             * All remaining certificates: add cert to context
             */
            PR_ASSERT(cinfo != NULL);

            if( SEC_PKCS7AddCertificate(cinfo, cert) != SECSuccess ) {
                JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
                    "Failed to add certificate to PKCS #7 encoding context");
                goto finish;
            }
        }
    }
    PR_ASSERT( i == certcount );
    PR_ASSERT( cinfo != NULL );

    /**************************************************
     * Encode the PKCS #7 context into its DER encoding
     **************************************************/
    info = createEncoderCallbackInfo();
    if(info == NULL) {
        JSS_throw(env, OUT_OF_MEMORY_ERROR);
        goto finish;
    }

    status = SEC_PKCS7Encode(cinfo,
                             encoderOutputCallback,
                             (void*)info,
                             NULL /* bulk key */,
                             NULL /* password function */,
                             NULL /* password function arg */ );
    if( status != SECSuccess ) {
        JSS_throwMsgPrErr(env, CERTIFICATE_ENCODING_EXCEPTION,
            "Failed to encode PKCS #7 context");
    }
    /* Make sure we got at least some data from the encoder */
    PR_ASSERT(info->totalLen > 0);
    PR_ASSERT(info->head != NULL);

    /**************************************************
     * Create a new byte array to hold the encoded PKCS #7
     **************************************************/
    pkcs7ByteArray = (*env)->NewByteArray(env, info->totalLen);
    if(pkcs7ByteArray == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    pkcs7Bytes = (*env)->GetByteArrayElements(env, pkcs7ByteArray, NULL);
    if(pkcs7Bytes == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    /**************************************************
     * Copy the PKCS #7 encoding into the byte array
     **************************************************/
    {
        BufferNode *node;
        unsigned long processed=0;

        for(node=info->head; node!=NULL; node = node->next) {
            PR_ASSERT(processed < info->totalLen);
            PR_ASSERT(node->data != NULL);
            PR_ASSERT(node->len > 0);
            memcpy(pkcs7Bytes+processed, node->data, node->len);
            processed += node->len;
        }
        PR_ASSERT( processed == info->totalLen );
    }

finish:
    /**************************************************
     * Free allocated resources
     **************************************************/
    if( cinfo != NULL) {
        SEC_PKCS7DestroyContentInfo(cinfo);
    }
    if(pkcs7Bytes != NULL) {
        PR_ASSERT(pkcs7ByteArray != NULL);
        (*env)->ReleaseByteArrayElements(env, pkcs7ByteArray, pkcs7Bytes, 0);
    }
    if( info != NULL ) {
        destroyEncoderCallbackInfo(info);
    }

    /**************************************************
     * Return the PKCS #7 information in a byte array, or NULL if an
     * exception occurred
     **************************************************/
    PR_ASSERT( (*env)->ExceptionOccurred(env)!=NULL || pkcs7ByteArray!=NULL );
    return pkcs7ByteArray;
}

/***************************************************************************
 * getCerts
 *
 * Gathers all certificates of the given type into a Java array.
 */
static jobjectArray
getCerts(JNIEnv *env, PK11CertListType type)
{
    jobjectArray certArray = NULL;
    jclass certClass;
    jobject certObject;
    CERTCertList *certList = NULL;
    CERTCertListNode *node;
    int numCerts, i;

    certList = PK11_ListCerts(type, NULL);
    if( certList == NULL ) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION, "Unable to list certificates");
        goto finish;
    }

    /* first count the damn certs */
    numCerts = 0;
    for( node = CERT_LIST_HEAD(certList); ! CERT_LIST_END(node, certList);
            node = CERT_LIST_NEXT(node) ) {
        ++numCerts;
    }

    /**************************************************
     * Create array of Java certificates
     **************************************************/
    certClass = (*env)->FindClass(env, X509_CERT_CLASS);
    if(certClass == NULL) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }

    certArray = (*env)->NewObjectArray( env,
                                        numCerts,
                                        certClass,
                                        NULL );
    if( certArray == NULL ) {
        ASSERT_OUTOFMEM(env);
        goto finish;
    }
    PR_ASSERT( (*env)->ExceptionOccurred(env) == NULL );


    /**************************************************
     * Put all the certs in the array
     **************************************************/
    i = 0;
    for( node = CERT_LIST_HEAD(certList); ! CERT_LIST_END(node, certList);
            node = CERT_LIST_NEXT(node) ) {

        PR_ASSERT( i < numCerts );

        certObject = JSS_PK11_wrapCert(env, &(node->cert));
        if( certObject == NULL ) {
            goto finish;
        }
        (*env)->SetObjectArrayElement(env, certArray, i, certObject);
        if( (*env)->ExceptionOccurred(env) ) {
            goto finish;
        }
        ++i;

    }
    PR_ASSERT( i == numCerts );

finish:
    if( certList != NULL ) {
        CERT_DestroyCertList(certList);
    }

    return certArray;
}


/***********************************************************************
 * CryptoManager.getCACerts
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_getCACerts
    (JNIEnv *env, jobject this)
{
    return getCerts(env, PK11CertListCA);
}

/***********************************************************************
 * CryptoManager.getPermCerts
 */
JNIEXPORT jobjectArray JNICALL
Java_org_mozilla_jss_CryptoManager_getPermCerts
    (JNIEnv *env, jobject this)
{
    return getCerts(env, PK11CertListUnique);
}


 /* Imports a CRL, and stores it into the cert7.db
  *
  * @param the DER-encoded CRL.
  */


JNIEXPORT void JNICALL
Java_org_mozilla_jss_CryptoManager_importCRLNative
    (JNIEnv *env, jobject this,
        jbyteArray der_crl, jstring url_jstr, jint rl_type)

{
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();
    CERTSignedCrl *crl = NULL;
    SECItem *packageItem = NULL;
    int status = SECFailure;
    char *url;
    char *errmsg = NULL;

    /***************************************************
     * Validate arguments
     ***************************************************/
    PR_ASSERT( env!=NULL && this!=NULL );
    if(der_crl == NULL) {
        PR_ASSERT(PR_FALSE);
        /* XXX need new exception here */
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
            "CRL package is NULL");
        goto finish;
    }
    PR_ASSERT(certdb != NULL);

    /* convert CRL byte[] into secitem */

    packageItem = JSS_ByteArrayToSECItem(env, der_crl);
    if ( packageItem == NULL ) {
        goto finish;
    }
    /* XXX need to deal with if error */

    if (url_jstr != NULL) {
        url = (char*) (*env)->GetStringUTFChars(env, url_jstr, NULL);
        PR_ASSERT(url!=NULL);
    }
    else {
        url = NULL;
    }

    crl = CERT_ImportCRL( certdb, packageItem, url, rl_type, NULL);

    if( crl == NULL ) {
        status = PR_GetError();
        errmsg = NULL;
        switch (status) {
            case SEC_ERROR_OLD_CRL:
            case SEC_ERROR_OLD_KRL:
                /* not an error - leave as NULL */
                errmsg = NULL;
                goto finish;
            case SEC_ERROR_CRL_EXPIRED:
                errmsg = "CRL Expired";
                break;
            case SEC_ERROR_KRL_EXPIRED:
                errmsg = "KRL Expired";
                break;
            case SEC_ERROR_CRL_NOT_YET_VALID:
                errmsg = "CRL Not yet valid";
                break;
            case SEC_ERROR_KRL_NOT_YET_VALID:
                errmsg = "KRL Not yet valid";
                break;
            case SEC_ERROR_CRL_INVALID:
                errmsg = "Invalid encoding of CRL";
                break;
            case SEC_ERROR_KRL_INVALID:
                errmsg = "Invalid encoding of KRL";
                break;
            case SEC_ERROR_BAD_DATABASE:
                errmsg = "Database error";
                break;
            default:
                /* printf("NSS ERROR = %d\n",status);  */
                errmsg = "Failed to import Revocation List";
            }
        if (errmsg) {
            JSS_throwMsgPrErr(env, CRL_IMPORT_EXCEPTION, errmsg);
        }
    }

finish:

    if (packageItem) {
        SECITEM_FreeItem(packageItem, PR_TRUE /*freeit*/);
    }

    if(url != NULL) {
        (*env)->ReleaseStringUTFChars(env, url_jstr, url);
    }

    if (crl) {
        SEC_DestroyCrl(crl);
    }
}

/***********************************************************************
 * CryptoManager.verifyCertNowNative
 *
 * Returns JNI_TRUE if success, JNI_FALSE otherwise
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_CryptoManager_verifyCertNowNative(JNIEnv *env,
        jobject self, jstring nickString, jboolean checkSig, jint cUsage)
{
    SECStatus         rv    = SECFailure;
    SECCertUsage      certUsage;
    CERTCertificate   *cert=NULL;
    char *nickname=NULL;

    nickname = (char *) (*env)->GetStringUTFChars(env, nickString, NULL);
    if( nickname == NULL ) {
         goto finish;
    }
    certUsage = cUsage;
    cert = CERT_FindCertByNickname(CERT_GetDefaultCertDB(), nickname);

    if (cert == NULL) {
        JSS_throw(env, OBJECT_NOT_FOUND_EXCEPTION);
        goto finish;
    } else {
        rv = CERT_VerifyCertNow(CERT_GetDefaultCertDB(), cert,
            checkSig, certUsage, NULL );
    }

finish:
    if(nickname != NULL) {
      (*env)->ReleaseStringUTFChars(env, nickString, nickname);
    }
    if(cert != NULL) {
       CERT_DestroyCertificate(cert);
    }
    if( rv == SECSuccess) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

/***********************************************************************
 * CryptoManager.verifyCertNative
 *
 * Returns JNI_TRUE if success, JNI_FALSE otherwise
 */
JNIEXPORT jboolean JNICALL
Java_org_mozilla_jss_CryptoManager_verifyCertTempNative(JNIEnv *env,
     jobject self, jbyteArray packageArray,jboolean checkSig, jint cUsage)
{
    SECStatus         rv    = SECFailure;
    SECCertUsage      certUsage;
    SECItem *derCerts[2];
    SECStatus status;
    CERTCertificate **certArray = NULL;
    CERTCertDBHandle *certdb = CERT_GetDefaultCertDB();

    /***************************************************
     * Validate arguments
     ***************************************************/
    if (packageArray == NULL) {
        JSS_throwMsg(env, CERTIFICATE_ENCODING_EXCEPTION,
                     "Certificate package is NULL");
        goto finish;
    }
    PR_ASSERT(certdb != NULL);

    derCerts[0] = NULL;
    derCerts[0] = JSS_ByteArrayToSECItem(env, packageArray);
    derCerts[1] = NULL;

    rv = CERT_ImportCerts(certdb, cUsage,
                          1, derCerts, &certArray, PR_FALSE /*temp Certs*/,
                          PR_FALSE /*caOnly*/, NULL);

    if ( rv != SECSuccess || certArray == NULL || certArray[0] == NULL) {
        JSS_throwMsgPrErr(env, TOKEN_EXCEPTION, "Unable to insert certificate"
                     " into temporary database");
        goto finish;
    }

    certUsage = cUsage;
    rv = CERT_VerifyCertNow(certdb, certArray[0],
                            checkSig, certUsage, NULL );

    finish:
    /* this checks for NULL */
    CERT_DestroyCertArray(certArray, 1);
    if (derCerts[0]) {
        SECITEM_FreeItem(derCerts[0], PR_TRUE /*freeit*/);
    }
    if ( rv == SECSuccess) {
        return JNI_TRUE;
    } else {
        return JNI_FALSE;
    }
}

