/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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
#include "prmon.h"
#include "nlsutil.h"
#include "serv.h" /* for SSM_DEBUG */
#include "ssmerrs.h"
#include "resource.h"
#include "kgenctxt.h"
#include "textgen.h"
#include "minihttp.h"
#include "certlist.h"
#include "ssldlgs.h"
#include "oldfunc.h"
#include "pkcs11ui.h"
#include "signtextres.h"
#include "prmem.h"
#include "certres.h"
#include "advisor.h"
#include "nlslayer.h"

typedef enum
{
    TEXTGEN_PUNCT_LEFTBRACE = (int) 0,
    TEXTGEN_PUNCT_RIGHTBRACE,
    TEXTGEN_PUNCT_SINGLEQUOTE,
    TEXTGEN_PUNCT_COMMA,
    TEXTGEN_PUNCT_DOLLAR,
    TEXTGEN_PUNCT_SPACE,
    TEXTGEN_PUNCT_PERCENT,
    TEXTGEN_PUNCT_MAX_INDEX
} PunctIndex;

static char *punct_ch = "{}',$ %"; /* make this match the enum above */

typedef struct KeywordHandlerEntry
{
    char *keyword;
    KeywordHandlerFunc func;
} KeywordHandlerEntry;

static SSMCollection *keyword_handlers = NULL;

/* Forward declarations */
SSMStatus SSM_GetAndExpandTextKeyedByString(SSMTextGenContext *cx,
                                            const char *key, 
                                            char **result);
/* password keyword handler */
SSMStatus SSM_ReSetPasswordKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_ShowFollowupKeywordHandler(SSMTextGenContext * cx);
SSMStatus SSM_PasswordPrefKeywordHandler(SSMTextGenContext * cx);

/* cert renewal keyword handler */
SSMStatus SSM_RenewalCertInfoHandler(SSMTextGenContext* cx);

/* Given a string and offset of a left brace, find the matching right brace. */
static int
SSMTextGen_FindRightBrace(char *str)
{
    int i, startOff = 0;
    int result = -1, len;
    char *raw;

    if (!punct_ch[0] || !str || (startOff < 0))
        return -1; /* didn't initialize earlier, or bad params  */

    /* Get the length of the source string */
    len = PL_strlen(str);

    raw = str;
    
    /* Walk along the string until we find either a left or right brace. */
    for(i=startOff+1;(i < len) && (result < 0);i++)
    {
        if (raw[i] == punct_ch[TEXTGEN_PUNCT_LEFTBRACE])
        {
            /* Another left brace. Recurse. 
               Assigning back to i is ok, because we'll increment
               before the next check (and avoid double-counting the
               terminating right brace on which i sits after 
               this call). */
            i += SSMTextGen_FindRightBrace(&str[i]);
        }
        else if (raw[i] == punct_ch[TEXTGEN_PUNCT_RIGHTBRACE])
        {
            /* Found the end. Return now. */
            result = i;
            break;
        }
    }

    return result;
}

static SSMStatus
SSMTextGen_DequotifyString(char *str)
{
    if (str == NULL) {
        return SSM_FAILURE;
    }
    while ((str = PL_strchr(str, '\'')) != NULL) {
        if (str[1] == '\'') {
            memmove(str, &str[1], PL_strlen(&str[1])+1); 
        }
        str++;
    }
    return SSM_SUCCESS;
}

PRBool
SSMTextGen_StringContainsFormatParams(char *str)
{
    while ((str = PL_strchr(str, punct_ch[TEXTGEN_PUNCT_PERCENT])) != NULL) {
        if (isdigit(str[1])) {
            return PR_TRUE;
        }
        str++;
    }
    return PR_FALSE;
}

static PRInt32
SSMTextGen_CountCommas(char *str)
{
    PRInt32 result = 0;
    
    while ((str = PL_strchr(str,punct_ch[TEXTGEN_PUNCT_COMMA])) != NULL) { 
        result++;
        str++;
    }

    return result;
}

/* Show a stack frame. */
void
SSMTextGen_Show(SSMTextGenContext *cx)
{
    char *temp_ch = NULL;

    temp_ch = cx->m_keyword;
    SSM_DEBUG("{%s", temp_ch);
    if (cx->m_params && (SSM_Count(cx->m_params) > 0))
    {
        char *param = (char *) SSM_At(cx->m_params, 0);
        int i = 0;
        while(param)
        {
            temp_ch = param;
            printf("%c%s", ((i==0)? '/':','), temp_ch);
            param = (char *) SSM_At(cx->m_params, ++i);
        }
    }
    printf("}\n");
}

/* Trace back a text gen context. */
void
SSMTextGen_DoTraceback(SSMTextGenContext *cx)
{
    
    if (!cx) 
        return;

    /* Depth first traceback */
    if ((cx->m_caller) && (cx->m_caller != cx))
        SSMTextGen_DoTraceback(cx->m_caller);
    SSMTextGen_Show(cx);
}

