/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_verifynode.c
 *
 * Verify Node Object Type Definition
 *
 */

#include "pkix_verifynode.h"

/* --Private-VerifyNode-Functions---------------------------------- */

/*
 * FUNCTION: pkix_VerifyNode_Create
 * DESCRIPTION:
 *
 *  This function creates a VerifyNode using the Cert pointed to by "cert",
 *  the depth given by "depth", and the Error pointed to by "error", storing
 *  the result at "pObject".
 *
 * PARAMETERS
 *  "cert"
 *      Address of Cert for the node. Must be non-NULL
 *  "depth"
 *      UInt32 value of the depth for this node.
 *  "error"
 *      Address of Error for the node.
 *  "pObject"
 *      Address where the VerifyNode pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_VerifyNode_Create(
        PKIX_PL_Cert *cert,
        PKIX_UInt32 depth,
        PKIX_Error *error,
        PKIX_VerifyNode **pObject,
        void *plContext)
{
        PKIX_VerifyNode *node = NULL;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_Create");
        PKIX_NULLCHECK_TWO(cert, pObject);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_VERIFYNODE_TYPE,
                sizeof (PKIX_VerifyNode),
                (PKIX_PL_Object **)&node,
                plContext),
                PKIX_COULDNOTCREATEVERIFYNODEOBJECT);

        PKIX_INCREF(cert);
        node->verifyCert = cert;

        PKIX_INCREF(error);
        node->error = error;

        node->depth = depth;

        node->children = NULL;

        *pObject = node;
        node = NULL;

cleanup:

        PKIX_DECREF(node);

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_AddToChain
 * DESCRIPTION:
 *
 *  Adds the VerifyNode pointed to by "child", at the appropriate depth, to the
 *  List of children of the VerifyNode pointed to by "parentNode". The chain of
 *  VerifyNodes is traversed until a VerifyNode is found at a depth one less
 *  than that specified in "child". An Error is returned if there is no parent
 *  at a suitable depth.
 *
 *  If "parentNode" has a NULL pointer for the List of children, a new List is
 *  created containing "child". Otherwise "child" is appended to the existing
 *  List.
 *
 *  Depth, in this context, means distance from the root node, which
 *  is at depth zero.
 *
 * PARAMETERS:
 *  "parentNode"
 *      Address of VerifyNode whose List of child VerifyNodes is to be
 *      created or appended to. Must be non-NULL.
 *  "child"
 *      Address of VerifyNode to be added to parentNode's List. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a VerifyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_VerifyNode_AddToChain(
        PKIX_VerifyNode *parentNode,
        PKIX_VerifyNode *child,
        void *plContext)
{
        PKIX_VerifyNode *successor = NULL;
        PKIX_List *listOfChildren = NULL;
        PKIX_UInt32 numChildren = 0;
        PKIX_UInt32 parentDepth = 0;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_AddToChain");
        PKIX_NULLCHECK_TWO(parentNode, child);

        parentDepth = parentNode->depth;
        listOfChildren = parentNode->children;
        if (listOfChildren == NULL) {

                if (parentDepth != (child->depth - 1)) {
                        PKIX_ERROR(PKIX_NODESMISSINGFROMCHAIN);
                }

                PKIX_CHECK(PKIX_List_Create(&listOfChildren, plContext),
                        PKIX_LISTCREATEFAILED);

                PKIX_CHECK(PKIX_List_AppendItem
                        (listOfChildren, (PKIX_PL_Object *)child, plContext),
                        PKIX_COULDNOTAPPENDCHILDTOPARENTSVERIFYNODELIST);

                parentNode->children = listOfChildren;
        } else {
                /* get number of children */
                PKIX_CHECK(PKIX_List_GetLength
                        (listOfChildren, &numChildren, plContext),
                        PKIX_LISTGETLENGTHFAILED);

                if (numChildren != 1) {
                        PKIX_ERROR(PKIX_AMBIGUOUSPARENTAGEOFVERIFYNODE);
                }

                /* successor = listOfChildren[0] */
                PKIX_CHECK(PKIX_List_GetItem
                        (listOfChildren,
                        0,
                        (PKIX_PL_Object **)&successor,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(pkix_VerifyNode_AddToChain
                        (successor, child, plContext),
                        PKIX_VERIFYNODEADDTOCHAINFAILED);
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)parentNode, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:
        PKIX_DECREF(successor);

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_SetDepth
 * DESCRIPTION:
 *
 *  The function sets the depth field of each VerifyNode in the List "children"
 *  to the value given by "depth", and recursively sets the depth of any
 *  successive generations to the successive values.
 *
 * PARAMETERS:
 *  "children"
 *      The List of VerifyNodes. Must be non-NULL.
 *  "depth"
 *      The value of the depth field to be set in members of the List.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_VerifyNode_SetDepth(PKIX_List *children,
        PKIX_UInt32 depth,
        void *plContext)
{
        PKIX_UInt32 numChildren = 0;
        PKIX_UInt32 chIx = 0;
        PKIX_VerifyNode *child = NULL;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_SetDepth");
        PKIX_NULLCHECK_ONE(children);

        PKIX_CHECK(PKIX_List_GetLength(children, &numChildren, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (chIx = 0; chIx < numChildren; chIx++) {
               PKIX_CHECK(PKIX_List_GetItem
                        (children, chIx, (PKIX_PL_Object **)&child, plContext),
                        PKIX_LISTGETITEMFAILED);

                child->depth = depth;

                if (child->children != NULL) {
                        PKIX_CHECK(pkix_VerifyNode_SetDepth
                                (child->children, depth + 1, plContext),
                                PKIX_VERIFYNODESETDEPTHFAILED);
                }

                PKIX_DECREF(child);
        }

cleanup:

        PKIX_DECREF(child);

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_AddToTree
 * DESCRIPTION:
 *
 *  Adds the VerifyNode pointed to by "child" to the List of children of the
 *  VerifyNode pointed to by "parentNode". If "parentNode" has a NULL pointer
 *  for the List of children, a new List is created containing "child".
 *  Otherwise "child" is appended to the existing List. The depth field of
 *  "child" is set to one more than the corresponding value in "parent", and
 *  if the "child" itself has child nodes, their depth fields are updated
 *  accordingly.
 *
 *  Depth, in this context, means distance from the root node, which
 *  is at depth zero.
 *
 * PARAMETERS:
 *  "parentNode"
 *      Address of VerifyNode whose List of child VerifyNodes is to be
 *      created or appended to. Must be non-NULL.
 *  "child"
 *      Address of VerifyNode to be added to parentNode's List. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_VerifyNode_AddToTree(
        PKIX_VerifyNode *parentNode,
        PKIX_VerifyNode *child,
        void *plContext)
{
        PKIX_List *listOfChildren = NULL;
        PKIX_UInt32 parentDepth = 0;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_AddToTree");
        PKIX_NULLCHECK_TWO(parentNode, child);

        parentDepth = parentNode->depth;
        listOfChildren = parentNode->children;
        if (listOfChildren == NULL) {

                PKIX_CHECK(PKIX_List_Create(&listOfChildren, plContext),
                        PKIX_LISTCREATEFAILED);

                parentNode->children = listOfChildren;
        }

        child->depth = parentDepth + 1;

        PKIX_CHECK(PKIX_List_AppendItem
                (parentNode->children, (PKIX_PL_Object *)child, plContext),
                PKIX_COULDNOTAPPENDCHILDTOPARENTSVERIFYNODELIST);

        if (child->children != NULL) {
                PKIX_CHECK(pkix_VerifyNode_SetDepth
                        (child->children, child->depth + 1, plContext),
                        PKIX_VERIFYNODESETDEPTHFAILED);
        }


cleanup:

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_SingleVerifyNode_ToString
 * DESCRIPTION:
 *
 *  Creates a String representation of the attributes of the VerifyNode pointed
 *  to by "node", other than its children, and stores the result at "pString".
 *
 * PARAMETERS:
 *  "node"
 *      Address of VerifyNode to be described by the string. Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if function succeeds
 *  Returns a VerifyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
PKIX_Error *
pkix_SingleVerifyNode_ToString(
        PKIX_VerifyNode *node,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *fmtString = NULL;
        PKIX_PL_String *errorString = NULL;
        PKIX_PL_String *outString = NULL;

        PKIX_PL_X500Name *issuerName = NULL;
        PKIX_PL_X500Name *subjectName = NULL;
        PKIX_PL_String *issuerString = NULL;
        PKIX_PL_String *subjectString = NULL;

        PKIX_ENTER(VERIFYNODE, "pkix_SingleVerifyNode_ToString");
        PKIX_NULLCHECK_THREE(node, pString, node->verifyCert);

        PKIX_TOSTRING(node->error, &errorString, plContext,
                PKIX_ERRORTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetIssuer
                (node->verifyCert, &issuerName, plContext),
                PKIX_CERTGETISSUERFAILED);

        PKIX_TOSTRING(issuerName, &issuerString, plContext,
                PKIX_X500NAMETOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Cert_GetSubject
                (node->verifyCert, &subjectName, plContext),
                PKIX_CERTGETSUBJECTFAILED);

        PKIX_TOSTRING(subjectName, &subjectString, plContext,
                PKIX_X500NAMETOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                "CERT[Issuer:%s, Subject:%s], depth=%d, error=%s",
                0,
                &fmtString,
                plContext),
                PKIX_CANTCREATESTRING);

        PKIX_CHECK(PKIX_PL_Sprintf
                (&outString,
                plContext,
                fmtString,
                issuerString,
                subjectString,
                node->depth,
                errorString),
                PKIX_SPRINTFFAILED);

        *pString = outString;

cleanup:

        PKIX_DECREF(fmtString);
        PKIX_DECREF(errorString);
        PKIX_DECREF(issuerName);
        PKIX_DECREF(subjectName);
        PKIX_DECREF(issuerString);
        PKIX_DECREF(subjectString);
        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_ToString_Helper
 * DESCRIPTION:
 *
 *  Produces a String representation of a VerifyNode tree below the VerifyNode
 *  pointed to by "rootNode", with each line of output prefixed by the String
 *  pointed to by "indent", and stores the result at "pTreeString". It is
 *  called recursively, with ever-increasing indentation, for successively
 *  lower nodes on the tree.
 *
 * PARAMETERS:
 *  "rootNode"
 *      Address of VerifyNode subtree. Must be non-NULL.
 *  "indent"
 *      Address of String to be prefixed to each line of output. May be NULL
 *      if no indentation is desired
 *  "pTreeString"
 *      Address where the resulting String will be stored; must be non-NULL
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a VerifyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_VerifyNode_ToString_Helper(
        PKIX_VerifyNode *rootNode,
        PKIX_PL_String *indent,
        PKIX_PL_String **pTreeString,
        void *plContext)
{
        PKIX_PL_String *nextIndentFormat = NULL;
        PKIX_PL_String *thisNodeFormat = NULL;
        PKIX_PL_String *childrenFormat = NULL;
        PKIX_PL_String *nextIndentString = NULL;
        PKIX_PL_String *resultString = NULL;
        PKIX_PL_String *thisItemString = NULL;
        PKIX_PL_String *childString = NULL;
        PKIX_VerifyNode *childNode = NULL;
        PKIX_UInt32 numberOfChildren = 0;
        PKIX_UInt32 childIndex = 0;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_ToString_Helper");

        PKIX_NULLCHECK_TWO(rootNode, pTreeString);

        /* Create a string for this node */
        PKIX_CHECK(pkix_SingleVerifyNode_ToString
                (rootNode, &thisItemString, plContext),
                PKIX_ERRORINSINGLEVERIFYNODETOSTRING);

        if (indent) {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII,
                        "%s%s",
                        0,
                        &thisNodeFormat,
                        plContext),
                        PKIX_ERRORCREATINGFORMATSTRING);

                PKIX_CHECK(PKIX_PL_Sprintf
                        (&resultString,
                        plContext,
                        thisNodeFormat,
                        indent,
                        thisItemString),
                        PKIX_ERRORINSPRINTF);
        } else {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII,
                        "%s",
                        0,
                        &thisNodeFormat,
                        plContext),
                        PKIX_ERRORCREATINGFORMATSTRING);

                PKIX_CHECK(PKIX_PL_Sprintf
                        (&resultString,
                        plContext,
                        thisNodeFormat,
                        thisItemString),
                        PKIX_ERRORINSPRINTF);
        }

        PKIX_DECREF(thisItemString);
        thisItemString = resultString;

        /* if no children, we are done */
        if (rootNode->children) {
                PKIX_CHECK(PKIX_List_GetLength
                        (rootNode->children, &numberOfChildren, plContext),
                        PKIX_LISTGETLENGTHFAILED);
        }

        if (numberOfChildren != 0) {
                /*
                 * We create a string for each child in turn,
                 * concatenating them to thisItemString.
                 */

                /* Prepare an indent string for each child */
                if (indent) {
                        PKIX_CHECK(PKIX_PL_String_Create
                                (PKIX_ESCASCII,
                                "%s. ",
                                0,
                                &nextIndentFormat,
                                plContext),
                                PKIX_ERRORCREATINGFORMATSTRING);

                        PKIX_CHECK(PKIX_PL_Sprintf
                                (&nextIndentString,
                                plContext,
                                nextIndentFormat,
                                indent),
                                PKIX_ERRORINSPRINTF);
                } else {
                        PKIX_CHECK(PKIX_PL_String_Create
                                (PKIX_ESCASCII,
                                ". ",
                                0,
                                &nextIndentString,
                                plContext),
                                PKIX_ERRORCREATINGINDENTSTRING);
                }

                /* Prepare the format for concatenation. */
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII,
                        "%s\n%s",
                        0,
                        &childrenFormat,
                        plContext),
                        PKIX_ERRORCREATINGFORMATSTRING);

                for (childIndex = 0;
                        childIndex < numberOfChildren;
                        childIndex++) {
                        PKIX_CHECK(PKIX_List_GetItem
                                (rootNode->children,
                                childIndex,
                                (PKIX_PL_Object **)&childNode,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(pkix_VerifyNode_ToString_Helper
                                (childNode,
                                nextIndentString,
                                &childString,
                                plContext),
                                PKIX_ERRORCREATINGCHILDSTRING);


                        PKIX_CHECK(PKIX_PL_Sprintf
                                (&resultString,
                                plContext,
                                childrenFormat,
                                thisItemString,
                                childString),
                        PKIX_ERRORINSPRINTF);

                        PKIX_DECREF(childNode);
                        PKIX_DECREF(childString);
                        PKIX_DECREF(thisItemString);

                        thisItemString = resultString;
                }
        }

        *pTreeString = thisItemString;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(thisItemString);
        }

        PKIX_DECREF(nextIndentFormat);
        PKIX_DECREF(thisNodeFormat);
        PKIX_DECREF(childrenFormat);
        PKIX_DECREF(nextIndentString);
        PKIX_DECREF(childString);
        PKIX_DECREF(childNode);

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_VerifyNode_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pTreeString,
        void *plContext)
{
        PKIX_VerifyNode *rootNode = NULL;
        PKIX_PL_String *resultString = NULL;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_ToString");

        PKIX_NULLCHECK_TWO(object, pTreeString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_VERIFYNODE_TYPE, plContext),
                PKIX_OBJECTNOTVERIFYNODE);

        rootNode = (PKIX_VerifyNode *)object;

        PKIX_CHECK(pkix_VerifyNode_ToString_Helper
                (rootNode, NULL, &resultString, plContext),
                PKIX_ERRORCREATINGSUBTREESTRING);

        *pTreeString = resultString;

