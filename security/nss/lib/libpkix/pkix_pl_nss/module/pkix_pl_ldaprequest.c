/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_pl_ldaprequest.c
 *
 */

#include "pkix_pl_ldaprequest.h"

/* --Private-LdapRequest-Functions------------------------------------- */

/* Note: lengths do not include the NULL terminator */
static const char caAttr[] = "caCertificate;binary";
static unsigned int caAttrLen = sizeof(caAttr) - 1;
static const char uAttr[] = "userCertificate;binary";
static unsigned int uAttrLen = sizeof(uAttr) - 1;
static const char ccpAttr[] = "crossCertificatePair;binary";
static unsigned int ccpAttrLen = sizeof(ccpAttr) - 1;
static const char crlAttr[] = "certificateRevocationList;binary";
static unsigned int crlAttrLen = sizeof(crlAttr) - 1;
static const char arlAttr[] = "authorityRevocationList;binary";
static unsigned int arlAttrLen = sizeof(arlAttr) - 1;

/*
 * XXX If this function were moved into pkix_pl_ldapcertstore.c then all of
 * LdapRequest and LdapResponse could be considered part of the LDAP client.
 * But the constants, above, would have to be copied as well, and they are
 * also needed in pkix_pl_LdapRequest_EncodeAttrs. So there would have to be
 * two copies.
 */

/*
 * FUNCTION: pkix_pl_LdapRequest_AttrTypeToBit
 * DESCRIPTION:
 *
 *  This function creates an attribute mask bit corresponding to the SECItem
 *  pointed to by "attrType", storing the result at "pAttrBit". The comparison
 *  is case-insensitive. If "attrType" does not match any of the known types,
 *  zero is stored at "pAttrBit".
 *
 * PARAMETERS
 *  "attrType"
 *      The address of the SECItem whose string contents are to be compared to
 *      the various known attribute types. Must be non-NULL.
 *  "pAttrBit"
 *      The address where the result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapRequest Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapRequest_AttrTypeToBit(
        SECItem *attrType,
        LdapAttrMask *pAttrBit,
        void *plContext)
{
        LdapAttrMask attrBit = 0;
        unsigned int attrLen = 0;
        const char *s = NULL;

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_AttrTypeToBit");
        PKIX_NULLCHECK_TWO(attrType, pAttrBit);

        s = (const char *)attrType->data;
        attrLen = attrType->len;

        /*
         * Taking note of the fact that all of the comparand strings are
         * different lengths, we do a slight optimization. If a string
         * length matches but the string does not match, we skip comparing
         * to the other strings. If new strings are added to the comparand
         * list, and any are of equal length, be careful to change the
         * grouping of tests accordingly.
         */
        if (attrLen == caAttrLen) {
                if (PORT_Strncasecmp(caAttr, s, attrLen) == 0) {
                        attrBit = LDAPATTR_CACERT;
                }
        } else if (attrLen == uAttrLen) {
                if (PORT_Strncasecmp(uAttr, s, attrLen) == 0) {
                        attrBit = LDAPATTR_USERCERT;
                }
        } else if (attrLen == ccpAttrLen) {
                if (PORT_Strncasecmp(ccpAttr, s, attrLen) == 0) {
                        attrBit = LDAPATTR_CROSSPAIRCERT;
                }
        } else if (attrLen == crlAttrLen) {
                if (PORT_Strncasecmp(crlAttr, s, attrLen) == 0) {
                        attrBit = LDAPATTR_CERTREVLIST;
                }
        } else if (attrLen == arlAttrLen) {
                if (PORT_Strncasecmp(arlAttr, s, attrLen) == 0) {
                        attrBit = LDAPATTR_AUTHREVLIST;
                }
        }

        *pAttrBit = attrBit;

        PKIX_RETURN(LDAPREQUEST);
}

