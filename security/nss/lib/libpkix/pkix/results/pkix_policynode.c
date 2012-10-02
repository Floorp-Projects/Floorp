/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_policynode.c
 *
 * Policy Node Object Type Definition
 *
 */

#include "pkix_policynode.h"

/* --Private-PolicyNode-Functions---------------------------------- */

/*
 * FUNCTION: pkix_PolicyNode_GetChildrenMutable
 * DESCRIPTION:
 *
 *  Retrieves the List of PolicyNodes representing the child nodes of the
 *  Policy Node pointed to by "node" and stores it at "pChildren". If "node"
 *  has no List of child nodes, this function stores NULL at "pChildren".
 *
 *  Note that the List returned by this function may be mutable. This function
 *  differs from the public function PKIX_PolicyNode_GetChildren in that
 *  respect. (It also differs in that the public function creates an empty
 *  List, if necessary, rather than storing NULL.)
 *
 *  During certificate processing, children Lists are created and modified.
 *  Once the list is accessed using the public call, the List is set immutable.
 *
 * PARAMETERS:
 *  "node"
 *      Address of PolicyNode whose child nodes are to be stored.
 *      Must be non-NULL.
 *  "pChildren"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_PolicyNode_GetChildrenMutable(
        PKIX_PolicyNode *node,
        PKIX_List **pChildren,  /* list of PKIX_PolicyNode */
        void *plContext)
{

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_GetChildrenMutable");

        PKIX_NULLCHECK_TWO(node, pChildren);

        PKIX_INCREF(node->children);

        *pChildren = node->children;

cleanup:
        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_Create
 * DESCRIPTION:
 *
 *  Creates a new PolicyNode using the OID pointed to by "validPolicy", the List
 *  of CertPolicyQualifiers pointed to by "qualifierSet", the criticality
 *  indicated by the Boolean value of "criticality", and the List of OIDs
 *  pointed to by "expectedPolicySet", and stores the result at "pObject". The
 *  criticality should be derived from whether the certificate policy extension
 *  was marked as critical in the certificate that led to creation of this
 *  PolicyNode. The "qualifierSet" and "expectedPolicySet" Lists are made
 *  immutable. The PolicyNode pointers to parent and to children are initialized
 *  to NULL, and the depth is set to zero; those values should be set by using
 *  the pkix_PolicyNode_AddToParent function.
 *
 * PARAMETERS
 *  "validPolicy"
 *      Address of OID of the valid policy for the path. Must be non-NULL
 *  "qualifierSet"
 *      Address of List of CertPolicyQualifiers associated with the validpolicy.
 *      May be NULL
 *  "criticality"
 *      Boolean indicator of whether the criticality should be set in this
 *      PolicyNode
 *  "expectedPolicySet"
 *      Address of List of OIDs that would satisfy this policy in the next
 *      certificate. Must be non-NULL
 *  "pObject"
 *      Address where the PolicyNode pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a PolicyNode Error if the function fails  in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_PolicyNode_Create(
        PKIX_PL_OID *validPolicy,
        PKIX_List *qualifierSet,
        PKIX_Boolean criticality,
        PKIX_List *expectedPolicySet,
        PKIX_PolicyNode **pObject,
        void *plContext)
{
        PKIX_PolicyNode *node = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_Create");

        PKIX_NULLCHECK_THREE(validPolicy, expectedPolicySet, pObject);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                (PKIX_CERTPOLICYNODE_TYPE,
                sizeof (PKIX_PolicyNode),
                (PKIX_PL_Object **)&node,
                plContext),
                PKIX_COULDNOTCREATEPOLICYNODEOBJECT);

        PKIX_INCREF(validPolicy);
        node->validPolicy = validPolicy;

        PKIX_INCREF(qualifierSet);
        node->qualifierSet = qualifierSet;
        if (qualifierSet) {
                PKIX_CHECK(PKIX_List_SetImmutable(qualifierSet, plContext),
                        PKIX_LISTSETIMMUTABLEFAILED);
        }

        node->criticality = criticality;

        PKIX_INCREF(expectedPolicySet);
        node->expectedPolicySet = expectedPolicySet;
        PKIX_CHECK(PKIX_List_SetImmutable(expectedPolicySet, plContext),
                PKIX_LISTSETIMMUTABLEFAILED);

        node->parent = NULL;
        node->children = NULL;
        node->depth = 0;

        *pObject = node;
        node = NULL;

cleanup:

        PKIX_DECREF(node);

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_AddToParent
 * DESCRIPTION:
 *
 *  Adds the PolicyNode pointed to by "child" to the List of children of
 *  the PolicyNode pointed to by "parentNode". If "parentNode" had a
 *  NULL pointer for the List of children, a new List is created containing
 *  "child". Otherwise "child" is appended to the existing List. The
 *  parent field in "child" is set to "parent", and the depth field is
 *  set to one more than the corresponding value in "parent".
 *
 *  Depth, in this context, means distance from the root node, which
 *  is at depth zero.
 *
 * PARAMETERS:
 *  "parentNode"
 *      Address of PolicyNode whose List of child PolicyNodes is to be
 *      created or appended to. Must be non-NULL.
 *  "child"
 *      Address of PolicyNode to be added to parentNode's List. Must be
 *      non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_PolicyNode_AddToParent(
        PKIX_PolicyNode *parentNode,
        PKIX_PolicyNode *child,
        void *plContext)
{
        PKIX_List *listOfChildren = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_AddToParent");

        PKIX_NULLCHECK_TWO(parentNode, child);

        listOfChildren = parentNode->children;
        if (listOfChildren == NULL) {
                PKIX_CHECK(PKIX_List_Create(&listOfChildren, plContext),
                        PKIX_LISTCREATEFAILED);
                parentNode->children = listOfChildren;
        }

        /*
         * Note: this link is not reference-counted. The link from parent
         * to child is counted (actually, the parent "owns" a List which
         * "owns" children), but the children do not "own" the parent.
         * Otherwise, there would be loops.
         */
        child->parent = parentNode;

        child->depth = 1 + (parentNode->depth);

        PKIX_CHECK(PKIX_List_AppendItem
                (listOfChildren, (PKIX_PL_Object *)child, plContext),
                PKIX_COULDNOTAPPENDCHILDTOPARENTSPOLICYNODELIST);

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)parentNode, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)child, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_Prune
 * DESCRIPTION:
 *
 *  Prunes a tree below the PolicyNode whose address is pointed to by "node",
 *  using the UInt32 value of "height" as the distance from the leaf level,
 *  and storing at "pDelete" the Boolean value of whether this PolicyNode is,
 *  after pruning, childless and should be pruned.
 *
 *  Any PolicyNode at height 0 is allowed to survive. If the height is greater
 *  than zero, pkix_PolicyNode_Prune is called recursively for each child of
 *  the current PolicyNode. After this process, a node with no children
 *  stores PKIX_TRUE in "pDelete" to indicate that it should be deleted.
 *
 * PARAMETERS:
 *  "node"
 *      Address of the PolicyNode to be pruned. Must be non-NULL.
 *  "height"
 *      UInt32 value for the distance from the leaf level
 *  "pDelete"
 *      Address to store the Boolean return value of PKIX_TRUE if this node
 *      should be pruned, or PKIX_FALSE if there remains at least one
 *      branch of the required height. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
