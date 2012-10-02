/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * pkix_list.c
 *
 * List Object Functions
 *
 */

#include "pkix_list.h"

/* --Private-Functions-------------------------------------------- */

/*
 * FUNCTION: pkix_List_Create_Internal
 * DESCRIPTION:
 *
 *  Creates a new List, using the Boolean value of "isHeader" to determine
 *  whether the new List should be a header, and stores it at "pList". The
 *  List is initially empty and holds no items. To initially add items to
 *  the List, use PKIX_List_AppendItem.
 *
 * PARAMETERS:
 *  "isHeader"
 *      Boolean value indicating whether new List should be a header.
 *  "pList"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_List_Create_Internal(
        PKIX_Boolean isHeader,
        PKIX_List **pList,
        void *plContext)
{
        PKIX_List *list = NULL;

        PKIX_ENTER(LIST, "pkix_List_Create_Internal");
        PKIX_NULLCHECK_ONE(pList);

        PKIX_CHECK(PKIX_PL_Object_Alloc
                    (PKIX_LIST_TYPE,
                    ((PKIX_UInt32)(sizeof (PKIX_List))),
                    (PKIX_PL_Object **)&list, plContext),
                    PKIX_ERRORCREATINGLISTITEM);

        list->item = NULL;
        list->next = NULL;
        list->immutable = PKIX_FALSE;
        list->length = 0;
        list->isHeader = isHeader;

        *pList = list;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_Destroy
 * (see comments for PKIX_PL_DestructorCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_List_Destroy(
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_List *list = NULL;
        PKIX_List *nextItem = NULL;

        PKIX_ENTER(LIST, "pkix_List_Destroy");
        PKIX_NULLCHECK_ONE(object);

        /* Check that this object is a list */
        PKIX_CHECK(pkix_CheckType(object, PKIX_LIST_TYPE, plContext),
                    PKIX_OBJECTNOTLIST);

        list = (PKIX_List *)object;

        /* We have a valid list. DecRef its item and recurse on next */
        PKIX_DECREF(list->item);
        while ((nextItem = list->next) != NULL) {
            list->next = nextItem->next;
            nextItem->next = NULL;
            PKIX_DECREF(nextItem);
        }      
        list->immutable = PKIX_FALSE;
        list->length = 0;
        list->isHeader = PKIX_FALSE;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_ToString_Helper
 * DESCRIPTION:
 *
 *  Helper function that creates a string representation of the List pointed
 *  to by "list" and stores its address in the object pointed to by "pString".
 *
 * PARAMETERS
 *  "list"
 *      Address of List whose string representation is desired.
 *      Must be non-NULL.
 *  "pString"
 *      Address of object pointer's destination. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a List Error if the function fails in a non-fatal way.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_List_ToString_Helper(
        PKIX_List *list,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_PL_String *itemString = NULL;
        PKIX_PL_String *nextString = NULL;
        PKIX_PL_String *format = NULL;
        PKIX_Boolean empty;

        PKIX_ENTER(LIST, "pkix_List_ToString_Helper");
        PKIX_NULLCHECK_TWO(list, pString);

        /* special case when list is the header */
        if (list->isHeader){

                PKIX_CHECK(PKIX_List_IsEmpty(list, &empty, plContext),
                            PKIX_LISTISEMPTYFAILED);

                if (empty){
                        PKIX_CHECK(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    "EMPTY",
                                    0,
                                    &itemString,
                                    plContext),
                                    PKIX_ERRORCREATINGITEMSTRING);
                        (*pString) = itemString;
                        PKIX_DEBUG_EXIT(LIST);
                        return (NULL);
                } else {
                        PKIX_CHECK(pkix_List_ToString_Helper
                                    (list->next, &itemString, plContext),
                                    PKIX_LISTTOSTRINGHELPERFAILED);
                }

                /* Create a string object from the format */
                PKIX_CHECK(PKIX_PL_String_Create
                            (PKIX_ESCASCII, "%s", 0, &format, plContext),
                            PKIX_STRINGCREATEFAILED);

                PKIX_CHECK(PKIX_PL_Sprintf
                            (pString, plContext, format, itemString),
                            PKIX_SPRINTFFAILED);
        } else {
                /* Get a string for this list's item */
                if (list->item == NULL) {
                        PKIX_CHECK(PKIX_PL_String_Create
                                    (PKIX_ESCASCII,
                                    "(null)",
                                    0,
                                    &itemString,
                                    plContext),
                                    PKIX_STRINGCREATEFAILED);
                } else {
                        PKIX_CHECK(PKIX_PL_Object_ToString
                                    ((PKIX_PL_Object*)list->item,
                                    &itemString,
                                    plContext),
                                    PKIX_OBJECTTOSTRINGFAILED);
                }
                if (list->next == NULL) {
                        /* Just return the itemstring */
                        (*pString) = itemString;
                        PKIX_DEBUG_EXIT(LIST);
                        return (NULL);
                }

                /* Recursive call to get string for this list's next pointer */
                PKIX_CHECK(pkix_List_ToString_Helper
                            (list->next, &nextString, plContext),
                            PKIX_LISTTOSTRINGHELPERFAILED);

                /* Create a string object from the format */
                PKIX_CHECK(PKIX_PL_String_Create
                            (PKIX_ESCASCII,
                            "%s, %s",
                            0,
                            &format,
                            plContext),
                            PKIX_STRINGCREATEFAILED);

                PKIX_CHECK(PKIX_PL_Sprintf
                            (pString,
                            plContext,
                            format,
                            itemString,
                            nextString),
                            PKIX_SPRINTFFAILED);
        }