/*
 * FUNCTION: pkix_pl_LdapRequest_AttrStringToBit
 * DESCRIPTION:
 *
 *  This function creates an attribute mask bit corresponding to the null-
 *  terminated string pointed to by "attrString", storing the result at
 *  "pAttrBit". The comparison is case-insensitive. If "attrString" does not
 *  match any of the known types, zero is stored at "pAttrBit".
 *
 * PARAMETERS
 *  "attrString"
 *      The address of the null-terminated string whose contents are to be compared to
 *      the various known attribute types. Must be non-NULL.
 *  "pAttrBit"
 *      The address where the result is stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapRequest Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapRequest_AttrStringToBit(
        char *attrString,
        LdapAttrMask *pAttrBit,
        void *plContext)
{
        LdapAttrMask attrBit = 0;
        unsigned int attrLen = 0;

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_AttrStringToBit");
        PKIX_NULLCHECK_TWO(attrString, pAttrBit);

        attrLen = PL_strlen(attrString);

        /*
         * Taking note of the fact that all of the comparand strings are
         * different lengths, we do a slight optimization. If a string
         * length matches but the string does not match, we skip comparing
         * to the other strings. If new strings are added to the comparand
         * list, and any are of equal length, be careful to change the
         * grouping of tests accordingly.
         */
        if (attrLen == caAttrLen) {
                if (PORT_Strncasecmp(caAttr, attrString, attrLen) == 0) {
                        attrBit = LDAPATTR_CACERT;
                }
        } else if (attrLen == uAttrLen) {
                if (PORT_Strncasecmp(uAttr, attrString, attrLen) == 0) {
                        attrBit = LDAPATTR_USERCERT;
                }
        } else if (attrLen == ccpAttrLen) {
                if (PORT_Strncasecmp(ccpAttr, attrString, attrLen) == 0) {
                        attrBit = LDAPATTR_CROSSPAIRCERT;
                }
        } else if (attrLen == crlAttrLen) {
                if (PORT_Strncasecmp(crlAttr, attrString, attrLen) == 0) {
                        attrBit = LDAPATTR_CERTREVLIST;
                }
        } else if (attrLen == arlAttrLen) {
                if (PORT_Strncasecmp(arlAttr, attrString, attrLen) == 0) {
                        attrBit = LDAPATTR_AUTHREVLIST;
                }
        }

        *pAttrBit = attrBit;

        PKIX_RETURN(LDAPREQUEST);
}

/*
 * FUNCTION: pkix_pl_LdapRequest_EncodeAttrs
 * DESCRIPTION:
 *
 *  This function obtains the attribute mask bits from the LdapRequest pointed
 *  to by "request", creates the corresponding array of AttributeTypes for the
 *  encoding of the SearchRequest message.
 *
 * PARAMETERS
 *  "request"
 *      The address of the LdapRequest whose attributes are to be encoded. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapRequest Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_pl_LdapRequest_EncodeAttrs(
        PKIX_PL_LdapRequest *request,
        void *plContext)
{
        SECItem **attrArray = NULL;
        PKIX_UInt32 attrIndex = 0;
        LdapAttrMask attrBits;

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_EncodeAttrs");
        PKIX_NULLCHECK_ONE(request);

        /* construct "attrs" according to bits in request->attrBits */
        attrBits = request->attrBits;
        attrArray = request->attrArray;
        if ((attrBits & LDAPATTR_CACERT) == LDAPATTR_CACERT) {
                attrArray[attrIndex] = &(request->attributes[attrIndex]);
                request->attributes[attrIndex].type = siAsciiString;
                request->attributes[attrIndex].data = (unsigned char *)caAttr;
                request->attributes[attrIndex].len = caAttrLen;
                attrIndex++;
        }
        if ((attrBits & LDAPATTR_USERCERT) == LDAPATTR_USERCERT) {
                attrArray[attrIndex] = &(request->attributes[attrIndex]);
                request->attributes[attrIndex].type = siAsciiString;
                request->attributes[attrIndex].data = (unsigned char *)uAttr;
                request->attributes[attrIndex].len = uAttrLen;
                attrIndex++;
        }
        if ((attrBits & LDAPATTR_CROSSPAIRCERT) == LDAPATTR_CROSSPAIRCERT) {
                attrArray[attrIndex] = &(request->attributes[attrIndex]);
                request->attributes[attrIndex].type = siAsciiString;
                request->attributes[attrIndex].data = (unsigned char *)ccpAttr;
                request->attributes[attrIndex].len = ccpAttrLen;
                attrIndex++;
        }
        if ((attrBits & LDAPATTR_CERTREVLIST) == LDAPATTR_CERTREVLIST) {
                attrArray[attrIndex] = &(request->attributes[attrIndex]);
                request->attributes[attrIndex].type = siAsciiString;
                request->attributes[attrIndex].data = (unsigned char *)crlAttr;
                request->attributes[attrIndex].len = crlAttrLen;
                attrIndex++;
        }
        if ((attrBits & LDAPATTR_AUTHREVLIST) == LDAPATTR_AUTHREVLIST) {
                attrArray[attrIndex] = &(request->attributes[attrIndex]);
                request->attributes[attrIndex].type = siAsciiString;
                request->attributes[attrIndex].data = (unsigned char *)arlAttr;
                request->attributes[attrIndex].len = arlAttrLen;
                attrIndex++;
        }
        attrArray[attrIndex] = (SECItem *)NULL;

        PKIX_RETURN(LDAPREQUEST);
}