cleanup:

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_VerifyNode_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_VerifyNode *node = NULL;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_Destroy");

        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_VERIFYNODE_TYPE, plContext),
                PKIX_OBJECTNOTVERIFYNODE);

        node = (PKIX_VerifyNode*)object;

        PKIX_DECREF(node->verifyCert);
        PKIX_DECREF(node->children);
        PKIX_DECREF(node->error);

        node->depth = 0;

cleanup:

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_SingleVerifyNode_Hashcode
 * DESCRIPTION:
 *
 *  Computes the hashcode of the attributes of the VerifyNode pointed to by
 *  "node", other than its parents and children, and stores the result at
 *  "pHashcode".
 *
 * PARAMETERS:
 *  "node"
 *      Address of VerifyNode to be hashcoded; must be non-NULL
 *  "pHashcode"
 *      Address where UInt32 result will be stored; must be non-NULL
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if function succeeds
 *  Returns a VerifyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
static PKIX_Error *
pkix_SingleVerifyNode_Hashcode(
        PKIX_VerifyNode *node,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_UInt32 errorHash = 0;
        PKIX_UInt32 nodeHash = 0;

        PKIX_ENTER(VERIFYNODE, "pkix_SingleVerifyNode_Hashcode");
        PKIX_NULLCHECK_TWO(node, pHashcode);

        PKIX_HASHCODE
                (node->verifyCert,
                &nodeHash,
                plContext,
                PKIX_FAILUREHASHINGCERT);

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                ((PKIX_PL_Object *)node->error,
                &errorHash,
                plContext),
                PKIX_FAILUREHASHINGERROR);

        nodeHash = 31*nodeHash + errorHash;
        *pHashcode = nodeHash;