cleanup:

        PKIX_DECREF(itemString);
        PKIX_DECREF(nextString);
        PKIX_DECREF(format);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_ToString
 * (see comments for PKIX_PL_ToStringCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_List_ToString(
        PKIX_PL_Object *object,
        PKIX_PL_String **pString,
        void *plContext)
{
        PKIX_List *list = NULL;
        PKIX_PL_String *listString = NULL;
        PKIX_PL_String *format = NULL;

        PKIX_ENTER(LIST, "pkix_List_ToString");
        PKIX_NULLCHECK_TWO(object, pString);

        PKIX_CHECK(pkix_CheckType(object, PKIX_LIST_TYPE, plContext),
                    PKIX_OBJECTNOTLIST);

        list = (PKIX_List *)object;

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        PKIX_CHECK(pkix_List_ToString_Helper(list, &listString, plContext),
                    PKIX_LISTTOSTRINGHELPERFAILED);

        PKIX_CHECK(PKIX_PL_String_Create
                    (PKIX_ESCASCII, "(%s)", 0, &format, plContext),
                    PKIX_STRINGCREATEFAILED);

        PKIX_CHECK(PKIX_PL_Sprintf(pString, plContext, format, listString),
                    PKIX_SPRINTFFAILED);

cleanup:

        PKIX_DECREF(listString);
        PKIX_DECREF(format);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_Equals
 * (see comments for PKIX_PL_EqualsCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_List_Equals(
        PKIX_PL_Object *first,
        PKIX_PL_Object *second,
        PKIX_Boolean *pResult,
        void *plContext)
{
        PKIX_UInt32 secondType;
        PKIX_Boolean cmpResult;
        PKIX_List *firstList = NULL;
        PKIX_List *secondList = NULL;
        PKIX_UInt32 firstLength = 0;
        PKIX_UInt32 secondLength = 0;
        PKIX_PL_Object *firstItem = NULL;
        PKIX_PL_Object *secondItem = NULL;
        PKIX_UInt32 i = 0;

        PKIX_ENTER(LIST, "pkix_List_Equals");
        PKIX_NULLCHECK_THREE(first, second, pResult);

        /* test that first is a List */
        PKIX_CHECK(pkix_CheckType(first, PKIX_LIST_TYPE, plContext),
                PKIX_FIRSTOBJECTNOTLIST);

        /*
         * Since we know first is a List, if both references are
         * identical, they must be equal
         */
        if (first == second){
                *pResult = PKIX_TRUE;
                goto cleanup;
        }

        /*
         * If second isn't a List, we don't throw an error.
         * We simply return a Boolean result of FALSE
         */
        *pResult = PKIX_FALSE;
        PKIX_CHECK(PKIX_PL_Object_GetType(second, &secondType, plContext),
                    PKIX_COULDNOTGETTYPEOFSECONDARGUMENT);
        if (secondType != PKIX_LIST_TYPE) goto cleanup;

        firstList = (PKIX_List *)first;
        secondList = (PKIX_List *)second;

        if ((!firstList->isHeader) && (!secondList->isHeader)){
                PKIX_ERROR(PKIX_INPUTLISTSMUSTBELISTHEADERS);
        }

        firstLength = firstList->length;
        secondLength = secondList->length;

        cmpResult = PKIX_FALSE;
        if (firstLength == secondLength){
                for (i = 0, cmpResult = PKIX_TRUE;
                    ((i < firstLength) && cmpResult);
                    i++){
                        PKIX_CHECK(PKIX_List_GetItem
                                    (firstList, i, &firstItem, plContext),
                                    PKIX_LISTGETITEMFAILED);

                        PKIX_CHECK(PKIX_List_GetItem
                                    (secondList, i, &secondItem, plContext),
                                    PKIX_LISTGETITEMFAILED);

                        if ((!firstItem && secondItem) ||
                            (firstItem && !secondItem)){
                                        cmpResult = PKIX_FALSE;
                        } else if (!firstItem && !secondItem){
                                continue;
                        } else {
                                PKIX_CHECK(PKIX_PL_Object_Equals
                                            (firstItem,
                                            secondItem,
                                            &cmpResult,
                                            plContext),
                                            PKIX_OBJECTEQUALSFAILED);

                                PKIX_DECREF(firstItem);
                                PKIX_DECREF(secondItem);
                        }
                }
        }

        *pResult = cmpResult;

cleanup:

        PKIX_DECREF(firstItem);
        PKIX_DECREF(secondItem);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_Hashcode
 * (see comments for PKIX_PL_HashcodeCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_List_Hashcode(
        PKIX_PL_Object *object,
        PKIX_UInt32 *pHashcode,
        void *plContext)
{
        PKIX_List *list = NULL;
        PKIX_PL_Object *element = NULL;
        PKIX_UInt32 hash = 0;
        PKIX_UInt32 tempHash = 0;
        PKIX_UInt32 length, i;

        PKIX_ENTER(LIST, "pkix_List_Hashcode");
        PKIX_NULLCHECK_TWO(object, pHashcode);

        PKIX_CHECK(pkix_CheckType(object, PKIX_LIST_TYPE, plContext),
                    PKIX_OBJECTNOTLIST);

        list = (PKIX_List *)object;

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        length = list->length;

        for (i = 0; i < length; i++){
                PKIX_CHECK(PKIX_List_GetItem(list, i, &element, plContext),
                            PKIX_LISTGETITEMFAILED);

                if (!element){
                        tempHash = 100;
                } else {
                        PKIX_CHECK(PKIX_PL_Object_Hashcode
                                    (element, &tempHash, plContext),
                                    PKIX_LISTHASHCODEFAILED);
                }

                hash = 31 * hash + tempHash;

                PKIX_DECREF(element);
        }

        *pHashcode = hash;

cleanup:

        PKIX_DECREF(element);
        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_Duplicate
 * (see comments for PKIX_PL_DuplicateCallback in pkix_pl_system.h)
 */
static PKIX_Error *
pkix_List_Duplicate(
        PKIX_PL_Object *object,
        PKIX_PL_Object **pNewObject,
        void *plContext)
{
        PKIX_List *list = NULL;
        PKIX_List *listDuplicate = NULL;

        PKIX_ENTER(LIST, "pkix_List_Duplicate");
        PKIX_NULLCHECK_TWO(object, pNewObject);

        PKIX_CHECK(pkix_CheckType(object, PKIX_LIST_TYPE, plContext),
                    PKIX_OBJECTNOTLIST);

        list = (PKIX_List *)object;

        if (list->immutable){
                PKIX_CHECK(pkix_duplicateImmutable
                            (object, pNewObject, plContext),
                            PKIX_DUPLICATEIMMUTABLEFAILED);
        } else {

                PKIX_CHECK(pkix_List_Create_Internal
                            (list->isHeader, &listDuplicate, plContext),
                            PKIX_LISTCREATEINTERNALFAILED);

                listDuplicate->length = list->length;

                PKIX_INCREF(list->item);
                listDuplicate->item = list->item;

                if (list->next == NULL){
                        listDuplicate->next = NULL;
                } else {
                        /* Recursively Duplicate list */
                        PKIX_CHECK(pkix_List_Duplicate
                                    ((PKIX_PL_Object *)list->next,
                                    (PKIX_PL_Object **)&listDuplicate->next,
                                    plContext),
                                    PKIX_LISTDUPLICATEFAILED);
                }

                *pNewObject = (PKIX_PL_Object *)listDuplicate;
        }

cleanup:

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(listDuplicate);
        }

        PKIX_RETURN(LIST);
}


/*
 * FUNCTION: pkix_List_GetElement
 * DESCRIPTION:
 *
 *  Copies the "list"'s element at "index" into "element". The input List must
 *  be the header of the List (as opposed to being an element of the List). The
 *  index counts from zero and must be less than the List's length. This
 *  function does NOT increment the reference count of the List element since
 *  the returned element's reference will not be stored by the calling
 *  function.
 *
 * PARAMETERS:
 *  "list"
 *      Address of List (must be header) to get element from. Must be non-NULL.
 *  "index"
 *      Index of list to get element from. Must be less than List's length.
 *  "pElement"
 *      Address where object pointer will be stored. Must be non-NULL.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Conditionally Thread Safe
 *      (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds.
 *  Returns a Fatal Error if the function fails in an unrecoverable way.
 */
static PKIX_Error *
pkix_List_GetElement(
        PKIX_List *list,
        PKIX_UInt32 index,
        PKIX_List **pElement,
        void *plContext)
{
        PKIX_List *iterator = NULL;
        PKIX_UInt32 length;
        PKIX_UInt32 position = 0;

        PKIX_ENTER(LIST, "pkix_List_GetElement");
        PKIX_NULLCHECK_TWO(list, pElement);

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        length = list->length;

        if (index >= length) {
                PKIX_ERROR(PKIX_INDEXOUTOFBOUNDS);
        }

        for (iterator = list; position++ <= index; iterator = iterator->next)
                ;

        (*pElement) = iterator;

cleanup:

        PKIX_RETURN(LIST);
}


/*
 * FUNCTION: pkix_List_RegisterSelf
 * DESCRIPTION:
 *  Registers PKIX_LIST_TYPE and its related functions with systemClasses[]
 * THREAD SAFETY:
 *  Not Thread Safe - for performance and complexity reasons
 *
 *  Since this function is only called by PKIX_PL_Initialize, which should
 *  only be called once, it is acceptable that this function is not
 *  thread-safe.
 */
PKIX_Error *
pkix_List_RegisterSelf(void *plContext)
{
        extern pkix_ClassTable_Entry systemClasses[PKIX_NUMTYPES];
        pkix_ClassTable_Entry entry;

        PKIX_ENTER(LIST, "pkix_List_RegisterSelf");

        entry.description = "List";
        entry.objCounter = 0;
        entry.typeObjectSize = sizeof(PKIX_List);
        entry.destructor = pkix_List_Destroy;
        entry.equalsFunction = pkix_List_Equals;
        entry.hashcodeFunction = pkix_List_Hashcode;
        entry.toStringFunction = pkix_List_ToString;
        entry.comparator = NULL;
        entry.duplicateFunction = pkix_List_Duplicate;

        systemClasses[PKIX_LIST_TYPE] = entry;

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_Contains
 * DESCRIPTION:
 *
 *  Checks a List pointed to by "list", to determine whether it includes
 *  an entry that is equal to the Object pointed to by "object", and stores
 *  the result in "pFound".
 *
 * PARAMETERS:
 *  "list"
 *      List to be searched; may be empty; must be non-NULL
 *  "object"
 *      Object to be checked for; must be non-NULL
 *  "pFound"
 *      Address where the result of the search will be stored. Must
 *      be non-NULL
 *  "plContext"
 *      platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_Contains(
        PKIX_List *list,
        PKIX_PL_Object *object,
        PKIX_Boolean *pFound,
        void *plContext)
{
        PKIX_PL_Object *current = NULL;
        PKIX_UInt32 numEntries = 0;
        PKIX_UInt32 index = 0;
        PKIX_Boolean match = PKIX_FALSE;

        PKIX_ENTER(LIST, "pkix_List_Contains");
        PKIX_NULLCHECK_THREE(list, object, pFound);

        PKIX_CHECK(PKIX_List_GetLength(list, &numEntries, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (index = 0; index < numEntries; index++) {
                PKIX_CHECK(PKIX_List_GetItem
                        (list, index, &current, plContext),
                        PKIX_LISTGETITEMFAILED);

                if (current) {
                        PKIX_CHECK(PKIX_PL_Object_Equals
                                (object, current, &match, plContext),
                                PKIX_OBJECTEQUALSFAILED);

                        PKIX_DECREF(current);
                }

                if (match) {
                        break;
                }
        }

        *pFound = match;

cleanup:

        PKIX_DECREF(current);
        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_Remove
 * DESCRIPTION:
 *
 *  Traverses the List pointed to by "list", to find and delete an entry
 *  that is equal to the Object pointed to by "object". If no such entry
 *  is found the function does not return an error.
 *
 * PARAMETERS:
 *  "list"
 *      List to be searched; may be empty; must be non-NULL
 *  "object"
 *      Object to be checked for and deleted, if found; must be non-NULL
 *  "plContext"
 *      platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a Validate Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_Remove(
        PKIX_List *list,
        PKIX_PL_Object *object,
        void *plContext)
{
        PKIX_PL_Object *current = NULL;
        PKIX_UInt32 numEntries = 0;
        PKIX_UInt32 index = 0;
        PKIX_Boolean match = PKIX_FALSE;

        PKIX_ENTER(LIST, "pkix_List_Remove");
        PKIX_NULLCHECK_TWO(list, object);

        PKIX_CHECK(PKIX_List_GetLength(list, &numEntries, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (index = 0; index < numEntries; index++) {
                PKIX_CHECK(PKIX_List_GetItem
                        (list, index, &current, plContext),
                        PKIX_LISTGETITEMFAILED);

                if (current) {
                        PKIX_CHECK(PKIX_PL_Object_Equals
                                (object, current, &match, plContext),
                                PKIX_OBJECTEQUALSFAILED);

                        PKIX_DECREF(current);
                }

                if (match) {
                        PKIX_CHECK(PKIX_List_DeleteItem
                                (list, index, plContext),
                                PKIX_LISTDELETEITEMFAILED);
                        break;
                }
        }

cleanup:

        PKIX_DECREF(current);
        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_RemoveItems
 * DESCRIPTION:
 *
 *  Traverses the List pointed to by "list", to find and delete an entry
 *  that is equal to the Object in the "deleteList". If no such entry
 *  is found the function does not return an error.
 *
 * PARAMETERS:
 *  "list"
 *      Object in "list" is checked for object in "deleteList" and deleted if
 *      found; may be empty; must be non-NULL
 *  "deleteList"
 *      List of objects to be searched ; may be empty; must be non-NULL
 *  "plContext"
 *      platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a Validate Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_RemoveItems(
        PKIX_List *list,
        PKIX_List *deleteList,
        void *plContext)
{
        PKIX_PL_Object *current = NULL;
        PKIX_UInt32 numEntries = 0;
        PKIX_UInt32 index = 0;

        PKIX_ENTER(LIST, "pkix_List_RemoveItems");
        PKIX_NULLCHECK_TWO(list, deleteList);

        PKIX_CHECK(PKIX_List_GetLength(deleteList, &numEntries, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (index = 0; index < numEntries; index++) {
                PKIX_CHECK(PKIX_List_GetItem
                        (deleteList, index, &current, plContext),
                        PKIX_LISTGETITEMFAILED);

                if (current) {
                        PKIX_CHECK(pkix_List_Remove
                                (list, current, plContext),
                                PKIX_OBJECTEQUALSFAILED);

                        PKIX_DECREF(current);
                }
        }

cleanup:

        PKIX_DECREF(current);
        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_MergeLists
 * DESCRIPTION:
 *
 *  Creates a new list consisting of the items from "firstList", followed by
 *  the items on "secondList", returns the new list at "pMergedList". If
 *  both input lists are NULL or empty, the result is an empty list. If an error
 *  occurs, the result is NULL.
 *
 * PARAMETERS:
 *  "firstList"
 *      Address of list to be merged from. May be NULL or empty.
 *  "secondList"
 *      Address of list to be merged from. May be NULL or empty.
 *  "pMergedList"
 *      Address where returned object is stored.
 *  "plContext"
 *      platform-specific context pointer * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a List Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_MergeLists(
        PKIX_List *firstList,
        PKIX_List *secondList,
        PKIX_List **pMergedList,
        void *plContext)
{
        PKIX_List *list = NULL;
        PKIX_PL_Object *item = NULL;
        PKIX_UInt32 numItems = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(LIST, "pkix_List_MergeLists");
        PKIX_NULLCHECK_ONE(pMergedList);

        *pMergedList = NULL;

        PKIX_CHECK(PKIX_List_Create(&list, plContext),
                    PKIX_LISTCREATEFAILED);

        if (firstList != NULL) {

                PKIX_CHECK(PKIX_List_GetLength(firstList, &numItems, plContext),
                    PKIX_LISTGETLENGTHFAILED);
        }

        for (i = 0; i < numItems; i++) {

                PKIX_CHECK(PKIX_List_GetItem(firstList, i, &item, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_List_AppendItem(list, item, plContext),
                        PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(item);
        }

        numItems = 0;
        if (secondList != NULL) {

                PKIX_CHECK(PKIX_List_GetLength
                        (secondList,
                        &numItems,
                        plContext),
                        PKIX_LISTGETLENGTHFAILED);

        }

        for (i = 0; i < numItems; i++) {

                PKIX_CHECK(PKIX_List_GetItem
                        (secondList, i, &item, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_List_AppendItem
                        (list, item, plContext), PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(item);
        }

        *pMergedList = list;
        list = NULL;

cleanup:
        PKIX_DECREF(list);
        PKIX_DECREF(item);
 
        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_AppendList
 * DESCRIPTION:
 *
 *  Append items on "fromList" to the "toList". Item reference count on
 *  "toList" is not incremented, but items appended from "fromList" are
 *  incremented.
 *
 * PARAMETERS:
 *  "toList"
 *      Address of list to be appended to. Must be non-NULL.
 *  "fromList"
 *      Address of list to be appended from. May be NULL or empty.
 *  "plContext"
 *      platform-specific context pointer
 * THREAD SAFETY:
 *  Thread Safe (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a List Error if the functions fails in a non-fatal way
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_AppendList(
        PKIX_List *toList,
        PKIX_List *fromList,
        void *plContext)
{
        PKIX_PL_Object *item = NULL;
        PKIX_UInt32 numItems = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(LIST, "pkix_List_AppendList");
        PKIX_NULLCHECK_ONE(toList);

        /* if fromList is NULL or is an empty list, no action */

        if (fromList == NULL) {
                goto cleanup;
        }

        PKIX_CHECK(PKIX_List_GetLength(fromList, &numItems, plContext),
                    PKIX_LISTGETLENGTHFAILED);

        if (numItems == 0) {
                goto cleanup;
        }

        for (i = 0; i < numItems; i++) {

                PKIX_CHECK(PKIX_List_GetItem
                        (fromList, i, &item, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_List_AppendItem(toList, item, plContext),
                            PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(item);
        }

cleanup:

        PKIX_DECREF(item);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_AppendUnique
 * DESCRIPTION:
 *
 *  Adds each Object in the List pointed to by "fromList" to the List pointed
 *  to by "toList", if it is not already a member of that List. In other words,
 *  "toList" becomes the union of the two sets.
 *
 * PARAMETERS:
 *  "toList"
 *      Address of a List of Objects to be augmented by "fromList". Must be
 *      non-NULL, but may be empty.
 *  "fromList"
 *      Address of a List of Objects to be added, if not already present, to
 *      "toList". Must be non-NULL, but may be empty.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "toList"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_AppendUnique(
        PKIX_List *toList,
        PKIX_List *fromList,
        void *plContext)
{
        PKIX_Boolean isContained = PKIX_FALSE;
        PKIX_UInt32 listLen = 0;
        PKIX_UInt32 listIx = 0;
        PKIX_PL_Object *object = NULL;

        PKIX_ENTER(BUILD, "pkix_List_AppendUnique");
        PKIX_NULLCHECK_TWO(fromList, toList);

        PKIX_CHECK(PKIX_List_GetLength(fromList, &listLen, plContext),
                PKIX_LISTGETLENGTHFAILED);

        for (listIx = 0; listIx < listLen; listIx++) {

                PKIX_CHECK(PKIX_List_GetItem
                        (fromList, listIx, &object, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(pkix_List_Contains
                        (toList, object, &isContained, plContext),
                        PKIX_LISTCONTAINSFAILED);

                if (isContained == PKIX_FALSE) {
                        PKIX_CHECK(PKIX_List_AppendItem
                                (toList, object, plContext),
                                PKIX_LISTAPPENDITEMFAILED);
                }

                PKIX_DECREF(object);
        }

cleanup:

        PKIX_DECREF(object);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_QuickSort
 * DESCRIPTION:
 *
 *  Sorts List of Objects "fromList" using "comparatorCallback"'s result as
 *  comasrison key and returns the sorted List at "pSortedList". The sorting
 *  algorithm used is quick sort (n*logn).
 *
 * PARAMETERS:
 *  "fromList"
 *      Address of a List of Objects to be sorted. Must be non-NULL, but may be
 *      empty.
 *  "comparatorCallback"
 *      Address of callback function that will compare two Objects on the List.
 *      It should return -1 for less, 0 for equal and 1 for greater. The
 *      callback implementation chooses what in Objects to be compared. Must be
 *      non-NULL.
 *  "pSortedList"
 *      Address of a List of Objects that shall be sorted and returned. Must be
 *      non-NULL, but may be empty.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "toList"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_QuickSort(
        PKIX_List *fromList,
        PKIX_List_SortComparatorCallback comparator,
        PKIX_List **pSortedList,
        void *plContext)
{
        PKIX_List *sortedList = NULL;
        PKIX_List *lessList = NULL;
        PKIX_List *greaterList = NULL;
        PKIX_List *sortedLessList = NULL;
        PKIX_List *sortedGreaterList = NULL;
        PKIX_PL_Object *object = NULL;
        PKIX_PL_Object *cmpObj = NULL;
        PKIX_Int32 cmpResult = 0;
        PKIX_UInt32 size = 0;
        PKIX_UInt32 i;

        PKIX_ENTER(BUILD, "pkix_List_QuickSort");
        PKIX_NULLCHECK_THREE(fromList, comparator, pSortedList);

        PKIX_CHECK(PKIX_List_GetLength(fromList, &size, plContext),
                PKIX_LISTGETLENGTHFAILED);

        PKIX_CHECK(PKIX_List_Create(&lessList, plContext),
                    PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_Create(&greaterList, plContext),
                    PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_GetItem
                (fromList, 0, &object, plContext),
                PKIX_LISTGETITEMFAILED);

        /*
         * Pick the first item on the list as the one to be compared.
         * Separate rest of the itmes into two lists: less-than or greater-
         * than lists. Sort those two lists recursively. Insert sorted
         * less-than list before the picked item and append the greater-
         * than list after the picked item.
         */
        for (i = 1; i < size; i++) {

                PKIX_CHECK(PKIX_List_GetItem
                        (fromList, i, &cmpObj, plContext),
                        PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(comparator(object, cmpObj, &cmpResult, plContext),
                        PKIX_COMPARATORCALLBACKFAILED);

                if (cmpResult >= 0) {
                        PKIX_CHECK(PKIX_List_AppendItem
                                (lessList, cmpObj, plContext),
                                PKIX_LISTAPPENDITEMFAILED);
                } else {
                        PKIX_CHECK(PKIX_List_AppendItem
                                (greaterList, cmpObj, plContext),
                                PKIX_LISTAPPENDITEMFAILED);
                }
                PKIX_DECREF(cmpObj);
        }

        PKIX_CHECK(PKIX_List_Create(&sortedList, plContext),
                    PKIX_LISTCREATEFAILED);

        PKIX_CHECK(PKIX_List_GetLength(lessList, &size, plContext),
                PKIX_LISTGETLENGTHFAILED);

        if (size > 1) {

                PKIX_CHECK(pkix_List_QuickSort
                        (lessList, comparator, &sortedLessList, plContext),
                        PKIX_LISTQUICKSORTFAILED);

                PKIX_CHECK(pkix_List_AppendList
                        (sortedList, sortedLessList, plContext),
                        PKIX_LISTAPPENDLISTFAILED);
        } else {
                PKIX_CHECK(pkix_List_AppendList
                        (sortedList, lessList, plContext),
                        PKIX_LISTAPPENDLISTFAILED);
        }

        PKIX_CHECK(PKIX_List_AppendItem(sortedList, object, plContext),
                PKIX_LISTAPPENDFAILED);

        PKIX_CHECK(PKIX_List_GetLength(greaterList, &size, plContext),
                PKIX_LISTGETLENGTHFAILED);

        if (size > 1) {

                PKIX_CHECK(pkix_List_QuickSort
                        (greaterList, comparator, &sortedGreaterList, plContext),
                        PKIX_LISTQUICKSORTFAILED);

                PKIX_CHECK(pkix_List_AppendList
                        (sortedList, sortedGreaterList, plContext),
                        PKIX_LISTAPPENDLISTFAILED);
        } else {
                PKIX_CHECK(pkix_List_AppendList
                        (sortedList, greaterList, plContext),
                        PKIX_LISTAPPENDLISTFAILED);
        }

        *pSortedList = sortedList;

cleanup:

        PKIX_DECREF(cmpObj);
        PKIX_DECREF(object);
        PKIX_DECREF(sortedGreaterList);
        PKIX_DECREF(sortedLessList);
        PKIX_DECREF(greaterList);
        PKIX_DECREF(lessList);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: pkix_List_BubbleSort
 * DESCRIPTION:
 *
 *  Sorts List of Objects "fromList" using "comparatorCallback"'s result as
 *  comasrison key and returns the sorted List at "pSortedList". The sorting
 *  algorithm used is bubble sort (n*n).
 *
 * PARAMETERS:
 *  "fromList"
 *      Address of a List of Objects to be sorted. Must be non-NULL, but may be
 *      empty.
 *  "comparatorCallback"
 *      Address of callback function that will compare two Objects on the List.
 *      It should return -1 for less, 0 for equal and 1 for greater. The
 *      callback implementation chooses what in Objects to be compared. Must be
 *      non-NULL.
 *  "pSortedList"
 *      Address of a List of Objects that shall be sorted and returned. Must be
 *      non-NULL, but may be empty.
 *  "plContext"
 *      Platform-specific context pointer.
 * THREAD SAFETY:
 *  Not Thread Safe - assumes exclusive access to "toList"
 *  (see Thread Safety Definitions in Programmer's Guide)
 * RETURNS:
 *  Returns NULL if the function succeeds
 *  Returns a Fatal Error if the function fails in an unrecoverable way
 */
PKIX_Error *
pkix_List_BubbleSort(
        PKIX_List *fromList,
        PKIX_List_SortComparatorCallback comparator,
        PKIX_List **pSortedList,
        void *plContext)
{
        PKIX_List *sortedList = NULL;
        PKIX_PL_Object *cmpObj = NULL;
        PKIX_PL_Object *leastObj = NULL;
        PKIX_Int32 cmpResult = 0;
        PKIX_UInt32 size = 0;
        PKIX_UInt32 i, j;

        PKIX_ENTER(BUILD, "pkix_List_BubbleSort");
        PKIX_NULLCHECK_THREE(fromList, comparator, pSortedList);
        
        if (fromList->immutable) {
            PKIX_ERROR(PKIX_CANNOTSORTIMMUTABLELIST);
        }
        PKIX_CHECK(pkix_List_Duplicate
                ((PKIX_PL_Object *) fromList,
                (PKIX_PL_Object **) &sortedList,
                 plContext),
                PKIX_LISTDUPLICATEFAILED);

        PKIX_CHECK(PKIX_List_GetLength(sortedList, &size, plContext),
                PKIX_LISTGETLENGTHFAILED);

        if (size > 1) {

        /*
         * Move from the first of the item on the list, For each iteration,
         * compare and swap the least value to the head of the comparisoning
         * sub-list.
         */
            for (i = 0; i < size - 1; i++) {

                PKIX_CHECK(PKIX_List_GetItem
                        (sortedList, i, &leastObj, plContext),
                        PKIX_LISTGETITEMFAILED);

                for (j = i + 1; j < size; j++) {
                        PKIX_CHECK(PKIX_List_GetItem
                                (sortedList, j, &cmpObj, plContext),
                                PKIX_LISTGETITEMFAILED);
                        PKIX_CHECK(comparator
                                (leastObj, cmpObj, &cmpResult, plContext),
                                PKIX_COMPARATORCALLBACKFAILED);
                        if (cmpResult > 0) {
                                PKIX_CHECK(PKIX_List_SetItem
                                           (sortedList, j, leastObj, plContext),
                                           PKIX_LISTSETITEMFAILED);

                                PKIX_DECREF(leastObj);
                                leastObj = cmpObj;
                                cmpObj = NULL;
                        } else {
                                PKIX_DECREF(cmpObj);
                        }
                }
                PKIX_CHECK(PKIX_List_SetItem
                           (sortedList, i, leastObj, plContext),
                           PKIX_LISTSETITEMFAILED);

                PKIX_DECREF(leastObj);
            }
                
        }

        *pSortedList = sortedList;
        sortedList = NULL;
cleanup:

        PKIX_DECREF(sortedList);
        PKIX_DECREF(leastObj);
        PKIX_DECREF(cmpObj);

        PKIX_RETURN(LIST);
}

/* --Public-List-Functions--------------------------------------------- */

/*
 * FUNCTION: PKIX_List_Create (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_Create(
        PKIX_List **pList,
        void *plContext)
{
        PKIX_List *list = NULL;

        PKIX_ENTER(LIST, "PKIX_List_Create");
        PKIX_NULLCHECK_ONE(pList);

        PKIX_CHECK(pkix_List_Create_Internal(PKIX_TRUE, &list, plContext),
                    PKIX_LISTCREATEINTERNALFAILED);

        *pList = list;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_SetImmutable (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_SetImmutable(
        PKIX_List *list,
        void *plContext)
{
        PKIX_ENTER(LIST, "PKIX_List_SetImmutable");
        PKIX_NULLCHECK_ONE(list);

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        list->immutable = PKIX_TRUE;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_IsImmutable (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_IsImmutable(
        PKIX_List *list,
        PKIX_Boolean *pImmutable,
        void *plContext)
{
        PKIX_ENTER(LIST, "PKIX_List_IsImmutable");
        PKIX_NULLCHECK_TWO(list, pImmutable);

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        *pImmutable = list->immutable;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_GetLength (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_GetLength(
        PKIX_List *list,
        PKIX_UInt32 *pLength,
        void *plContext)
{
        PKIX_ENTER(LIST, "PKIX_List_GetLength");
        PKIX_NULLCHECK_TWO(list, pLength);

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        *pLength = list->length;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_IsEmpty (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_IsEmpty(
        PKIX_List *list,
        PKIX_Boolean *pEmpty,
        void *plContext)
{
        PKIX_UInt32 length;

        PKIX_ENTER(LIST, "PKIX_List_IsEmpty");
        PKIX_NULLCHECK_TWO(list, pEmpty);

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        length = list->length;

        if (length == 0){
                *pEmpty = PKIX_TRUE;
        } else {
                *pEmpty = PKIX_FALSE;
        }

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_AppendItem (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_AppendItem(
        PKIX_List *list,
        PKIX_PL_Object *item,
        void *plContext)
{
        PKIX_List *lastElement = NULL;
        PKIX_List *newElement = NULL;
        PKIX_UInt32 length, i;

        PKIX_ENTER(LIST, "PKIX_List_AppendItem");
        PKIX_NULLCHECK_ONE(list);

        if (list->immutable){
                PKIX_ERROR(PKIX_OPERATIONNOTPERMITTEDONIMMUTABLELIST);
        }

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        length = list->length;

        /* find last element of list and create new element there */

        lastElement = list;
        for (i = 0; i < length; i++){
                lastElement = lastElement->next;
        }

        PKIX_CHECK(pkix_List_Create_Internal
                    (PKIX_FALSE, &newElement, plContext),
                    PKIX_LISTCREATEINTERNALFAILED);

        PKIX_INCREF(item);
        newElement->item = item;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)list, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

        lastElement->next = newElement;
        newElement = NULL;
        list->length += 1;

cleanup:

        PKIX_DECREF(newElement);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_InsertItem (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_InsertItem(
        PKIX_List *list,
        PKIX_UInt32 index,
        PKIX_PL_Object *item,
        void *plContext)
{
        PKIX_List *element = NULL;
        PKIX_List *newElem = NULL;

        PKIX_ENTER(LIST, "PKIX_List_InsertItem");
        PKIX_NULLCHECK_ONE(list);


        if (list->immutable){
                PKIX_ERROR(PKIX_OPERATIONNOTPERMITTEDONIMMUTABLELIST);
        }

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        /* Create a new list object */
        PKIX_CHECK(pkix_List_Create_Internal(PKIX_FALSE, &newElem, plContext),
                    PKIX_LISTCREATEINTERNALFAILED);

        if (list->length) {
            PKIX_CHECK(pkix_List_GetElement(list, index, &element, plContext),
                       PKIX_LISTGETELEMENTFAILED);
            /* Copy the old element's contents into the new element */
            newElem->item = element->item;
            /* Add new item to the list */
            PKIX_INCREF(item);
            element->item = item;
            /* Set the new element's next pointer to the old element's next */
            newElem->next = element->next;
            /* Set the old element's next pointer to the new element */
            element->next = newElem;
            newElem = NULL;
        } else {
            PKIX_INCREF(item);
            newElem->item = item;
            newElem->next = NULL;
            list->next = newElem;
            newElem = NULL;
        }
        list->length++;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)list, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);
cleanup:
        PKIX_DECREF(newElem);

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_GetItem (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_GetItem(
        PKIX_List *list,
        PKIX_UInt32 index,
        PKIX_PL_Object **pItem,
        void *plContext)
{
        PKIX_List *element = NULL;

        PKIX_ENTER(LIST, "PKIX_List_GetItem");
        PKIX_NULLCHECK_TWO(list, pItem);

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        PKIX_CHECK(pkix_List_GetElement(list, index, &element, plContext),
                    PKIX_LISTGETELEMENTFAILED);

        PKIX_INCREF(element->item);
        *pItem = element->item;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_SetItem (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_SetItem(
        PKIX_List *list,
        PKIX_UInt32 index,
        PKIX_PL_Object *item,
        void *plContext)
{
        PKIX_List *element;

        PKIX_ENTER(LIST, "PKIX_List_SetItem");
        PKIX_NULLCHECK_ONE(list);

        if (list->immutable){
                PKIX_ERROR(PKIX_OPERATIONNOTPERMITTEDONIMMUTABLELIST);
        }

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        PKIX_CHECK(pkix_List_GetElement(list, index, &element, plContext),
                    PKIX_LISTGETELEMENTFAILED);

        /* DecRef old contents */
        PKIX_DECREF(element->item);

        /* Set New Contents */
        PKIX_INCREF(item);
        element->item = item;

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)list, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_DeleteItem (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_DeleteItem(
        PKIX_List *list,
        PKIX_UInt32 index,
        void *plContext)
{
        PKIX_List *element = NULL;
        PKIX_List *prevElement = NULL;
        PKIX_List *nextElement = NULL;

        PKIX_ENTER(LIST, "PKIX_List_DeleteItem");
        PKIX_NULLCHECK_ONE(list);

        if (list->immutable){
                PKIX_ERROR(PKIX_OPERATIONNOTPERMITTEDONIMMUTABLELIST);
        }

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        PKIX_CHECK(pkix_List_GetElement(list, index, &element, plContext),
                    PKIX_LISTGETELEMENTFAILED);

        /* DecRef old contents */
        PKIX_DECREF(element->item);

        nextElement = element->next;

        if (nextElement != NULL) {
                /* If the next element exists, splice it out. */

                /* Don't need to change ref counts for targets of next */
                element->item = nextElement->item;
                nextElement->item = NULL;

                /* Don't need to change ref counts for targets of next */
                element->next = nextElement->next;
                nextElement->next = NULL;

                PKIX_DECREF(nextElement);

        } else { /* The element is at the tail of the list */
                if (index != 0) {
                        PKIX_CHECK(pkix_List_GetElement
                                    (list, index-1, &prevElement, plContext),
                                    PKIX_LISTGETELEMENTFAILED);
                } else if (index == 0){ /* prevElement must be header */
                        prevElement = list;
                }
                prevElement->next = NULL;

                /* Delete the element */
                PKIX_DECREF(element);
        }

        PKIX_CHECK(PKIX_PL_Object_InvalidateCache
                    ((PKIX_PL_Object *)list, plContext),
                    PKIX_OBJECTINVALIDATECACHEFAILED);

        list->length = list->length - 1;

cleanup:

        PKIX_RETURN(LIST);
}

/*
 * FUNCTION: PKIX_List_ReverseList (see comments in pkix_util.h)
 */
PKIX_Error *
PKIX_List_ReverseList(
        PKIX_List *list,
        PKIX_List **pReversedList,
        void *plContext)
{
        PKIX_List *reversedList = NULL;
        PKIX_PL_Object *item = NULL;
        PKIX_PL_Object *duplicateItem = NULL;
        PKIX_UInt32 length, i;

        PKIX_ENTER(LIST, "pkix_List_ReverseList");
        PKIX_NULLCHECK_TWO(list, pReversedList);

        if (!list->isHeader){
                PKIX_ERROR(PKIX_INPUTLISTMUSTBEHEADER);
        }

        length = list->length;

        /* Create a new list object */
        PKIX_CHECK(PKIX_List_Create(&reversedList, plContext),
                    PKIX_LISTCREATEINTERNALFAILED);

        /*
         * Starting with the last item and traversing backwards (from
         * the original list), append each item to the reversed list
         */

        for (i = 1; i <= length; i++){
                PKIX_CHECK(PKIX_List_GetItem
                            (list, (length - i), &item, plContext),
                            PKIX_LISTGETITEMFAILED);

                PKIX_CHECK(PKIX_PL_Object_Duplicate
                            (item, &duplicateItem, plContext),
                            PKIX_LISTDUPLICATEFAILED);

                PKIX_CHECK(PKIX_List_AppendItem
                            (reversedList, duplicateItem, plContext),
                            PKIX_LISTAPPENDITEMFAILED);

                PKIX_DECREF(item);
                PKIX_DECREF(duplicateItem);
        }

        *pReversedList = reversedList;

cleanup:

        PKIX_DECREF(item);
        PKIX_DECREF(duplicateItem);

        if (PKIX_ERROR_RECEIVED){
                PKIX_DECREF(reversedList);
        }

        PKIX_RETURN(LIST);
}