/*
 * FUNCTION: pkix_pl_LdapRequest_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapRequest_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_LdapRequest *ldapRq = NULL;

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_Destroy");
        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_LDAPREQUEST_TYPE, plContext),
                    PKIX_OBJECTNOTLDAPREQUEST);

        ldapRq = (PKIX_PL_LdapRequest *)object;

        /*
         * All dynamic fields in an LDAPRequest are allocated
         * in an arena, and will be freed when the arena is destroyed.
         */

cleanup:

        PKIX_RETURN(LDAPREQUEST);
}

/*
 * FUNCTION: pkix_pl_LdapRequest_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapRequest_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_UInt32 dataLen = 0;
        PKIX_UInt32 dindex = 0;
        PKIX_UInt32 sizeOfLength = 0;
        PKIX_UInt32 idLen = 0;
        const unsigned char *msgBuf = NULL;
        PKIX_PL_LdapRequest *ldapRq = NULL;

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_LDAPREQUEST_TYPE, plContext),
                    PKIX_OBJECTNOTLDAPREQUEST);

        ldapRq = (PKIX_PL_LdapRequest *)object;

        *pHashcode = 0;

        /*
         * Two requests that differ only in msgnum are a match! Therefore,
         * start hashcoding beyond the encoded messageID field.
         */
        if (ldapRq->encoded) {
                msgBuf = (const unsigned char *)ldapRq->encoded->data;
                /* Is message length short form (one octet) or long form? */
                if ((msgBuf[1] & 0x80) != 0) {
                        sizeOfLength = msgBuf[1] & 0x7F;
                        for (dindex = 0; dindex < sizeOfLength; dindex++) {
                                dataLen = (dataLen << 8) + msgBuf[dindex + 2];
                        }
                } else {
                        dataLen = msgBuf[1];
                }

                /* How many bytes for the messageID? (Assume short form) */
                idLen = msgBuf[dindex + 3] + 2;
                dindex += idLen;
                dataLen -= idLen;
                msgBuf = &msgBuf[dindex + 2];

                PKIX_CHECK(pkix_hash(msgBuf, dataLen, pHashcode, plContext),
                        PKIX_HASHFAILED);
        }

cleanup:

        PKIX_RETURN(LDAPREQUEST);

}