PKIX_Error *
pkix_PolicyNode_Prune(
        PKIX_PolicyNode *node,
        PKIX_UInt32 height,
        PKIX_Boolean *pDelete,
        void *plContext)
{
        PKIX_Boolean childless = PKIX_FALSE;
        PKIX_Boolean shouldBePruned = PKIX_FALSE;
        PKIX_UInt32 listSize = 0;
        PKIX_UInt32 listIndex = 0;
        PKIX_PolicyNode *candidate = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_Prune");

        PKIX_NULLCHECK_TWO(node, pDelete);

        /* Don't prune at the leaf */
        if (height == 0) {
                goto cleanup;
        }

        /* Above the bottom level, childless nodes get pruned */
        if (!(node->children)) {
                childless = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * This node has children. If they are leaf nodes,
         * we know they will live. Otherwise, check them out.
         */
        if (height > 1) {
                PKIX_CHECK(PKIX_List_GetLength
                        (node->children, &listSize, plContext),
                        PKIX_LISTGETLENGTHFAILED);
                /*
                 * By working backwards from the end of the list,
                 * we avoid having to worry about possible
                 * decreases in the size of the list, as we
                 * delete items. The only nuisance is that since the
                 * index is UInt32, we can't check for it to reach -1;
                 * we have to use the 1-based index, rather than the
                 * 0-based index that PKIX_List functions require.
                 */
                for (listIndex = listSize; listIndex > 0; listIndex--) {
                        PKIX_CHECK(PKIX_List_GetItem
                                (node->children,
                                (listIndex - 1),
                                (PKIX_PL_Object **)&candidate,
                                plContext),
                                PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(pkix_PolicyNode_Prune
                                (candidate,
                                height - 1,
                                &shouldBePruned,
                                plContext),
                                PKIX_POLICYNODEPRUNEFAILED);

                        if (shouldBePruned == PKIX_TRUE) {
                                PKIX_CHECK(PKIX_List_DeleteItem
                                        (node->children,
                                        (listIndex - 1),
                                        plContext),
                                        PKIX_LISTDELETEITEMFAILED);
                        }

                        PKIX_DECREF(candidate);
                }
        }

        /* Prune if this node has *become* childless */
        PKIX_CHECK(PKIX_List_GetLength
                (node->children, &listSize, plContext),
                PKIX_LISTGETLENGTHFAILED);
        if (listSize == 0) {
                childless = PKIX_TRUE;
        }

        /*
         * Even if we did not change this node, or any of its children,
         * maybe a [great-]*grandchild was pruned.
         */
        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                ((PKIX_PL_Object *)node, plContext),
                PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:
        *pDelete = childless;

        PKIX_DECREF(candidate);

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_SinglePolicyNode_ToString
 * DESCRIPTION:
 *
 *  Creates a String representation of the attributes of the PolicyNode
 *  pointed to by "node", other than its parents or children, and
 *  stores the result at "pString".
 *
 * PARAMETERS:
 *  "node"
 *      Address of PolicyNode to be described by the string. Must be non-NULL.
 *  "pString"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if function succeeds
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
PKIX_Error *
pkix_SinglePolicyNode_ToString(
        PKIX_PolicyNode *node,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *fmtString = NULL;
        PKIX_PL_String *validString = NULL;
        PKIX_PL_String *qualifierString = NULL;
        PKIX_PL_String *criticalityString = NULL;
        PKIX_PL_String *expectedString = NULL;
        PKIX_PL_String *outString = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_SinglePolicyNode_ToString");
        PKIX_NULLCHECK_TWO(node, pString);
        PKIX_NULLCHECK_TWO(node->validPolicy, node->expectedPolicySet);

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                "{%s,%s,%s,%s,%d}",
                0,
                &fmtString,
                plContext),
                PKIX_CANTCREATESTRING);

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)(node->validPolicy),
                &validString,
                plContext),
                PKIX_OIDTOSTRINGFAILED);

        PKIX_CHECK(PKIX_PL_Object_ToString
                ((PKIX_PL_Object *)(node->expectedPolicySet),
                &expectedString,
                plContext),
                PKIX_LISTTOSTRINGFAILED);

        if (node->qualifierSet) {
                PKIX_CHECK(PKIX_PL_Object_ToString
                        ((PKIX_PL_Object *)(node->qualifierSet),
                        &qualifierString,
                        plContext),
                        PKIX_LISTTOSTRINGFAILED);
        } else {
                PKIX_CHECK(PKIX_PL_String_Create
                        (PKIX_ESCASCII,
                        "{}",
                        0,
                        &qualifierString,
                        plContext),
                        PKIX_CANTCREATESTRING);
        }

        PKIX_CHECK(PKIX_PL_String_Create
                (PKIX_ESCASCII,
                (node->criticality)?"Critical":"Not Critical",
                0,
                &criticalityString,
                plContext),
                PKIX_CANTCREATESTRING);

        PKIX_CHECK(PKIX_PL_Sprintf
                (&outString,
                plContext,
                fmtString,
                validString,
                qualifierString,
                criticalityString,
                expectedString,
                node->depth),
                PKIX_SPRINTFFAILED);

        *pString = outString;

