/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define __STRINGLIB_INTERNAL__

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "cpr_stddef.h"
#include "cpr_locks.h"
#include "string_lib.h"
#include "phone_debug.h"
#include "debug.h"


#define STRING_SIGNATURE (('S'<<8)|'T')
#define STR_TO_STRUCT(str) ((string_block_t *)((str) - (offsetof(string_block_t,data))))
#define STRUCT_TO_STR(sbt) ((const char *) (sbt)->data)

static string_t empty_str;
static int strlib_is_string(string_t str);

/*
 *  Function: strlib_malloc
 *
 *  PARAMETERS:const char* : string which is to be malloc'ed
               int         : length of string or -1
 *
 *  DESCRIPTION:strlib_malloc : creates a new string and returns a const char*
 *  to the new string. Size of String is equal to length specified or actual
 *  length of the string when length is specified as -1(LEN_UNKNOWN)
 *
 *  RETURNS: Pointer to malloc'ed string
 *
 */
string_t
strlib_malloc (const char *str, int length, const char *fname, int line)
{
    string_block_t *temp;
    int size;

    // if specified length is unknown or invalid... then calculate it
    // Length < 0 is not expected, but since length is an int, it could
    // theoritically be negative.  [ This check accounts for that, and
    // avoids a static analysis warning related to same ]
    if ((length == LEN_UNKNOWN) || (length < 0)) {
        length = strlen(str);
    }

    size = sizeof(string_block_t) + length + 1;
    temp = (string_block_t *) cpr_malloc(size);

    if (!temp) {
        CSFLogError("src-common",
            "Error: Strlib_Malloc() Failed. Requested Size = %d\n", size);
        return (string_t) 0;
    }

    temp->refcount  = 1;
    temp->length    = (uint16_t) length;
    temp->signature = STRING_SIGNATURE;
    temp->fname     = fname;
    temp->line      = line;
    /* There used to be memcpy here which will walk off the end of */
    /* str pointer which is a bad thing to do */
    sstrncpy(temp->data, str, length + 1);
    temp->data[length] = '\0';

    return STRUCT_TO_STR(temp);
}


/*
 *  Function: strlib_copy
 *
 *  PARAMETERS:string_t : string whose ref count is to be incremented
 *
 *  DESCRIPTION:just increments the reference count of the string.
 *  It is not like conventional strcpy as far as implementation
 *  is concerned, but effectivly when user calls strlib_copy he gets
 *  private copy which cannot be modified by any other function.
 *  String should be modified only by calling strlib_open
 *  which checks for the refcount and if its more than 1 allocates new string.
 *
 *  RETURNS: string_t: string whose ref count is  incremented
 *
 */
string_t
strlib_copy (string_t str)
{
    string_block_t *temp;

    if (!strlib_is_string(str)) {
        return (NULL);
    }

    temp = STR_TO_STRUCT(str);

    /*
     * Refcount is limited to USHRT_MAX since refcount is
     * of type uint16_t.
     */
    if ((temp->refcount < 0xffff) && (str != empty_str)) {
        temp->refcount++;
    }

    return STRUCT_TO_STR(temp);
}


/*
 *  Function: strlib_update
 *
 *  PARAMETERS:string_t    : destination string
 *             const char* : source string
 *
 *  DESCRIPTION:like strcpy but returns const char* to a string in pool
 *
 *  RETURNS: string_t:Pointer to a new string
 *
 */
string_t
strlib_update (string_t destination, const char *source,
              const char *calling_fname, int line)
{
    const char *fname = "strlib_udpate";
    string_t ret_str;

    /* Bogus destination */
    if (!destination) {
        /* Should never happen, so report it */
        CSFLogError("src-common", "%s: Destination String is invalid: %s:%d",
                fname, calling_fname, line);
        /* bad input, bad output */
        return NULL;
    }

    /* Bogus source */
    if (!source) {
        /* Should never happen, so report it and return something */
        CSFLogError("src-common", "%s: Source String is invalid: %s:%d", fname,
                calling_fname, line);
        strlib_free(destination);
        return strlib_empty();
    }

    if (source[0] == '\0') {
        /* Source is NULL string, use string empty */
        strlib_free(destination);
        return strlib_empty();
    }

    ret_str = strlib_malloc(source, LEN_UNKNOWN, calling_fname, line);

    if (!ret_str) {
        /*
         * If a malloc error occurred, give them back what they had.
         * It's not right, but it's better than nothing.
         */
        ret_str = destination;
    } else {
        strlib_free(destination);
    }

    return (ret_str);
}


/*
 *  Function: strlib_append
 *
 *  PARAMETERS: string_t    : oringinal string
 *              const char* : to be appended string
 *
 *  DESCRIPTION:this function will append a string to the original string,
 *  makes a duplicate of the string and returns it.
 *
 *  RETURNS: Appended string (string_t) or NULL if failure
 *
 */