void
SSMTextGen_Traceback(char *reason, SSMTextGenContext *cx)
{
    if (reason)
        SSM_DEBUG("ERROR - %s\n", reason);
    SSM_DEBUG("Traceback:\n");
    if (!cx)
        SSM_DEBUG("(None available)\n");
    else
        SSMTextGen_DoTraceback(cx);
    SSM_DEBUG("-- End of traceback --\n");
}

/* Create/destroy a textgen context. */
SSMStatus
SSMTextGen_NewContext(SSMTextGenContext *caller, /* can be NULL */
                      HTTPRequest *req,
                      char *keyword,
                      char **params,
                      SSMTextGenContext **result)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMTextGenContext *cx = (SSMTextGenContext *) PR_CALLOC(sizeof(SSMTextGenContext));
    if (!cx)
        goto loser;

    /* Create the collection within the context. */
    cx->m_params = SSM_NewCollection();
    if (!cx->m_params)
        goto loser;

    cx->m_caller = caller;
    cx->m_request = req;

    if (keyword)
        cx->m_keyword = PL_strdup(keyword);
    else
        cx->m_keyword = PL_strdup("");

    if (cx->m_keyword == NULL)
        goto loser;

    cx->m_result = PL_strdup("");

    if (params)
    {
        char *p;
        PRIntn i;

        for(i=0; params[i] != NULL; i++)
        {
            p = params[i];
            if (p)
                SSM_Enqueue(cx->m_params, SSM_PRIORITY_NORMAL, p);
        }
    }

    goto done;
 loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
    if (cx)
    {
        if (cx->m_params)
        {
            /* Couldn't finish filling out the params. The parameter
               strings are still owned by the caller at this point, so
               clear out the collection before destroying the context. */
            char *tmp;
            SSMStatus trv;

            /* Deallocate the parameters individually, then
               destroy the parameter collection. */
            trv = SSM_Dequeue(cx->m_params, SSM_PRIORITY_NORMAL, 
                              (void **) &tmp, PR_FALSE);
            while ((trv == PR_SUCCESS) && tmp)
            {
                trv = SSM_Dequeue(cx->m_params, SSM_PRIORITY_NORMAL, 
                                  (void **) &tmp, PR_FALSE);
            }
        }
        SSMTextGen_DestroyContext(cx);
        cx = NULL;
    }
 done:
    *result = cx;
    return rv;
}

SSMStatus
SSMTextGen_NewTopLevelContext(HTTPRequest *req,
                              SSMTextGenContext **result)
{
    SSMTextGenContext *cx = NULL;
    SSMStatus rv = SSM_SUCCESS;
    
    rv = SSMTextGen_NewContext(NULL, req, NULL, NULL, &cx);

    *result = cx;
    return rv;
}

static SSMTextGenContext *
SSMTextGen_PushStack(SSMTextGenContext *cx, 
                     char *key,
                     char **params)
{
    SSMTextGenContext *newcx = NULL;
    SSMStatus rv;
    
    rv = SSMTextGen_NewContext(cx, cx->m_request, key, params, &newcx);
    if (rv != SSM_SUCCESS)
        SSMTextGen_Traceback("Couldn't push textgen context stack", cx);

#if 0
    if (newcx)
    {
        SSM_DEBUG("New stack frame: ");
        SSMTextGen_Show(newcx);
    }
#endif
    return newcx;
}

void
SSMTextGen_DestroyContext(SSMTextGenContext *cx)
{
    if (cx->m_params)
    {
        char *tmp;
        SSMStatus rv;

        /* Deallocate the parameters individually, then
           destroy the parameter collection. */
        rv = SSM_Dequeue(cx->m_params, SSM_PRIORITY_NORMAL, 
                         (void **) &tmp, PR_FALSE);
        while ((rv == PR_SUCCESS) && tmp)
        {
            PR_Free(tmp);
            rv = SSM_Dequeue(cx->m_params, SSM_PRIORITY_NORMAL, 
                             (void **) &tmp, PR_FALSE);
        }
        SSM_DestroyCollection(cx->m_params);
    }

    if (cx->m_keyword)
        PR_Free(cx->m_keyword);

    if (cx->m_result)
        PR_Free(cx->m_result);

    PR_Free(cx);
}


SSMControlConnection *
SSMTextGen_GetControlConnection(SSMTextGenContext *cx)
{
    if (cx && cx->m_request)
        return cx->m_request->ctrlconn;
    else
        return NULL;
}

SSMResource *
SSMTextGen_GetTargetObject(SSMTextGenContext *cx)
{
    SSMResource *target = NULL;

    if (cx && cx->m_request && cx->m_request->target) {
        target = cx->m_request->target;
    } else {
        SSMControlConnection *ctrl;
        ctrl = SSMTextGen_GetControlConnection(cx);
        target = &(ctrl->super.super);
    }
    return target;
}