/*
 * FUNCTION: pkix_pl_LdapRequest_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_pl_LdapRequest_Equals(
        PKIX_PL_Object *firstObj,
        PKIX_PL_Object *secondObj,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PL_LdapRequest *firstReq = NULL;
        PKIX_PL_LdapRequest *secondReq = NULL;
        PKIX_UInt32 secondType = 0;
        PKIX_UInt32 firstLen = 0;
        const unsigned char *firstData = NULL;
        const unsigned char *secondData = NULL;
        PKIX_UInt32 sizeOfLength = 0;
        PKIX_UInt32 dindex = 0;
        PKIX_UInt32 i = 0;

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_Equals");
        PKIX_NULLCHECK_THREE(firstObj, secondObj, pResult);

        /* test that firstObj is a LdapRequest */
        PKIX_CHECK(pkix_CheckType(firstObj, PKIX_LDAPREQUEST_TYPE, plContext),
                    PKIX_FIRSTOBJARGUMENTNOTLDAPREQUEST);

        /*
         * Since we know firstObj is a LdapRequest, if both references are
         * identical, they must be equal
         */
        if (firstObj == secondObj){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObj isn't a LdapRequest, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObj, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_LDAPREQUEST_TYPE) {
                goto cleanup;
        }

        firstReq = (PKIX_PL_LdapRequest *)firstObj;
        secondReq = (PKIX_PL_LdapRequest *)secondObj;

        /* If either lacks an encoded string, they cannot be compared */
        if (!(firstReq->encoded) || !(secondReq->encoded)) {
                goto cleanup;
        }

        if (firstReq->encoded->len != secondReq->encoded->len) {
                goto cleanup;
        }

        firstData = (const unsigned char *)firstReq->encoded->data;
        secondData = (const unsigned char *)secondReq->encoded->data;

        /*
         * Two requests that differ only in msgnum are equal! Therefore,
         * start the byte comparison beyond the encoded messageID field.
         */

        /* Is message length short form (one octet) or long form? */
        if ((firstData[1] & 0x80) != 0) {
                sizeOfLength = firstData[1] & 0x7F;
                for (dindex = 0; dindex < sizeOfLength; dindex++) {
                        firstLen = (firstLen << 8) + firstData[dindex + 2];
                }
        } else {
                firstLen = firstData[1];
        }

        /* How many bytes for the messageID? (Assume short form) */
        i = firstData[dindex + 3] + 2;
        dindex += i;
        firstLen -= i;
        firstData = &firstData[dindex + 2];

        /*
         * In theory, we have to calculate where the second message data
         * begins by checking its length encodings. But if these messages
         * are equal, we can re-use the calculation we already did. If they
         * are not equal, the byte comparisons will surely fail.
         */

        secondData = &secondData[dindex + 2];
        
        for (i = 0; i < firstLen; i++) {
                if (firstData[i] != secondData[i]) {
                        goto cleanup;
                }
        }

        *pResult = PKIX_TRUE;

cleanup:

        PKIX_RETURN(LDAPREQUEST);
}

/*
 * FUNCTION: pkix_pl_LdapRequest_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_LDAPREQUEST_TYPE and its related functions with
 *  systemClasses[]
 * PARAMETERS:
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_pl_LdapRequest_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_RegisterSelf");

        entry.description = "LdapRequest";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PL_LdapRequest);
        entry.destructor = pkix_pl_LdapRequest_Destroy;
        entry.equalsFunction = pkix_pl_LdapRequest_Equals;
        entry.hashcodeFunction = pkix_pl_LdapRequest_Hashcode;
        entry.toStringFunction = NULL;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_duplicateImmutable;

        systemClasses[PKIX_LDAPREQUEST_TYPE] = entry;

        PKIX_RETURN(LDAPREQUEST);
}

/* --Public-Functions------------------------------------------------------- */