cleanup:

        PKIX_DECREF(fmtString);
        PKIX_DECREF(validString);
        PKIX_DECREF(qualifierString);
        PKIX_DECREF(criticalityString);
        PKIX_DECREF(expectedString);
        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_ToString_Helper
 * DESCRIPTION:
 *
 *  Produces a String representation of a PolicyNode tree below the PolicyNode
 *  pointed to by "rootNode", with each line of output prefixed by the String
 *  pointed to by "indent", and stores the result at "pTreeString". It is
 *  called recursively, with ever-increasing indentation, for successively
 *  lower nodes on the tree.
 *
 * PARAMETERS:
 *  "rootNode"
 *      Address of PolicyNode subtree. Must be non-NULL.
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
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_PolicyNode_ToString_Helper(
        PKIX_PolicyNode *rootNode,
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
        PKIX_PolicyNode *childNode = NULL;
        PKIX_UInt32 numberOfChildren = 0;
        PKIX_UInt32 childIndex = 0;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_ToString_Helper");

        PKIX_NULLCHECK_TWO(rootNode, pTreeString);

        /* Create a string for this node */
        PKIX_CHECK(pkix_SinglePolicyNode_ToString
                (rootNode, &thisItemString, plContext),
                PKIX_ERRORINSINGLEPOLICYNODETOSTRING);

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

                        PKIX_CHECK(pkix_PolicyNode_ToString_Helper
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

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_PolicyNode_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pTreeString,
        void *plContext)
{
        PKIX_PolicyNode *rootNode = NULL;
        PKIX_PL_String *resultString = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_ToString");

        PKIX_NULLCHECK_TWO(object, pTreeString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CERTPOLICYNODE_TYPE, plContext),
                PKIX_OBJECTNOTPOLICYNODE);

        rootNode = (PKIX_PolicyNode *)object;

        PKIX_CHECK(pkix_PolicyNode_ToString_Helper
                (rootNode, NULL, &resultString, plContext),
                PKIX_ERRORCREATINGSUBTREESTRING);

        *pTreeString = resultString;

cleanup:

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_PolicyNode_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PolicyNode *node = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_Destroy");

        PKIX_NULLCHECK_ONE(object);

        PKIX_CHECK(pkix_CheckType(object, PKIX_CERTPOLICYNODE_TYPE, plContext),
                PKIX_OBJECTNOTPOLICYNODE);

        node = (PKIX_PolicyNode*)object;

        node->criticality = PKIX_FALSE;
        PKIX_DECREF(node->validPolicy);
        PKIX_DECREF(node->qualifierSet);
        PKIX_DECREF(node->expectedPolicySet);
        PKIX_DECREF(node->children);

        /*
         * Note: the link to parent is not reference-counted. See comment
         * in pkix_PolicyNode_AddToParent for more details.
         */
        node->parent = NULL;
        node->depth = 0;

cleanup:

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_SinglePolicyNode_Hashcode
 * DESCRIPTION:
 *
 *  Computes the hashcode of the attributes of the PolicyNode pointed to by
 *  "node", other than its parents and children, and stores the result at
 *  "pHashcode".
 *
 * PARAMETERS:
 *  "node"
 *      Address of PolicyNode to be hashcoded; must be non-NULL
 *  "pHashcode"
 *      Address where UInt32 result will be stored; must be non-NULL
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if function succeeds
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
static PKIX_Error *
pkix_SinglePolicyNode_Hashcode(
        PKIX_PolicyNode *node,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_UInt32 componentHash = 0;
        PKIX_UInt32 nodeHash = 0;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_SinglePolicyNode_Hashcode");
        PKIX_NULLCHECK_TWO(node, pHashcode);
        PKIX_NULLCHECK_TWO(node->validPolicy, node->expectedPolicySet);

        PKIX_HASHCODE
                (node->qualifierSet,
                &nodeHash,
                plContext,
                PKIX_FAILUREHASHINGLISTQUALIFIERSET);

        if (PKIX_TRUE == (node->criticality)) {
                nodeHash = 31*nodeHash + 0xff;
        } else {
                nodeHash = 31*nodeHash + 0x00;
        }

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                ((PKIX_PL_Object *)node->validPolicy,
                &componentHash,
                plContext),
                PKIX_FAILUREHASHINGOIDVALIDPOLICY);

        nodeHash = 31*nodeHash + componentHash;

        PKIX_CHECK(PKIX_PL_Object_Hashcode
                ((PKIX_PL_Object *)node->expectedPolicySet,
                &componentHash,
                plContext),
                PKIX_FAILUREHASHINGLISTEXPECTEDPOLICYSET);

        nodeHash = 31*nodeHash + componentHash;

        *pHashcode = nodeHash;

cleanup:

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_PolicyNode_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_PolicyNode *node = NULL;
        PKIX_UInt32 childrenHash = 0;
        PKIX_UInt32 nodeHash = 0;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTPOLICYNODE_TYPE, plContext),
                PKIX_OBJECTNOTPOLICYNODE);

        node = (PKIX_PolicyNode *)object;

        PKIX_CHECK(pkix_SinglePolicyNode_Hashcode
                (node, &nodeHash, plContext),
                PKIX_SINGLEPOLICYNODEHASHCODEFAILED);

        nodeHash = 31*nodeHash + (PKIX_UInt32)(node->parent);

        PKIX_HASHCODE
                (node->children,
                &childrenHash,
                plContext,
                PKIX_OBJECTHASHCODEFAILED);

        nodeHash = 31*nodeHash + childrenHash;

        *pHashcode = nodeHash;