/* Allocate/deallocate an array of UTF8 Strings. */
static void
SSMTextGen_DeleteStringPtrArray(char **array)
{
    PRInt32 i;
    
    if (array)
    {
        for(i=0; array[i] != NULL; i++)
        {
            PR_Free(array[i]);
            array[i] = NULL;
        }
        PR_Free(array);
    }
}

/* Given a comma-delimited UnicodeString, split the first
   string off and put the remainder of the fields into an array. */
static SSMStatus
SSMTextGen_SplitKeywordParams(const char *orig,
                              char **keywdResult,
                              char ***paramResult)
{
    char **params = NULL;
    char *keywd;
    char *space, *cursor, *comma;
    PRInt32 argLen;
    SSMStatus rv = SSM_SUCCESS;
    PRInt32 i;
    PRInt32 numParams;

    /* in case we fail */
    *keywdResult = NULL;
    *paramResult = NULL;

    if (!orig)
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser;
    }

    /* Get the keyword out first. */
    /* If we have parameters, copy them off. */
    space = PL_strchr(orig, punct_ch[TEXTGEN_PUNCT_SPACE]);
    if (space != NULL) {
        char ch;
        ch = space[0];
        space[0] = '\0';
        keywd = PL_strdup(orig);
        space[0] = ch;
    } else {
        int len, i;

        len = PL_strlen(orig);
        keywd = PL_strdup(orig);
        for (i=len; i>=0 && isspace(keywd[i]); i--) {
            keywd = '\0';
        }
    }
    /* Now get the parameters. */
    if (space != NULL) {
        cursor = space+1;
        numParams = SSMTextGen_CountCommas(cursor)+2;
        params = SSM_ZNEW_ARRAY(char*, numParams);
        for (i=0; i<(numParams-1) && cursor != NULL; i++) {
            comma = PL_strchr(cursor, punct_ch[TEXTGEN_PUNCT_COMMA]);
            if (comma != NULL) {
                argLen = comma-cursor;
            } else {
                argLen = PL_strlen(cursor);
            }
            params[i] = SSM_NEW_ARRAY(char, (argLen+1));
            if (params[i] == NULL) {
                goto loser;
            }
            PL_strncpy(params[i], cursor, argLen);
            params[i][argLen] = '\0';
            cursor = (comma == NULL) ? NULL : comma+1;
        }
    }

    goto done;
 loser:
    if (rv != SSM_SUCCESS)
        rv = SSM_FAILURE;

    if (keywd)
    {
        PR_Free(keywd);
        keywd = NULL;
    }

    if (params)
    {
        SSMTextGen_DeleteStringPtrArray(params);
        params = NULL;
    }
 done:
    *keywdResult = keywd;
    *paramResult = params;
    return rv;
}

/* Keyword handler routines */
static SSMStatus
SSM_KeywordHandlerInitialize(void)
{
    keyword_handlers = SSM_NewCollection();
    return SSM_SUCCESS;
}

SSMStatus
SSM_RegisterKeywordHandler(char *keyword,
                           KeywordHandlerFunc func)
{
    SSMStatus rv = SSM_SUCCESS;
    KeywordHandlerEntry *entry = NULL;

    if ((!keyword_handlers) || (!keyword) || (!func))
        goto loser;

    entry = (KeywordHandlerEntry *) PR_CALLOC(sizeof(KeywordHandlerEntry));
    if (!entry)
        goto loser;
    entry->keyword = keyword;
    entry->func = func;

    SSM_Enqueue(keyword_handlers, SSM_PRIORITY_NORMAL, entry);

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
    if (entry)
        PR_Free(entry);
 done:
    return rv;
}

SSMStatus
SSMTextGen_DeregisterKeywordHandler(KeywordHandlerFunc *func)
{
    KeywordHandlerEntry *e, *found = NULL;
    PRIntn i;
    PRIntn numEntries = SSM_Count(keyword_handlers);
    SSMStatus rv = SSM_SUCCESS;

    for(i=0;i<numEntries;i++)
    {
        e = (KeywordHandlerEntry *) SSM_At(keyword_handlers, i);
        if ((e != NULL) && (e->func == (KeywordHandlerFunc) func))
        {
            found = e;
            break;
        }
    }

    if (found)
    {
        rv = SSM_Remove(keyword_handlers, found);
        if (rv == SSM_SUCCESS)
        {
            /* Deallocate (found) since we no longer need it. */
            PR_Free(found);
        }
    }
    else
        rv = SSM_FAILURE;

    return rv;
}

static SSMStatus
SSMTextGen_FindKeywordHandler(char *key, 
                              KeywordHandlerFunc *func)
{
    KeywordHandlerEntry *e, *found = NULL;
    char *key_ch = key;
    PRIntn i;
    PRIntn numEntries = SSM_Count(keyword_handlers);

    *func = NULL; /* in case we fail */

    if (key_ch)
    {
        for(i=0;i<numEntries;i++)
        {
            e = (KeywordHandlerEntry *) SSM_At(keyword_handlers, i);
            if (!PL_strcmp(e->keyword, key_ch))
            {
                found = e;
                break;
            }
        }
    }

    if (found)
        *func = found->func;

    return (*func ? SSM_SUCCESS : SSM_FAILURE);
}