string_t
strlib_append (string_t str, const char *toappend_str,
              const char *fname, int line)
{
    int curlen;
    char *buf;

    /* Should never happen */
    if (!str) {
        return NULL;
    }

    curlen = strlen(str);
    /* not really an open, rather more like modify */
    buf = strlib_open(str, curlen + strlen(toappend_str) + 1, fname, line);
    /*
     * Validate returned value, but buf could be equal to str (buf == str)
     * so there still may be issues.
     */
    if (buf) {
        strcpy(buf + curlen, toappend_str);
        return buf;
    }
    return NULL;
}


/*
 *  Function: strlib_free
 *
 *  PARAMETERS: string_t : string to which reference is to be removed
 *
 *  DESCRIPTION:It will remove the node containing the string from the linked
 *  list if its refcount is 0 else it will decrement the refcount.
 *
 *  RETURNS: none
 *
 */
void
strlib_free (string_t str)
{
    string_block_t *temp;

    if ((!strlib_is_string(str)) || (str == empty_str)) {
        return;
    }

    temp = STR_TO_STRUCT(str);
    temp->refcount--;
    if (temp->refcount == 0) {
        temp->signature = 0;
        cpr_free(temp);
    }

    return;
}


/*
 *  Function: strlib_open
 *
 *  PARAMETERS: string_t : string to be modified
 *              int      : length of string to be modified
 *
 *  DESCRIPTION:User will call open when he wants to modify the string. If
 *  length of string after modification is going to be = < original string
 *  return to him pointer to original string, else a new string is malloced
 *  for user and a pointer to it is send to him.
 *
 *  RETURNS: char* to modifiable string
 *
 */
char *
strlib_open (string_t str, int length, const char *fname, int line)
{
    char *ret_str;
    string_block_t *temp;

    if (!strlib_is_string(str)) {
        return (NULL);
    }

    temp = STR_TO_STRUCT(str);

    if ((temp->refcount == 1) && (length <= temp->length)) {
        ret_str = (char *) str;
    } else {
        ret_str = (char *) strlib_malloc(str, length, fname, line);
        if (!ret_str) {
            /*
             * If a malloc error occurred, give them back what they had.
             * It's not right, but it's better than nothing.
             */
            ret_str = (char *) str;
        } else {
            strlib_free(str);
        }
    }

    return (ret_str);
}


/*
 *  Function: strlib_close
 *
 *  PARAMETERS:char* : string to be made unmodifiable
 *
 *  DESCRIPTION:Just returns const char* for the string, so that user
 *  does not change it by mistake
 *
 *  RETURNS:string_t: pointer to same string
 *
 */
string_t
strlib_close (char *str)
{
    if (!strlib_is_string(str)) {
        return (NULL);
    }
    return (str);
}


/*
 *  Function: strlib_is_string
 *
 *  PARAMETERS: string_t : string which is to be validated
 *
 *  DESCRIPTION:Checks whether the signature for the provided string is valid
 *
 *  RETURNS: static int
 *
 */
int
strlib_is_string (string_t str)
{
    string_block_t *temp;

    if (str == NULL) {
        CSFLogError("src-common",
          "Strlib Error: strlib_is_tring passed invalid string\n");
        return (0);
    }

    temp = STR_TO_STRUCT(str);

    if (temp->signature == STRING_SIGNATURE) {
        return (1);
    } else {
        CSFLogError("src-common",
          "Strlib Error: strlib_is_tring passed invalid string\n");
        return (0);
    }

}

int
strlib_test_memory_is_string (void *mem)
{
    string_block_t *temp;

    if (!mem) {
        return FALSE;
    }

    temp = (string_block_t *) mem;
    if (temp->signature == STRING_SIGNATURE) {
        return TRUE;
    }
    return FALSE;
}


/*
 *  Function: strlib_empty
 *
 *  PARAMETERS: None
 *
 *  DESCRIPTION:strlib_empty will be called by user when he wants
 *  to initialize his string to '/0' or "". It is not same as initializing
 *  it to NULL. if(str) will evaluate to true if str = strlib_empty().
 *  Correct way to check for empty string will be to do if (str[0] == '\0') or
 *  if (str[0] == "")
 *
 *  RETURNS: Pointer to empty_str
 */
string_t
strlib_empty (void)
{
    string_block_t *temp;
    static boolean empty_str_init = FALSE;

    if (empty_str_init == FALSE) {
        empty_str = strlib_malloc("", LEN_UNKNOWN, __FILE__, __LINE__);
        temp = STR_TO_STRUCT(empty_str);
        temp->refcount = 0xffff;
        empty_str_init = TRUE;
    }
    return (empty_str);
}

void
strlib_init (void)
{
  (void) strlib_empty(); // force it to allocate the empty string buffer
}