cleanup:

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_VerifyNode_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_VerifyNode *node = NULL;
        PKIX_UInt32 childrenHash = 0;
        PKIX_UInt32 nodeHash = 0;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_VERIFYNODE_TYPE, plContext),
                PKIX_OBJECTNOTVERIFYNODE);

        node = (PKIX_VerifyNode *)object;

        PKIX_CHECK(pkix_SingleVerifyNode_Hashcode
                (node, &nodeHash, plContext),
                PKIX_SINGLEVERIFYNODEHASHCODEFAILED);

        PKIX_HASHCODE
                (node->children,
                &childrenHash,
                plContext,
                PKIX_OBJECTHASHCODEFAILED);

        nodeHash = 31*nodeHash + childrenHash;

        *pHashcode = nodeHash;

cleanup:

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_SingleVerifyNode_Equals
 * DESCRIPTION:
 *
 *  Compares for equality the components of the VerifyNode pointed to by
 *  "firstPN", other than its parents and children, with those of the
 *  VerifyNode pointed to by "secondPN" and stores the result at "pResult"
 *  (PKIX_TRUE if equal; PKIX_FALSE if not).
 *
 * PARAMETERS:
 *  "firstPN"
 *      Address of first of the VerifyNodes to be compared; must be non-NULL
 *  "secondPN"
 *      Address of second of the VerifyNodes to be compared; must be non-NULL
 *  "pResult"
 *      Address where Boolean will be stored; must be non-NULL
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if function succeeds
 *  Returns a VerifyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