/*
  Given a numbered keyword argument in (arg), return the appropriate
  argument from the textgen context.
 */
static SSMStatus
SSMTextGen_ReplaceArgument(SSMTextGenContext *cx,
                           char *keywd, char **dest)
{
    SSMStatus rv = SSM_SUCCESS;
    char *arg = NULL, *param = NULL, *raw;
    PRInt32 argNum = -1;

    /* Is the first char a $? If not, bail. */
    raw = keywd;
    if ((!raw) || (raw[0] != punct_ch[TEXTGEN_PUNCT_DOLLAR]))
        goto loser;

    /* 
       If we're here, this means that we think we have a numeric parameter. 
       Copy the keyword, lop off the first char, and convert to an arg number.
    */
    arg = keywd+1;
    argNum = SSMTextGen_atoi(arg);
    if (argNum < 0)
        goto loser;

    param = (char *) SSM_At(cx->m_params, argNum-1);
    if (!param)
    {
        SSM_DEBUG("ERROR: Wanted argument %d when only %d args present.\n", 
                  argNum, SSM_Count(cx->m_params));
        SSMTextGen_Traceback(NULL, cx);
        goto loser;
    }

    *dest = PL_strdup(param);
    
    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
    if (*dest)
    {
        PR_Free(*dest);
        *dest = NULL;
    }
 done:
    return rv;
}

/*
    Given a Cartman keyword (presumably found within a string),
    replace it with appropriate content.
*/
static SSMStatus
SSMTextGen_ProcessKeyword(SSMTextGenContext *cx, 
                          char *src, char **dest)
{
    SSMStatus rv = SSM_SUCCESS;
    SSMTextGenContext *newcx = NULL;
    char *keywd = NULL;
    char **params = NULL;
    KeywordHandlerFunc func;

    *dest = NULL; /* in case we fail */
    if (PL_strchr(src, '\n')) {
        /* We've got some text with newlines in it.  Keywords in properties
         * files aren't allowed to have newlines in them.
         */
        goto loser;
    }

    /* Split out into keyword and parameters. */
    rv = SSMTextGen_SplitKeywordParams(src, &keywd, &params);
    if (rv != SSM_SUCCESS)
        goto loser;
/*     SSM_DebugUTF8String("SSMTextGen_ProcessKeyword - orig src", src); */
/*     SSM_DebugUTF8String("SSMTextGen_ProcessKeyword - keywd", keywd); */
/*     if (cx->m_params) */
/*     { */
/*     char buf[256]; */
/*     int i = 0; */
/*        UnicodeStringPtr param = NULL; */
/*         for(param = SSM_At(cx->m_params, 0), i = 0; param != NULL; i++) */
/*         { */
/*             param = SSM_At(cx->m_params, i); */
/*             if (param) */
/*             { */
/*                 PR_snprintf(buf, 256, "SSMTextGen_ProcessKeyword - param[%d]", i+1); */
/*                 SSM_DebugUTF8String(buf, param); */
/*             } */
/*         } */
/*     } */

    /* If this is a numbered parameter, replace it with
       what we have stored in the textgen context. */
    rv = SSMTextGen_ReplaceArgument(cx, keywd, dest);
    if (rv == SSM_SUCCESS)
        goto done;

    /* Push the textgen stack. */
    newcx = SSMTextGen_PushStack(cx, keywd, params);
    if (!newcx)
        goto loser;

    /* SSMTextGen_PushStack puts the args of params into a 
     * collection.  So we can now free the array since the 
     * individual pointers are stored in a collection
     */
    PR_Free(params);
    params = NULL;

    /* Look for a keyword handler. */
    rv = SSMTextGen_FindKeywordHandler(keywd, &func);
    if ((rv == SSM_SUCCESS) && (func))
    {
        /* Run the keyword handler. */
        rv = (*func)(newcx);
        if (rv != SSM_SUCCESS)
        {
            SSMTextGen_Traceback("Keyword handler returned error %d", newcx);
            goto loser;
        }

        *dest = newcx->m_result;
        newcx->m_result = NULL; /* so the memory doesn't get deallocated now */

        goto done;
    }

    /* Treat (keywd) as the name of a string in the properties
       file. Extract the value, expand it, and return it (if there
       is something available). (This pushes the textgen stack.) */
    rv = SSM_GetAndExpandTextKeyedByString(newcx, keywd, dest);
    if (rv == SSM_SUCCESS) 
        goto done;

 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
    if (*dest)
    {
        PR_Free(*dest);
        *dest = NULL;
    }
 done:
    if (keywd)
        PR_Free(keywd);
    if (newcx)
        SSMTextGen_DestroyContext(newcx);
/*     if (params && !newcx) */
/*         SSMTextGen_DeleteStringPtrArray(params); */
    return rv;
}

typedef struct SSMTextGenResultStr {
    char *result;
    int allocSize, curSize;
} SSMTextGenResult;