/*
 * FUNCTION: pkix_pl_LdapRequest_Create
 * DESCRIPTION:
 *
 *  This function creates an LdapRequest using the PLArenaPool pointed to by
 *  "arena", a message number whose value is "msgnum", a base object pointed to
 *  by "issuerDN", a scope whose value is "scope", a derefAliases flag whose
 *  value is "derefAliases", a sizeLimit whose value is "sizeLimit", a timeLimit
 *  whose value is "timeLimit", an attrsOnly flag whose value is "attrsOnly", a
 *  filter whose value is "filter", and attribute bits whose value is
 *  "attrBits"; storing the result at "pRequestMsg".
 *
 *  See pkix_pl_ldaptemplates.c (and below) for the ASN.1 representation of
 *  message components, and see pkix_pl_ldapt.h for data types.
 *
 * PARAMETERS
 *  "arena"
 *      The address of the PLArenaPool to be used in the encoding. Must be
 *      non-NULL.
 *  "msgnum"
 *      The UInt32 message number to be used for the messageID component of the
 *      LDAP message exchange.
 *  "issuerDN"
 *      The address of the string to be used for the baseObject component of the
 *      LDAP SearchRequest message. Must be non-NULL.
 *  "scope"
 *      The (enumerated) ScopeType to be used for the scope component of the
 *      LDAP SearchRequest message
 *  "derefAliases"
 *      The (enumerated) DerefType to be used for the derefAliases component of
 *      the LDAP SearchRequest message
 *  "sizeLimit"
 *      The UInt32 value to be used for the sizeLimit component of the LDAP
 *      SearchRequest message
 *  "timeLimit"
 *      The UInt32 value to be used for the timeLimit component of the LDAP
 *      SearchRequest message
 *  "attrsOnly"
 *      The Boolean value to be used for the attrsOnly component of the LDAP
 *      SearchRequest message
 *  "filter"
 *      The filter to be used for the filter component of the LDAP
 *      SearchRequest message
 *  "attrBits"
 *      The LdapAttrMask bits indicating the attributes to be included in the
 *      attributes sequence of the LDAP SearchRequest message
 *  "pRequestMsg"
 *      The address at which the address of the LdapRequest is stored. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapRequest Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
/*
 * SearchRequest ::=
 *      [APPLICATION 3] SEQUENCE {
 *              baseObject      LDAPDN,
 *              scope           ENUMERATED {
 *                                      baseObject              (0),
 *                                      singleLevel             (1),
 *                                      wholeSubtree            (2)
 *                              },
 *              derefAliases    ENUMERATED {
 *                                      neverDerefAliases       (0),
 *                                      derefInSearching        (1),
 *                                      derefFindingBaseObj     (2),
 *                                      alwaysDerefAliases      (3)
 *                              },
 *              sizeLimit       INTEGER (0 .. MAXINT),
 *                              -- value of 0 implies no sizeLimit
 *              timeLimit       INTEGER (0 .. MAXINT),
 *                              -- value of 0 implies no timeLimit
 *              attrsOnly       BOOLEAN,
 *                              -- TRUE, if only attributes (without values)
 *                              -- to be returned
 *              filter          Filter,
 *              attributes      SEQUENCE OF AttributeType
 *      }
 *
 * Filter ::=
 *      CHOICE {
 *              and             [0] SET OF Filter,
 *              or              [1] SET OF Filter,
 *              not             [2] Filter,
 *              equalityMatch   [3] AttributeValueAssertion,
 *              substrings      [4] SubstringFilter,
 *              greaterOrEqual  [5] AttributeValueAssertion,
 *              lessOrEqual     [6] AttributeValueAssertion,
 *              present         [7] AttributeType,
 *              approxMatch     [8] AttributeValueAssertion
 *      }
 *
 * SubstringFilter ::=
 *      SEQUENCE {
 *              type            AttributeType,
 *              SEQUENCE OF CHOICE {
 *                      initial [0] LDAPString,
 *                      any     [1] LDAPString,
 *                      final   [2] LDAPString,
 *              }
 *      }
 *
 * AttributeValueAssertion ::=
 *      SEQUENCE {
 *              attributeType   AttributeType,
 *              attributeValue  AttributeValue,
 *      }
 *
 * AttributeValue ::= OCTET STRING
 *
 * AttributeType ::= LDAPString
 *               -- text name of the attribute, or dotted
 *               -- OID representation
 *
 * LDAPDN ::= LDAPString
 *
 * LDAPString ::= OCTET STRING
 *
 */