static PKIX_Error *
pkix_SingleVerifyNode_Equals(
        PKIX_VerifyNode *firstVN,
        PKIX_VerifyNode *secondVN,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_Boolean compResult = PKIX_FALSE;

        PKIX_ENTER(VERIFYNODE, "pkix_SingleVerifyNode_Equals");
        PKIX_NULLCHECK_THREE(firstVN, secondVN, pResult);

        /* If both references are identical, they must be equal */
        if (firstVN == secondVN) {
                compResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * It seems we have to do the comparisons. Do
         * the easiest ones first.
         */
        if ((firstVN->depth) != (secondVN->depth)) {
                goto cleanup;
        }

        /* These fields must be non-NULL */
        PKIX_NULLCHECK_TWO(firstVN->verifyCert, secondVN->verifyCert);

        PKIX_EQUALS
                (firstVN->verifyCert,
                secondVN->verifyCert,
                &compResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (compResult == PKIX_FALSE) {
                goto cleanup;
        }

        PKIX_EQUALS
                (firstVN->error,
                secondVN->error,
                &compResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

cleanup:

        *pResult = compResult;

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_VerifyNode_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_VerifyNode *firstVN = NULL;
        PKIX_VerifyNode *secondVN = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean compResult = PKIX_FALSE;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a VerifyNode */
        PKIX_CHECK(pkix_CheckType
                (firstObject, PKIX_VERIFYNODE_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTVERIFYNODE);

        /*
         * Since we know firstObject is a VerifyNode,
         * if both references are identical, they must be equal
         */
        if (firstObject == secondObject){
                compResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a VerifyNode, we
         * don't throw an error. We simply return FALSE.
         */
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        if (secondType != PKIX_VERIFYNODE_TYPE) {
                goto cleanup;
        }

        /*
         * Oh, well, we have to do the comparisons. Do
         * the easiest ones first.
         */
        firstVN = (PKIX_VerifyNode *)firstObject;
        secondVN = (PKIX_VerifyNode *)secondObject;

        PKIX_CHECK(pkix_SingleVerifyNode_Equals
                (firstVN, secondVN, &compResult, plContext),
                PKIX_SINGLEVERIFYNODEEQUALSFAILED);

        if (compResult == PKIX_FALSE) {
                goto cleanup;
        }

        PKIX_EQUALS
                (firstVN->children,
                secondVN->children,
                &compResult,
                plContext,
                PKIX_OBJECTEQUALSFAILEDONCHILDREN);

cleanup:

        *pResult = compResult;

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_DuplicateHelper
 * DESCRIPTION:
 *
 *  Duplicates the VerifyNode whose address is pointed to by "original",
 *  and stores the result at "pNewNode", if a non-NULL pointer is provided
 *  for "pNewNode". In addition, the created VerifyNode is added as a child
 *  to "parent", if a non-NULL pointer is provided for "parent". Then this
 *  function is called recursively to duplicate each of the children of
 *  "original". At the top level this function is called with a null
 *  "parent" and a non-NULL "pNewNode". Below the top level "parent" will
 *  be non-NULL and "pNewNode" will be NULL.
 *
 * PARAMETERS:
 *  "original"
 *      Address of VerifyNode to be copied; must be non-NULL
 *  "parent"
 *      Address of VerifyNode to which the created node is to be added as a
 *      child; NULL for the top-level call and non-NULL below the top level
 *  "pNewNode"
 *      Address to store the node created; should be NULL if "parent" is
 *      non-NULL and vice versa
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if function succeeds
 *  Returns a VerifyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
static PKIX_Error *
pkix_VerifyNode_DuplicateHelper(
        PKIX_VerifyNode *original,
        PKIX_VerifyNode *parent,
        PKIX_VerifyNode **pNewNode,
        void *plContext)
{
        PKIX_UInt32 numChildren = 0;
        PKIX_UInt32 childIndex = 0;
        PKIX_List *children = NULL; /* List of PKIX_VerifyNode */
        PKIX_VerifyNode *copy = NULL;
        PKIX_VerifyNode *child = NULL;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_DuplicateHelper");

        PKIX_NULLCHECK_TWO
                (original, original->verifyCert);

        /*
         * These components are immutable, so copying the pointers
         * is sufficient. The create function increments the reference
         * counts as it stores the pointers into the new object.
         */
        PKIX_CHECK(pkix_VerifyNode_Create
                (original->verifyCert,
                original->depth,
                original->error,
                &copy,
                plContext),
                PKIX_VERIFYNODECREATEFAILED);

        /* Are there any children to duplicate? */
        children = original->children;

        if (children) {
            PKIX_CHECK(PKIX_List_GetLength(children, &numChildren, plContext),
                PKIX_LISTGETLENGTHFAILED);
        }

        for (childIndex = 0; childIndex < numChildren; childIndex++) {
                PKIX_CHECK(PKIX_List_GetItem
                        (children,
                        childIndex,
                        (PKIX_PL_Object **)&child,
                        plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(pkix_VerifyNode_DuplicateHelper
                        (child, copy, NULL, plContext),
                        PKIX_VERIFYNODEDUPLICATEHELPERFAILED);

                PKIX_DECREF(child);
        }

        if (pNewNode) {
                *pNewNode = copy;
                copy = NULL; /* no DecRef if we give our handle away */
        }

cleanup:
        PKIX_DECREF(copy);
        PKIX_DECREF(child);

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_Duplicate
 * (see comments for PKIX_PL_Duplicate_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_VerifyNode_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_VerifyNode *original = NULL;
        PKIX_VerifyNode *copy = NULL;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_Duplicate");

        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_VERIFYNODE_TYPE, plContext),
                PKIX_OBJECTNOTVERIFYNODE);

        original = (PKIX_VerifyNode *)object;

        PKIX_CHECK(pkix_VerifyNode_DuplicateHelper
                (original, NULL, &copy, plContext),
                PKIX_VERIFYNODEDUPLICATEHELPERFAILED);

        *pNewObject = (PKIX_PL_Object *)copy;

cleanup:

        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: pkix_VerifyNode_RegisterSelf
 * DESCRIPTION:
 *
 *  Registers PKIX_VERIFYNODE_TYPE and its related
 *  functions with systemClasses[]
 *
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize,
 *  which should only be called once, it is acceptable that
 *  this function is not thread-safe.
 */
PKIX_Error *
pkix_VerifyNode_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(VERIFYNODE, "pkix_VerifyNode_RegisterSelf");

        entry.description = "VerifyNode";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_VerifyNode);
        entry.destructor = pkix_VerifyNode_Destroy;
        entry.equalsFunction = pkix_VerifyNode_Equals;
        entry.hashcodeFunction = pkix_VerifyNode_Hashcode;
        entry.toStringFunction = pkix_VerifyNode_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_VerifyNode_Duplicate;

        systemClasses[PKIX_VERIFYNODE_TYPE] = entry;

        PKIX_RETURN(VERIFYNODE);
}

/* --Public-VerifyNode-Functions----------------------------------- */

/*
 * FUNCTION: PKIX_VerifyNode_SetError
 * DESCRIPTION:
 *
 *  This function sets the Error field of the VerifyNode pointed to by "node"
 *  to contain the Error pointed to by "error".
 *
 * PARAMETERS:
 *  "node"
 *      The address of the VerifyNode to be modified. Must be non-NULL.
 *  "error"
 *      The address of the Error to be stored.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_VerifyNode_SetError(
        PKIX_VerifyNode *node,
        PKIX_Error *error,
        void *plContext)
{

        PKIX_ENTER(VERIFYNODE, "PKIX_VerifyNode_SetError");

        PKIX_NULLCHECK_TWO(node, error);

        PKIX_DECREF(node->error); /* should have been NULL */
        PKIX_INCREF(error);
        node->error = error;

cleanup:
        PKIX_RETURN(VERIFYNODE);
}

/*
 * FUNCTION: PKIX_VerifyNode_FindError
 * DESCRIPTION:
 *
 * Finds meaningful error in the log. For now, just returns the first
 * error it finds in. In the future the function should be changed to
 * return a top priority error.
 *
 * PARAMETERS:
 *  "node"
 *      The address of the VerifyNode to be modified. Must be non-NULL.
 *  "error"
 *      The address of a pointer the error will be returned to.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_VerifyNode_FindError(
        PKIX_VerifyNode *node,
        PKIX_Error **error,
        void *plContext)
{
    PKIX_VerifyNode *childNode = NULL;

    PKIX_ENTER(VERIFYNODE, "PKIX_VerifyNode_FindError");

    /* Make sure the return address is initialized with NULL */
    PKIX_DECREF(*error);

    if (!node)
        goto cleanup;
    
    /* First, try to get error from lowest level. */
    if (node->children) {
        PKIX_UInt32 length = 0;
        PKIX_UInt32 index = 0;

        PKIX_CHECK(
            PKIX_List_GetLength(node->children, &length,
                                plContext),
            PKIX_LISTGETLENGTHFAILED);
        for (index = 0;index < length;index++) {
            PKIX_CHECK(
                PKIX_List_GetItem(node->children, index,
                                  (PKIX_PL_Object**)&childNode, plContext),
                PKIX_LISTGETITEMFAILED);
            if (!childNode)
                continue;
            PKIX_CHECK(
                pkix_VerifyNode_FindError(childNode, error,
                                          plContext),
                PKIX_VERIFYNODEFINDERRORFAILED);
            PKIX_DECREF(childNode);
            if (*error) {
                goto cleanup;
            }
        }
    }
    
    if (node->error && node->error->plErr) {
        PKIX_INCREF(node->error);
        *error = node->error;
    }

cleanup:
    PKIX_DECREF(childNode);
    
    PKIX_RETURN(VERIFYNODE);
}