SSMStatus
ssmtextgen_init_segresult(SSMTextGenResult *result, int origLen)
{
    memset (result, 0, sizeof(SSMTextGenResult));
    result->allocSize = (int)(origLen*1.5);
    result->result    = SSM_NEW_ARRAY(char, result->allocSize+1);
    if (result->result == NULL) {
        return SSM_FAILURE;
    }
    result->result[0] = '\0';
    result->curSize   = 0;
    return SSM_SUCCESS;
}

SSMStatus
ssmtextgen_add_segment(SSMTextGenResult *result, char *segment)
{
    int segLen;

    segLen = PL_strlen(segment);
    if ((result->curSize+segLen) > result->allocSize) {
        char *newString;
        int newLen, defReallocLen, newSegReallocLen;

        defReallocLen = result->allocSize*2;
        newSegReallocLen = segLen + result->curSize;
        newLen = (defReallocLen > newSegReallocLen) ? defReallocLen : 
                                                      newSegReallocLen * 2;
        newString = (char *) PR_Realloc(result->result, newLen);
        if (newString == NULL) {
            return SSM_FAILURE;
        }
        result->result    = newString;
        result->allocSize = newLen;
    }
    memcpy(&result->result[result->curSize], segment, segLen);
    result->curSize += segLen;
    result->result[result->curSize] = '\0';
    return SSM_SUCCESS;
}

/* 
   Perform substitutions for non-numeric parameters in (str). Parameters
   in the text are surrounded by curly braces.
*/
SSMStatus
SSMTextGen_SubstituteString(SSMTextGenContext *cx, 
                            char *str, char **result)
{
    SSMStatus rv = SSM_SUCCESS;
    int len, fragLen, rightBraceIndex;
    char *repl1 = NULL, *repl2 = NULL;
    char *leftBrace, *rightBrace;
    char *tmp, *raw;
    SSMTextGenResult segResult;

    /* in case we fail */
    *result = NULL;

    if ((!str) || (!result))
        return SSM_FAILURE;

    /* Get the length of the source string, and a ptr into it */
    raw = str;
    len = PL_strlen(raw);

    if (ssmtextgen_init_segresult(&segResult, len) != SSM_SUCCESS) {
        goto loser;
    }
    /* Scan the string for the next keyword, if any. */
    while (1)
    {
        /* First look for the left brace */
        leftBrace = PL_strchr(raw, punct_ch[TEXTGEN_PUNCT_LEFTBRACE]);
        if (leftBrace == NULL) {
            if (ssmtextgen_add_segment(&segResult, raw) != 
                SSM_SUCCESS) {
                goto loser;
            }
            break;
        }
        /* Look for the corresponding right brace. */
        rightBraceIndex = SSMTextGen_FindRightBrace(leftBrace);
        if (rightBraceIndex >= len) {
            /* No corresponding right brace, should ignore this one. */
            /* We can stop processing here. */
            if (ssmtextgen_add_segment(&segResult, raw) != 
                SSM_SUCCESS) {
                goto loser;
            }
            break;
        }
        /* Process the keyword. */
        /* Get all of the text between the braces and expand it. */
        rightBrace = &leftBrace[rightBraceIndex];
        fragLen = rightBraceIndex;
        tmp = SSM_NEW_ARRAY(char, fragLen);
        if (tmp == NULL) {
            goto loser;
        }
        memcpy(tmp, leftBrace+1, fragLen-1);
        tmp[fragLen-1] = '\0';
        rv = SSMTextGen_SubstituteString(cx, tmp, &repl1);
        PR_Free(tmp);
        tmp = NULL;
        if (rv != SSM_SUCCESS) {
            goto loser;        
        }
        rv = SSMTextGen_ProcessKeyword(cx, repl1, &repl2);
        if (rv != SSM_SUCCESS) {
            char ch;
            SSMStatus rv1, rv2;
            /* The text between the braces couldn't be replaced, so
             * insert everything starting with raw up to and including
             * the left brace, then insert repl1 which is what the substring
             * was substituted as, then place the closing right bracket.
             */
            ch = leftBrace[1];
            leftBrace[1] = '\0';
            rv1 = ssmtextgen_add_segment(&segResult, raw);
            leftBrace[1] = ch;
            rv2 = ssmtextgen_add_segment(&segResult, repl1);
            if (rv1 != SSM_SUCCESS || rv2 != SSM_SUCCESS ||
                ssmtextgen_add_segment(&segResult, "}") != SSM_SUCCESS) {
                goto loser;
            }
        } else {
            /* We processed a keyword, so take the string before the left
             * brace, make it the next segment, then make the response
             * from SSMTextGen_ProcessKeyword the segment after that.
             */
            SSMStatus rv1, rv2;

            leftBrace[0] = '\0';
            rv1 = ssmtextgen_add_segment(&segResult, raw);
            rv2 = ssmtextgen_add_segment(&segResult, repl2);
            PR_Free(repl2);
            repl2 = NULL;
            leftBrace[0] = punct_ch[TEXTGEN_PUNCT_LEFTBRACE];
            if (rv1 != SSM_SUCCESS || rv2 != SSM_SUCCESS) {
                goto loser;
            }
        }
        if (repl1 != NULL) {
            PR_Free(repl1);
            repl1 = NULL;
        }
        if (repl2 != NULL) {
            PR_Free(repl2);
            repl2 = NULL;
        }
        raw = &rightBrace[1];
    }
    *result = segResult.result;
    rv = SSM_SUCCESS;
    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;

    /* delete/NULL result if we ran into a problem */
    if (*result)
    {
        PR_Free(*result);
        *result = NULL;
    }
 done:
    /* deallocate working strings */
    return rv;
}