PKIX_Error *
pkix_pl_LdapRequest_Create(
        PLArenaPool *arena,
        PKIX_UInt32 msgnum,
        char *issuerDN,
        ScopeType scope,
        DerefType derefAliases,
        PKIX_UInt32 sizeLimit,
        PKIX_UInt32 timeLimit,
        char attrsOnly,
        LDAPFilter *filter,
        LdapAttrMask attrBits,
        PKIX_PL_LdapRequest **pRequestMsg,
        void *plContext)
{
        LDAPMessage msg;
        LDAPSearch *search;
        PKIX_PL_LdapRequest *ldapRequest = NULL;
        char scopeTypeAsChar;
        char derefAliasesTypeAsChar;
        SECItem *attrArray[MAX_LDAPATTRS + 1];

        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_Create");
        PKIX_NULLCHECK_THREE(arena, issuerDN, pRequestMsg);

        /* create a PKIX_PL_LdapRequest object */
        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_LDAPREQUEST_TYPE,
                    sizeof (PKIX_PL_LdapRequest),
                    (PKIX_PL_Object **)&ldapRequest,
                    plContext),
                    PKIX_COULDNOTCREATEOBJECT);

        ldapRequest->arena = arena;
        ldapRequest->msgnum = msgnum;
        ldapRequest->issuerDN = issuerDN;
        ldapRequest->scope = scope;
        ldapRequest->derefAliases = derefAliases;
        ldapRequest->sizeLimit = sizeLimit;
        ldapRequest->timeLimit = timeLimit;
        ldapRequest->attrsOnly = attrsOnly;
        ldapRequest->filter = filter;
        ldapRequest->attrBits = attrBits;

        ldapRequest->attrArray = attrArray;

        PKIX_CHECK(pkix_pl_LdapRequest_EncodeAttrs
                (ldapRequest, plContext),
                PKIX_LDAPREQUESTENCODEATTRSFAILED);

        PKIX_PL_NSSCALL
                (LDAPREQUEST, PORT_Memset, (&msg, 0, sizeof (LDAPMessage)));

        msg.messageID.type = siUnsignedInteger;
        msg.messageID.data = (void*)&msgnum;
        msg.messageID.len = sizeof (msgnum);

        msg.protocolOp.selector = LDAP_SEARCH_TYPE;

        search = &(msg.protocolOp.op.searchMsg);

        search->baseObject.type = siAsciiString;
        search->baseObject.data = (void *)issuerDN;
        search->baseObject.len = PL_strlen(issuerDN);
        scopeTypeAsChar = (char)scope;
        search->scope.type = siUnsignedInteger;
        search->scope.data = (void *)&scopeTypeAsChar;
        search->scope.len = sizeof (scopeTypeAsChar);
        derefAliasesTypeAsChar = (char)derefAliases;
        search->derefAliases.type = siUnsignedInteger;
        search->derefAliases.data =
                (void *)&derefAliasesTypeAsChar;
        search->derefAliases.len =
                sizeof (derefAliasesTypeAsChar);
        search->sizeLimit.type = siUnsignedInteger;
        search->sizeLimit.data = (void *)&sizeLimit;
        search->sizeLimit.len = sizeof (PKIX_UInt32);
        search->timeLimit.type = siUnsignedInteger;
        search->timeLimit.data = (void *)&timeLimit;
        search->timeLimit.len = sizeof (PKIX_UInt32);
        search->attrsOnly.type = siBuffer;
        search->attrsOnly.data = (void *)&attrsOnly;
        search->attrsOnly.len = sizeof (attrsOnly);

        PKIX_PL_NSSCALL
                (LDAPREQUEST,
                PORT_Memcpy,
                (&search->filter, filter, sizeof (LDAPFilter)));

        search->attributes = attrArray;

        PKIX_PL_NSSCALLRV
                (LDAPREQUEST, ldapRequest->encoded, SEC_ASN1EncodeItem,
                (arena, NULL, (void *)&msg, PKIX_PL_LDAPMessageTemplate));

        if (!(ldapRequest->encoded)) {
                PKIX_ERROR(PKIX_FAILEDINENCODINGSEARCHREQUEST);
        }

        *pRequestMsg = ldapRequest;

cleanup:

        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(ldapRequest);
        }

        PKIX_RETURN(LDAPREQUEST);
}

/*
 * FUNCTION: pkix_pl_LdapRequest_GetEncoded
 * DESCRIPTION:
 *
 *  This function obtains the encoded message from the LdapRequest pointed to
 *  by "request", storing the result at "pRequestBuf".
 *
 * PARAMETERS
 *  "request"
 *      The address of the LdapRequest whose encoded message is to be
 *      retrieved. Must be non-NULL.
 *  "pRequestBuf"
 *      The address at which is stored the address of the encoded message. Must
 *      be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns an LdapRequest Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_pl_LdapRequest_GetEncoded(
        PKIX_PL_LdapRequest *request,
        SECItem **pRequestBuf,
        void *plContext)
{
        PKIX_ENTER(LDAPREQUEST, "pkix_pl_LdapRequest_GetEncoded");
        PKIX_NULLCHECK_TWO(request, pRequestBuf);

        *pRequestBuf = request->encoded;

        PKIX_RETURN(LDAPREQUEST);
}