cleanup:

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_SinglePolicyNode_Equals
 * DESCRIPTION:
 *
 *  Compares for equality the components of the PolicyNode pointed to by
 *  "firstPN", other than its parents and children, with those of the
 *  PolicyNode pointed to by "secondPN" and stores the result at "pResult"
 *  (PKIX_TRUE if equal; PKIX_FALSE if not).
 *
 * PARAMETERS:
 *  "firstPN"
 *      Address of first of the PolicyNodes to be compared; must be non-NULL
 *  "secondPN"
 *      Address of second of the PolicyNodes to be compared; must be non-NULL
 *  "pResult"
 *      Address where Boolean will be stored; must be non-NULL
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if function succeeds
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
static PKIX_Error *
pkix_SinglePolicyNode_Equals(
        PKIX_PolicyNode *firstPN,
        PKIX_PolicyNode *secondPN,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_Boolean compResult = PKIX_FALSE;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_SinglePolicyNode_Equals");
        PKIX_NULLCHECK_THREE(firstPN, secondPN, pResult);

        /* If both references are identical, they must be equal */
        if (firstPN == secondPN) {
                compResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * It seems we have to do the comparisons. Do
         * the easiest ones first.
         */
        if ((firstPN->criticality) != (secondPN->criticality)) {
                goto cleanup;
        }
        if ((firstPN->depth) != (secondPN->depth)) {
                goto cleanup;
        }

        PKIX_EQUALS
                (firstPN->qualifierSet,
                secondPN->qualifierSet,
                &compResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (compResult == PKIX_FALSE) {
                goto cleanup;
        }

        /* These fields must be non-NULL */
        PKIX_NULLCHECK_TWO(firstPN->validPolicy, secondPN->validPolicy);

        PKIX_EQUALS
                (firstPN->validPolicy,
                secondPN->validPolicy,
                &compResult,
                plContext,
                PKIX_OBJECTEQUALSFAILED);

        if (compResult == PKIX_FALSE) {
                goto cleanup;
        }

        /* These fields must be non-NULL */
        PKIX_NULLCHECK_TWO
                (firstPN->expectedPolicySet, secondPN->expectedPolicySet);

        PKIX_EQUALS
                (firstPN->expectedPolicySet,
                secondPN->expectedPolicySet,
                &compResult,
                plContext,
                PKIX_OBJECTEQUALSFAILEDONEXPECTEDPOLICYSETS);

cleanup:

        *pResult = compResult;

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_Equals
 * (see comments for PKIX_PL_Equals_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_PolicyNode_Equals(
        PKIX_PL_Object *firstObject,
        PKIX_PL_Object *secondObject,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_PolicyNode *firstPN = NULL;
        PKIX_PolicyNode *secondPN = NULL;
        PKIX_UInt32 secondType;
        PKIX_Boolean compResult = PKIX_FALSE;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_Equals");
        PKIX_NULLCHECK_THREE(firstObject, secondObject, pResult);

        /* test that firstObject is a PolicyNode */
        PKIX_CHECK(pkix_CheckType
                (firstObject, PKIX_CERTPOLICYNODE_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTPOLICYNODE);

        /*
         * Since we know firstObject is a PolicyNode,
         * if both references are identical, they must be equal
         */
        if (firstObject == secondObject){
                compResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If secondObject isn't a PolicyNode, we
         * don't throw an error. We simply return FALSE.
         */
        PKIX_CHECK(PKIX_PL_Object_GetType
                    (secondObject, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);

        if (secondType != PKIX_CERTPOLICYNODE_TYPE) {
                goto cleanup;
        }

        /*
         * Oh, well, we have to do the comparisons. Do
         * the easiest ones first.
         */
        firstPN = (PKIX_PolicyNode *)firstObject;
        secondPN = (PKIX_PolicyNode *)secondObject;

        /*
         * We don't require the parents to be identical. In the
         * course of traversing the tree, we will have checked the
         * attributes of the parent nodes, and checking the lists
         * of children will determine whether they match.
         */

        PKIX_EQUALS
                (firstPN->children,
                secondPN->children,
                &compResult,
                plContext,
                PKIX_OBJECTEQUALSFAILEDONCHILDREN);

        if (compResult == PKIX_FALSE) {
                goto cleanup;
        }

        PKIX_CHECK(pkix_SinglePolicyNode_Equals
                (firstPN, secondPN, &compResult, plContext),
                PKIX_SINGLEPOLICYNODEEQUALSFAILED);

cleanup:

        *pResult = compResult;

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_DuplicateHelper
 * DESCRIPTION:
 *
 *  Duplicates the PolicyNode whose address is pointed to by "original",
 *  and stores the result at "pNewNode", if a non-NULL pointer is provided
 *  for "pNewNode". In addition, the created PolicyNode is added as a child
 *  to "parent", if a non-NULL pointer is provided for "parent". Then this
 *  function is called recursively to duplicate each of the children of
 *  "original". At the top level this function is called with a null
 *  "parent" and a non-NULL "pNewNode". Below the top level "parent" will
 *  be non-NULL and "pNewNode" will be NULL.
 *
 * PARAMETERS:
 *  "original"
 *      Address of PolicyNode to be copied; must be non-NULL
 *  "parent"
 *      Address of PolicyNode to which the created node is to be added as a
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
 *  Returns a PolicyNode Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in a fatal way
 */
static PKIX_Error *
pkix_PolicyNode_DuplicateHelper(
        PKIX_PolicyNode *original,
        PKIX_PolicyNode *parent,
        PKIX_PolicyNode **pNewNode,
        void *plContext)
{
        PKIX_UInt32 numChildren = 0;
        PKIX_UInt32 childIndex = 0;
        PKIX_List *children = NULL; /* List of PKIX_PolicyNode */
        PKIX_PolicyNode *copy = NULL;
        PKIX_PolicyNode *child = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_DuplicateHelper");

        PKIX_NULLCHECK_THREE
                (original, original->validPolicy, original->expectedPolicySet);

        /*
         * These components are immutable, so copying the pointers
         * is sufficient. The create function increments the reference
         * counts as it stores the pointers into the new object.
         */
        PKIX_CHECK(pkix_PolicyNode_Create
                (original->validPolicy,
                original->qualifierSet,
                original->criticality,
                original->expectedPolicySet,
                &copy,
                plContext),
                PKIX_POLICYNODECREATEFAILED);

        if (parent) {
                PKIX_CHECK(pkix_PolicyNode_AddToParent(parent, copy, plContext),
                        PKIX_POLICYNODEADDTOPARENTFAILED);
        }

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

                PKIX_CHECK(pkix_PolicyNode_DuplicateHelper
                        (child, copy, NULL, plContext),
                        PKIX_POLICYNODEDUPLICATEHELPERFAILED);

                PKIX_DECREF(child);
        }

        if (pNewNode) {
                *pNewNode = copy;
                copy = NULL; /* no DecRef if we give our handle away */
        }

cleanup:
        PKIX_DECREF(copy);
        PKIX_DECREF(child);

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_Duplicate
 * (see comments for PKIX_PL_Duplicate_Callback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_PolicyNode_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_PolicyNode *original = NULL;
        PKIX_PolicyNode *copy = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_Duplicate");

        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType
                (object, PKIX_CERTPOLICYNODE_TYPE, plContext),
                PKIX_OBJECTNOTPOLICYNODE);

        original = (PKIX_PolicyNode *)object;

        PKIX_CHECK(pkix_PolicyNode_DuplicateHelper
                (original, NULL, &copy, plContext),
                PKIX_POLICYNODEDUPLICATEHELPERFAILED);

        *pNewObject = (PKIX_PL_Object *)copy;

cleanup:

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: pkix_PolicyNode_RegisterSelf
 * DESCRIPTION:
 *
 *  Registers PKIX_CERTPOLICYNODE_TYPE and its related
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
pkix_PolicyNode_RegisterSelf(void *plContext)
{

        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(CERTPOLICYNODE, "pkix_PolicyNode_RegisterSelf");

        entry.description = "PolicyNode";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_PolicyNode);
        entry.destructor = pkix_PolicyNode_Destroy;
        entry.equalsFunction = pkix_PolicyNode_Equals;
        entry.hashcodeFunction = pkix_PolicyNode_Hashcode;
        entry.toStringFunction = pkix_PolicyNode_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_PolicyNode_Duplicate;

        systemClasses[PKIX_CERTPOLICYNODE_TYPE] = entry;

        PKIX_RETURN(CERTPOLICYNODE);
}


/* --Public-PolicyNode-Functions----------------------------------- */

/*
 * FUNCTION: PKIX_PolicyNode_GetChildren
 * (see description of this function in pkix_results.h)
 */
PKIX_Error *
PKIX_PolicyNode_GetChildren(
        PKIX_PolicyNode *node,
        PKIX_List **pChildren,  /* list of PKIX_PolicyNode */
        void *plContext)
{
        PKIX_List *children = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "PKIX_PolicyNode_GetChildren");

        PKIX_NULLCHECK_TWO(node, pChildren);

        PKIX_INCREF(node->children);
        children = node->children;

        if (!children) {
                PKIX_CHECK(PKIX_List_Create(&children, plContext),
                        PKIX_LISTCREATEFAILED);
        }

        PKIX_CHECK(PKIX_List_SetImmutable(children, plContext),
                PKIX_LISTSETIMMUTABLEFAILED);

        *pChildren = children;

cleanup:
        if (PKIX_ERROR_RECEIVED) {
                PKIX_DECREF(children);
        }

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: PKIX_PolicyNode_GetParent
 * (see description of this function in pkix_results.h)
 */
PKIX_Error *
PKIX_PolicyNode_GetParent(
        PKIX_PolicyNode *node,
        PKIX_PolicyNode **pParent,
        void *plContext)
{

        PKIX_ENTER(CERTPOLICYNODE, "PKIX_PolicyNode_GetParent");

        PKIX_NULLCHECK_TWO(node, pParent);

        PKIX_INCREF(node->parent);
        *pParent = node->parent;

cleanup:
        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: PKIX_PolicyNode_GetValidPolicy
 * (see description of this function in pkix_results.h)
 */
PKIX_Error *
PKIX_PolicyNode_GetValidPolicy(
        PKIX_PolicyNode *node,
        PKIX_PL_OID **pValidPolicy,
        void *plContext)
{

        PKIX_ENTER(CERTPOLICYNODE, "PKIX_PolicyNode_GetValidPolicy");

        PKIX_NULLCHECK_TWO(node, pValidPolicy);

        PKIX_INCREF(node->validPolicy);
        *pValidPolicy = node->validPolicy;

cleanup:
        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: PKIX_PolicyNode_GetPolicyQualifiers
 * (see description of this function in pkix_results.h)
 */
PKIX_Error *
PKIX_PolicyNode_GetPolicyQualifiers(
        PKIX_PolicyNode *node,
        PKIX_List **pQualifiers,  /* list of PKIX_PL_CertPolicyQualifier */
        void *plContext)
{
        PKIX_List *qualifiers = NULL;

        PKIX_ENTER(CERTPOLICYNODE, "PKIX_PolicyNode_GetPolicyQualifiers");

        PKIX_NULLCHECK_TWO(node, pQualifiers);

        PKIX_INCREF(node->qualifierSet);
        qualifiers = node->qualifierSet;

        if (!qualifiers) {
                PKIX_CHECK(PKIX_List_Create(&qualifiers, plContext),
                        PKIX_LISTCREATEFAILED);
        }

        PKIX_CHECK(PKIX_List_SetImmutable(qualifiers, plContext),
                PKIX_LISTSETIMMUTABLEFAILED);

        *pQualifiers = qualifiers;

cleanup:

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: PKIX_PolicyNode_GetExpectedPolicies
 * (see description of this function in pkix_results.h)
 */
PKIX_Error *
PKIX_PolicyNode_GetExpectedPolicies(
        PKIX_PolicyNode *node,
        PKIX_List **pExpPolicies,  /* list of PKIX_PL_OID */
        void *plContext)
{

        PKIX_ENTER(CERTPOLICYNODE, "PKIX_PolicyNode_GetExpectedPolicies");

        PKIX_NULLCHECK_TWO(node, pExpPolicies);

        PKIX_INCREF(node->expectedPolicySet);
        *pExpPolicies = node->expectedPolicySet;

cleanup:
        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: PKIX_PolicyNode_IsCritical
 * (see description of this function in pkix_results.h)
 */
PKIX_Error *
PKIX_PolicyNode_IsCritical(
        PKIX_PolicyNode *node,
        PKIX_Boolean *pCritical,
        void *plContext)
{

        PKIX_ENTER(CERTPOLICYNODE, "PKIX_PolicyNode_IsCritical");

        PKIX_NULLCHECK_TWO(node, pCritical);

        *pCritical = node->criticality;

        PKIX_RETURN(CERTPOLICYNODE);
}

/*
 * FUNCTION: PKIX_PolicyNode_GetDepth
 * (see description of this function in pkix_results.h)
 */
PKIX_Error *
PKIX_PolicyNode_GetDepth(
        PKIX_PolicyNode *node,
        PKIX_UInt32 *pDepth,
        void *plContext)
{

        PKIX_ENTER(CERTPOLICYNODE, "PKIX_PolicyNode_GetDepth");

        PKIX_NULLCHECK_TWO(node, pDepth);

        *pDepth = node->depth;

        PKIX_RETURN(CERTPOLICYNODE);
}