/* Given a key and a context, cycle through resource bundles until
   we find a match for the desired key. */
SSMStatus
SSM_FindUTF8StringInBundles(SSMTextGenContext *cx,
                               const char *key, 
                               char **utf8Result)
{
    SSMStatus rv = SSM_FAILURE;
    char *utf8;

    /* Attempt to get the string. */
    utf8 = nlsGetUTF8String(key);
    if (utf8) {
        rv = SSMTextGen_DequotifyString(utf8); /* found it */
        *utf8Result = utf8;
    }
    return rv;
}

/* Get a UnicodeString from the resource database and expand it. */
SSMStatus
SSM_GetAndExpandText(SSMTextGenContext *cx, 
                     const char *key, 
                     char **result)
{
    char *replText = NULL;
    char *origText=NULL;
    SSMStatus rv = SSM_SUCCESS;

    if (!key || !result)
        goto loser;
    PR_FREEIF(*result);
    *result = NULL; /* in case we fail */

    if (cx != NULL && PL_strcmp(cx->m_keyword, key)) {
        PR_Free(cx->m_keyword);
        cx->m_keyword = PL_strdup(key);
    }

    /* Get the text from the database. */
    SSM_DEBUG("Requesting string <%s> from the text database.\n", key);
    /*nrv = NLS_PropertyResourceBundleGetString(nls_bndl, 
                                              key,
                                              origText);*/
    rv = SSM_FindUTF8StringInBundles(cx, key, &origText);
    if (rv != SSM_SUCCESS)
    {
        if (cx != NULL)
            SSM_HTTPReportSpecificError(cx->m_request, 
                                        "SSM_GetAndExpandText: SSM error %d "
                                        "getting property string '%s'.", 
                                        rv, key);
        goto loser;
    }

    /* Expand/replace keywords in the string. */
    rv = SSMTextGen_SubstituteString(cx, origText, &replText);
    /*     SSM_DebugUTF8String("SSM_GetAndExpandText - expanded text",  */
    /*                            replText); */
    if ((rv != SSM_SUCCESS) || (!replText))
        goto loser;
    *result = replText;
    replText = NULL; /* So we don't free it*/
    goto done;
 loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
    /* ### mwelch Need to attempt to return a blank string in case some
       errors happen (such as: we can't find the string/locale not supported).*/
    if (*result) {
        PR_Free(*result);
        *result = NULL;
    }
 done:
    /* Dispose of intermediate strings if applicable. */
    if (origText)
        PR_Free(origText);
    if (replText)
        PR_Free(replText);

    return rv;
}

/* Get a string from the resource database using a Unicode string
   as key. Then, expand the string. */
SSMStatus
SSM_GetAndExpandTextKeyedByString(SSMTextGenContext *cx,
                                  const char *key, 
                                  char **result)
{
    SSMStatus rv = SSM_SUCCESS;
    char *text=NULL;

    *result = NULL; /* in case we fail */

    /* Use the ascii equivalent of (key) as the actual key. */
    if (!key)
        goto loser;

    /* Now, call SSM_GetAndExpandText above using the ASCII key. */
    rv = SSM_GetAndExpandText(cx, key, &text);
    if (rv != SSM_SUCCESS)
        goto loser;

    *result = text;
    text = NULL;

    goto done;
 loser:
    if (rv == SSM_SUCCESS) 
        rv = SSM_FAILURE;
    if (text != NULL) 
        PR_Free(text);
    *result = NULL;
 done:
    return rv;
}

/* Get a numeric parameter from the properties file. */
SSMStatus
SSM_GetNumber(SSMTextGenContext *cx, char *key, PRInt32 *param)
{
    char *text = NULL;
    SSMStatus rv = SSM_SUCCESS;

    *param = 0; /* in case we fail */

    /* Get expanded text from the database. */
    rv = SSM_GetAndExpandText(cx, key, &text);
    if ((rv != SSM_SUCCESS) || (!text))
        goto loser;

    *param = SSMTextGen_atoi(text);

    goto done;
 loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
 done:
    if (text)
        PR_Free(text);
    return rv;
}

/* 
   Top level routine called by non-NLS-using parts of Cartman. 
   Retrieves a string, expands it, then formats it according to the
   _Print method of the target object. 
*/
SSMStatus
SSM_GetUTF8Text(SSMTextGenContext *cx,
                const char *key, 
                char **resultStr)
{
    char *expandedText = NULL, *fmtResult = NULL;
    SSMStatus rv = SSM_SUCCESS;
    SSMResource *targetObj;

    *resultStr = NULL; /* in case we fail */

    /* Get expanded text from the database. */
    rv = SSM_GetAndExpandText(cx, key, &expandedText);
    if ((rv != SSM_SUCCESS) || (!expandedText))
        goto loser;

    targetObj = SSMTextGen_GetTargetObject(cx);
    if (SSMTextGen_StringContainsFormatParams(expandedText))
    {
        if (targetObj != NULL)
            rv = SSM_MessageFormatResource(targetObj, expandedText, 
					   cx->m_request->numParams-2, 
					   &(cx->m_request->paramValues[2]), 
                                           &fmtResult);
        else
        {
            PR_ASSERT(!"No target object for formatting");
            rv = SSM_FAILURE;
        }
    } else {
        fmtResult = expandedText;
    }
    if (rv != SSM_SUCCESS)
        goto loser;

    *resultStr = fmtResult;
             
    if (*resultStr == NULL) {
        goto loser;
    }
    goto done;
 loser:
    if (rv != SSM_SUCCESS) rv = SSM_FAILURE;
    if (*resultStr != NULL) {
        PR_Free(*resultStr);
    }
    *resultStr = NULL;
 done:
    if ((expandedText) && (*resultStr != expandedText))
        PR_Free(expandedText);
    if ((fmtResult) && (*resultStr != fmtResult))
        PR_Free(fmtResult);
    return rv;
}

void
TestNLS(void)
{
    char *ustr;
    SSMTextGenContext *cx;
    SSMStatus rv;

    rv = SSMTextGen_NewTopLevelContext(NULL, &cx);
    SSM_GetUTF8Text(cx, "testnls", &ustr);
    SSM_DebugUTF8String("Expanded testnls", ustr);
    PR_FREEIF(ustr);
    SSM_GetUTF8Text(cx, "top1_content", &ustr);
    SSM_DebugUTF8String("Expanded top1", ustr);
    PR_FREEIF(ustr);
    SSM_GetUTF8Text(cx, "left3-1_content", &ustr);
    SSM_DebugUTF8String("Expanded left3-1", ustr);
    PR_FREEIF(ustr);
    SSMTextGen_DestroyContext(cx);
}

/* 
   Hello World keyword handler example.
   This handler replaces all instances of the keyword "{hello}"
   with "Hello, World!" in the text being processed. Look for
   "Hello World" in minihttp.c for content handler examples.
*/
SSMStatus
SSM_HelloKeywordHandler(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS; /* generic rv */

    cx->m_result = PL_strdup("Hello, World");
    /* All done. */
    return rv;
}

SSMStatus
wrap_test(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    char *p, *pattern, *temp = NULL, *fmt;
    int i;

    pattern = PL_strdup("test(%1$s) ");
    /* Zero out the result. */
    SSMTextGen_UTF8StringClear(&cx->m_result);

    /* Wrap the parameters inside "test()", and concatenate them onto the 
       result. */
    for(i=0;i<SSM_Count(cx->m_params);i++)
    {
        p = (char *) SSM_At(cx->m_params, i);
        temp = PR_smprintf(pattern, p);
        if (temp == NULL) {
            goto loser;
        }
        fmt = PR_smprintf("%s%s", cx->m_result, temp);
        PR_smprintf_free(temp);
        if (fmt == NULL) {
            goto loser;
        }
        PR_Free(cx->m_result);
        cx->m_result = fmt;
    }
    goto done;
 loser:
    if (rv == SSM_SUCCESS)
        rv = SSM_FAILURE;
    SSMTextGen_UTF8StringClear(&cx->m_result);
 done:
    PR_FREEIF(temp);
    return rv;
}

void SSM_InitNLS(char *dataDirectory)
{
    SSMStatus rv = SSM_SUCCESS;

    /* Initialize hash table which contains keyword handlers. */
    rv = SSM_KeywordHandlerInitialize();
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Create keyword handlers */
    SSM_RegisterKeywordHandler("_certlist", SSM_CertListKeywordHandler);
    SSM_RegisterKeywordHandler("_server_cert_info", 
                               SSM_ServerCertKeywordHandler);
    SSM_RegisterKeywordHandler("view_cert_info", 
                               SSM_ViewCertInfoKeywordHandler);
    SSM_RegisterKeywordHandler("tokenList", SSMTokenUI_KeywordHandler);

    SSM_RegisterKeywordHandler("_client_auth_certList", 
			       SSM_ClientAuthCertListKeywordHandler);
    SSM_RegisterKeywordHandler("_get_current_time", 
                               SSM_CurrentTimeKeywordHandler);
    SSM_RegisterKeywordHandler("_server_cert_domain_info",
                               SSM_ServerAuthDomainNameKeywordHandler);
    SSM_RegisterKeywordHandler("_verify_server_cert",
                               SSM_VerifyServerCertKeywordHandler);

    SSM_RegisterKeywordHandler("_fipsmode", SSM_PKCS11FIPSModeKeywordHandler);
    SSM_RegisterKeywordHandler("_pk11modules",
                               SSM_PKCS11ModulesKeywordHandler);
    SSM_RegisterKeywordHandler("_pk11slots",
                               SSM_PKCS11SlotsKeywordHandler);
    SSM_RegisterKeywordHandler("_signtext_certList",
                    SSM_SignTextCertListKeywordHandler);
    SSM_RegisterKeywordHandler("_ca_cert_info", SSM_CACertKeywordHandler);
    SSM_RegisterKeywordHandler("_ca_policy_info", SSM_CAPolicyKeywordHandler);
    SSM_RegisterKeywordHandler("verify_cert", SSM_VerifyCertKeywordHandler);
    SSM_RegisterKeywordHandler("choose_list", SSM_SelectCertKeywordHandler);
    SSM_RegisterKeywordHandler("edit_cert", SSM_EditCertKeywordHandler);
    SSM_RegisterKeywordHandler("sa_selected_item", SSMSecurityAdvisorContext_sa_selected_item);
    SSM_RegisterKeywordHandler("set_or_reset_password", SSM_ReSetPasswordKeywordHandler);
    SSM_RegisterKeywordHandler("show_result", SSM_ShowFollowupKeywordHandler); 
    SSM_RegisterKeywordHandler("get_pref_list", SSMSecurityAdvisorContext_GetPrefListKeywordHandler);
    SSM_RegisterKeywordHandler("_ocsp_options", 
                               SSM_OCSPOptionsKeywordHandler);
    SSM_RegisterKeywordHandler("_default_ocsp_responder",
                               SSM_OCSPDefaultResponderKeywordHandler);
    SSM_RegisterKeywordHandler("password_lifetime_pref", SSM_PasswordPrefKeywordHandler);
    SSM_RegisterKeywordHandler("delete_cert_help", SSM_DeleteCertHelpKeywordHandler);
    SSM_RegisterKeywordHandler("delete_cert_warning", SSM_DeleteCertWarnKeywordHandler);
    SSM_RegisterKeywordHandler("get_new_cert_url", SSM_ObtainNewCertSite);
    SSM_RegisterKeywordHandler("ldap_server_list", SSM_LDAPServerListKeywordHandler);
#if 0
    SSM_RegisterKeywordHandler("_java_principals", SSM_JavaPrincipalsKeywordHandler);
#endif
#if 0
    SSM_RegisterKeywordHandler("testwrap", wrap_test);
    SSM_RegisterKeywordHandler("hello", SSM_HelloKeywordHandler);
#endif
    SSM_RegisterKeywordHandler("free_target", 
                               SSM_FreeTarget);
    SSM_RegisterKeywordHandler("pkcs12_incompatibility_warning",
                               SSM_WarnPKCS12Incompatibility);
    SSM_RegisterKeywordHandler("pass_var",
                               SSM_PassVariable);
    SSM_RegisterKeywordHandler("_ocsp_responder_list",
                               SSM_OCSPResponderList);
    SSM_RegisterKeywordHandler("_renewal_cert_info",
                               SSM_RenewalCertInfoHandler);
    SSM_RegisterKeywordHandler("_emailAddresses",
                               SSM_FillTextWithEmails);
    SSM_RegisterKeywordHandler("_certIssuerWindowName",
                               SSM_MakeUniqueNameForIssuerWindow);
    SSM_RegisterKeywordHandler("_windowOffset",
                               SSM_GetWindowOffset);
    SSM_RegisterKeywordHandler("_crlButton",
                               SSM_DisplayCRLButton);
    SSM_RegisterKeywordHandler("_crlList",
                               SSM_ListCRLs);
    SSM_RegisterKeywordHandler("_smime_tab",
                               SSM_LayoutSMIMETab);
    SSM_RegisterKeywordHandler("_java_js_tab",
                               SSM_LayoutJavaJSTab);
    SSM_RegisterKeywordHandler("_addOthersCerts",
                               SSM_LayoutOthersTab);
#if 0
    TestNLS();
#endif
    return;
 loser:
    SSM_DEBUG("NLS initialization failed. UI will be broken.\n");
    /* Run our little test. */
    /*SSM_TestNLS();*/
}

SSMStatus
SSM_PassVariable(SSMTextGenContext *cx)
{
    SSMStatus rv;
    char *variable, *value;

    variable = (char *) SSM_At(cx->m_params, 0);
    rv = SSM_HTTPParamValue(cx->m_request, variable, &value);
    PR_FREEIF(cx->m_result);
    if (rv == SSM_SUCCESS) {
        cx->m_result = PR_smprintf("&%s=%s", variable, value);
    } else {
        cx->m_result = PL_strdup("");
    }
    return SSM_SUCCESS;
}
