/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "pkcs11ui.h"
#include "pkcs11.h"
#include "pk11func.h"
#include "plstr.h"
#include "secmod.h"
#include "secmodti.h"
#include "minihttp.h"
#include "textgen.h"

/* Utility */
PK11SlotInfo *
SSMPKCS11_FindSlotByID(SECMODModuleID modID,
                       CK_SLOT_ID slotID)
{
    SECMODModule *mod = NULL;
    PRIntn i;

    mod = SECMOD_FindModuleByID(modID);
    if (mod)
    {
        for(i=0;i<mod->slotCount;i++)
        {
            if (mod->slots[i]->slotID == slotID)
                return mod->slots[i];
        }
    }
    return NULL;
}

/*
  --------------------------------------------------------- 
  PKCS11 module processing (list, add, delete)
  --------------------------------------------------------- 
 */

SSMStatus
ssmpkcs11_convert_module(SSMTextGenContext *cx, 
                         PRInt32 modIndex,
                         SECMODModule *mod, 
                         char *fmt)
{
    char *dllName = NULL;
    char *tempStr = NULL;
    char *lib_ch  = NULL;
    CK_INFO modInfo;
    SSMStatus rv = SSM_FAILURE;
    SECStatus srv;
    /* 65? Why 65??? It's what's was used in the original UI. */
    char buf[65];
    char buf2[65];

    srv = PK11_GetModInfo(mod, &modInfo);
    if (srv != SECSuccess) {
        /*
         * The module was not properly loaded at start-up,
         * so just make all the strings blank.
         */
        lib_ch     = "";
        buf[0]     = '\0';
    } else {
        /* we provide the space in (buf), so we don't deallocate lib_ch */
        lib_ch = PK11_MakeString(NULL,buf2,(char *)modInfo.libraryDescription,
                                 sizeof(modInfo.libraryDescription));
        PR_snprintf(buf, sizeof(buf), "%d.%d",
                    modInfo.libraryVersion.major, modInfo.libraryVersion.minor);
    }
    if (mod->dllName)
    {
        char *cursor, *newString;
        int numSlashes = 0, newLen, i, j, oldLen;
        
        dllName = mod->dllName;
        /*
         * Now we need to escape the '\' characters so the string shows
         * up correctly in the UI.
         */
        /* First count them to see if we even need to re-allocate*/
        cursor = dllName;
        while ((cursor = PL_strchr(cursor, '\\')) != NULL) {
            numSlashes++;
            cursor++;
        }
        if (numSlashes > 0) {
            oldLen = PL_strlen(dllName);
            newLen = oldLen + numSlashes + 1;
            newString = SSM_NEW_ARRAY(char, newLen);
            /*
             * If we can't allocate a new buffer, then let's just display
             * the original string.  That's better than not displaying
             * anything.
             */
            if (newString != NULL) {
                for (i=0, j=0; i<oldLen+1; i++,j++){
                    newString[j] = dllName[i];
                    if (newString[j] == '\\'){
                        newString[j+1] = '\\';
                        j++;
                    }
                }
                dllName = newString;
            }
        }
    }
    else
    {
        rv = SSM_GetAndExpandText(cx, "text_pk11_no_dll", &dllName);
        if (rv != SSM_SUCCESS)
            goto loser;
    }
    
    tempStr = PR_smprintf(fmt, modIndex, (long)mod->moduleID, lib_ch,
                          mod->commonName, dllName, buf);
    if (tempStr == NULL) {
        rv = SSM_FAILURE;
        goto loser;
    }
    rv = SSM_ConcatenateUTF8String(&cx->m_result, tempStr);

 loser:
    if (dllName && dllName != mod->dllName)
        PR_Free(dllName);
    PR_FREEIF (tempStr);
    return rv;
}

/*
  PKCS11 module list keyword handler.
  Syntax: {_pk11modules <wrapper_key>}
*/

SSMStatus
SSM_PKCS11ModulesKeywordHandler(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    char *wrapperKey = NULL;
    char *wrapperStr = NULL;
    SECMODModuleList *modList = SECMOD_GetDefaultModuleList();
    SECMODModuleList *modWalk = NULL;
    SECMODListLock *modLock = SECMOD_GetDefaultModuleListLock();
    PRBool gotLock = PR_FALSE; /* indicates whether we should release at end */
    PRInt32 i=0;

    /* Check for parameter validity */
    PR_ASSERT(cx);
    PR_ASSERT(cx->m_request);
    PR_ASSERT(cx->m_params);
    PR_ASSERT(cx->m_result);
    if (!cx || !cx->m_request || !cx->m_params || !cx->m_result)
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto real_loser; /* really bail here */
    }

    if (SSM_Count(cx->m_params) != 1)
    {
        SSM_HTTPReportSpecificError(cx->m_request, "_certList: "
                                    "Incorrect number of parameters "
                                    "(%d supplied, 1 needed).\n",
                                    SSM_Count(cx->m_params));
        goto user_loser;
    }

    /* Convert parameters to something we can use in finding certs. */
    wrapperKey = (char *) SSM_At(cx->m_params, 0);
    PR_ASSERT(wrapperKey);

    /* Get the wrapper text. */
    rv = SSM_GetAndExpandTextKeyedByString(cx, wrapperKey, &wrapperStr);
    if (rv != SSM_SUCCESS)
        goto real_loser; /* error string set by the called function */

    /* Iterate over the PKCS11 modules. Put relevant info from each
       into its own copy of the wrapper text. */
    SECMOD_GetReadLock(modLock);
    gotLock = PR_TRUE;

    modWalk = modList;
    while (modWalk)
    {
        rv = ssmpkcs11_convert_module(cx, i++, modWalk->module, wrapperStr);
        modWalk = modWalk->next;
    }

    goto done;
user_loser:
    /* If we reach this point, something in the input is wrong, but we
       can still send something back to the client to indicate that a
       problem has occurred. */

    /* If we can't do what we're about to do, really bail. */
    if (!cx->m_request || !cx->m_request->errormsg)
        goto real_loser;

    /* Clear the string we were accumulating. */
    SSMTextGen_UTF8StringClear(&cx->m_result);

    /* Use the result string given to us to explain what happened. */
    SSM_ConcatenateUTF8String(&cx->m_result, cx->m_request->errormsg);
    /* Clear the result string, since we're sending this inline */
    SSMTextGen_UTF8StringClear(&cx->m_request->errormsg);

    goto done;
real_loser:
    /* If we reach this point, then we are so screwed that we cannot
       send anything vaguely normal back to the client. Bail. */
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
done:
    PR_FREEIF(wrapperStr);
    if (modLock && gotLock)
        SECMOD_ReleaseReadLock(modLock);

    return rv;
}

/*
  --------------------------------------------------------- 
  PKCS11 slot code
  --------------------------------------------------------- 
 */

enum
{
    SSM_PK11STR_SLOT_LOGGED_IN = (long) 0,
    SSM_PK11STR_SLOT_NOT_LOGGED_IN,
    SSM_PK11STR_SLOT_NO_LOGIN_REQUIRED,
    SSM_PK11STR_SLOT_NOT_PRESENT,
    SSM_PK11STR_SLOT_UNINITIALIZED,
    SSM_PK11STR_SLOT_DISABLED,
    SSM_PK11STR_SLOT_READY,
    SSM_PK11STR_SLOT_STRING_COUNT
};

static char *slotStringKeys[] =
{
    "text_pk11_slot_logged_in",
    "text_pk11_slot_not_logged_in",
    "text_pk11_slot_no_login_required",
    "text_pk11_slot_not_present",
    "text_pk11_slot_uninitialized",
    "text_pk11_slot_disabled",
    "text_pk11_slot_ready"
};

static char **slotStrings = NULL;

void
ssmpkcs11_initialize_slot_labels(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    PRIntn i;

    slotStrings = (char **) PR_Calloc(SSM_PK11STR_SLOT_STRING_COUNT + 1,
                            sizeof(char**));
    PR_ASSERT(slotStrings);

    for(i=0;i<SSM_PK11STR_SLOT_STRING_COUNT;i++)
    {
        rv = SSM_FindUTF8StringInBundles(cx, slotStringKeys[i],
                                         &slotStrings[i]);
        PR_ASSERT(rv == SSM_SUCCESS);
    }
}

SSMStatus
ssmpkcs11_convert_slot(SSMTextGenContext *cx, 
                       PRInt32 slotIndex,
                       PK11SlotInfo *slot,
                       char *fmt,
                       PRBool accumulate) /* accumulate in cx->m_result? */
{
    char *name = NULL;
    char *statusStr = NULL;
    char *serial = NULL;
    char *version = NULL;
    char *tempStr = NULL;
    long status, slotID = 0, moduleID = 0;
    CK_TOKEN_INFO tokenInfo;
    SSMStatus rv = SSM_FAILURE;
    SECStatus srv = SECSuccess;
    /* 65? Why 65??? It's what's was used in the original UI. */
    char buf[65];
    char empty[1] = { '\0' } ;

    if (!slotStrings)
        ssmpkcs11_initialize_slot_labels(cx);

    /* If we have a NULL slot, return blank information. */
    if (!slot)
    {
        name      = empty;
        statusStr = empty;
        serial    = empty;
        version   = empty;
        slotID    = 0;

        goto show_stuff;
    }

    /* Get the slot name. Either the default name or the name of the token
       in the slot will be used. */
    if (PK11_IsPresent(slot))
        name = PK11_GetTokenName(slot);
    else
        name = PK11_GetSlotName(slot);

    /* Report the status of the slot. */
    if (PK11_IsDisabled(slot))
        status = SSM_PK11STR_SLOT_DISABLED;
    else if (!PK11_IsPresent(slot))
        status = SSM_PK11STR_SLOT_NOT_PRESENT;
    else if (PK11_NeedLogin(slot) && PK11_NeedUserInit(slot))
        status = SSM_PK11STR_SLOT_UNINITIALIZED;
    else if (PK11_NeedLogin(slot) && !PK11_IsLoggedIn(slot, NULL))
        status = SSM_PK11STR_SLOT_NOT_LOGGED_IN;
    else if (PK11_NeedLogin(slot))
        status = SSM_PK11STR_SLOT_LOGGED_IN;
    else
        status = SSM_PK11STR_SLOT_READY;

    statusStr = slotStrings[status];

    /* Get the serial number and version. */

    /* This is how the old UI determines if there's a token in, so... */
    if (PK11_IsPresent(slot))
        srv = PK11_GetTokenInfo(slot, &tokenInfo);
    
    if (PK11_IsPresent(slot) && (srv == SECSuccess))
    {
        /* Get serial number and version from the token info. */
        serial = PK11_MakeString(NULL, NULL, (char*)tokenInfo.serialNumber,
                                 sizeof(tokenInfo.serialNumber));
        PR_snprintf(buf, sizeof(buf), "%d.%d",
                    tokenInfo.firmwareVersion.major, 
                    tokenInfo.firmwareVersion.minor);
        version = buf;
    }
    else
    {
        /* Get serial number and version from the slot info. */
        CK_SLOT_INFO slotInfo;
        srv = PK11_GetSlotInfo(slot, &slotInfo);
        if (srv != SECSuccess)
            goto loser;

        serial = empty;
        PR_snprintf(buf, sizeof(buf), "%d.%d",
                    slotInfo.firmwareVersion.major, 
                    slotInfo.firmwareVersion.minor);
        version = buf;
    }

    slotID = (long) (slot->slotID);
    moduleID = (long) (slot->module->moduleID);

 show_stuff:
    tempStr = PR_smprintf(fmt, slotIndex, slotID, moduleID, name,
                          statusStr, serial, version);
    if (accumulate)
    {
        rv = SSM_ConcatenateUTF8String(&cx->m_result, tempStr);
        if (rv != SSM_SUCCESS) {
            goto loser;
        }
    }
    else
        rv = SSM_HTTPSendUTF8String(cx->m_request, tempStr);

 loser:
    /* The data that isn't freed is either because it's a member of the 
     * data in a structure of the PK11 libraries or its a static local 
     * variable.
     */
    if (serial && serial != empty)
        PR_Free(serial);
    PR_FREEIF(tempStr);
    return rv;
}

/*
  PKCS11 slot list keyword handler.
  Syntax: {_pk11slots <moduleID>,<wrapper_key>}
*/

SSMStatus
SSM_PKCS11SlotsKeywordHandler(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    SECMODModule *module = NULL;
    char *wrapperKey = NULL;
    char *moduleIDStr = NULL;
    long moduleID;
    char *wrapperStr = NULL;
    PRInt32 i=0;

    /* Check for parameter validity */
    PR_ASSERT(cx);
    PR_ASSERT(cx->m_request);
    PR_ASSERT(cx->m_params);
    PR_ASSERT(cx->m_result);
    if (!cx || !cx->m_request || !cx->m_params || !cx->m_result)
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto real_loser; /* really bail here */
    }

    if (SSM_Count(cx->m_params) != 2)
    {
        SSM_HTTPReportSpecificError(cx->m_request, "_certList: "
                                    "Incorrect number of parameters "
                                    "(%d supplied, 2 needed).\n",
                                    SSM_Count(cx->m_params));
        goto user_loser;
    }

    /* Convert parameters to something we can use in finding certs. */
    moduleIDStr = (char *) SSM_At(cx->m_params, 0);
    PR_ASSERT(moduleIDStr);
    wrapperKey = (char *) SSM_At(cx->m_params, 1);
    PR_ASSERT(wrapperKey);

    /* Find the module we're looking for based on the module ID. */
    module = SECMOD_FindModuleByID((SECMODModuleID) moduleID);
    if (!module)
        goto user_loser;

    /* Get the wrapper text. */
    rv = SSM_GetAndExpandTextKeyedByString(cx, wrapperKey, &wrapperStr);
    if (rv != SSM_SUCCESS)
        goto real_loser; /* error string set by the called function */

    /* Iterate over the slots from this module. Put relevant info from each
       into its own copy of the wrapper text. */
    for(i=0;i<module->slotCount;i++)
    {
        rv = ssmpkcs11_convert_slot(cx, i, module->slots[i], wrapperStr, 
                                    PR_TRUE);
        if (rv != SSM_SUCCESS)
            goto user_loser;
    }
    goto done;
user_loser:
    /* If we reach this point, something in the input is wrong, but we
       can still send something back to the client to indicate that a
       problem has occurred. */

    /* If we can't do what we're about to do, really bail. */
    if (!cx->m_request || !cx->m_request->errormsg)
        goto real_loser;

    /* Clear the string we were accumulating. */
    SSMTextGen_UTF8StringClear(&cx->m_result);

    /* Use the result string given to us to explain what happened. */
    SSM_ConcatenateUTF8String(&cx->m_result, cx->m_request->errormsg);
    /* Clear the result string, since we're sending this inline */
    SSMTextGen_UTF8StringClear(&cx->m_request->errormsg);

    goto done;
real_loser:
    /* If we reach this point, then we are so screwed that we cannot
       send anything vaguely normal back to the client. Bail. */
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
done:
    PR_FREEIF(wrapperStr);
    if (module)
        SECMOD_DestroyModule(module);
    return rv;
}

SSMStatus
ssm_pkcs11_chuck_property(SSMTextGenContext *cx, char *propName)
{
    char *text = NULL;
    SSMStatus rv;

    rv = SSM_GetAndExpandText(cx, propName, &text);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPSendUTF8String(cx->m_request, text);
    
 loser:
    PR_FREEIF(text);
    SSMTextGen_UTF8StringClear(&cx->m_result);
    return rv;
}

/* PKCS11ShowSlots?module=<moduleID> */
SSMStatus 
SSM_ShowSlotsCommandHandler(HTTPRequest *req)
{
    SSMTextGenContext *cx = NULL;
    char *tmpl = NULL, *type = NULL;
    char *nomod_ch = NULL;
    char *modID_ch = NULL;
    long moduleID;
    SECMODModule *module = NULL;
    PRIntn i;
    SSMStatus rv;

    /* If we have a "no_module" parameter, then there 
       is no module for which to load slots. */
    rv = SSM_HTTPParamValue(req, "no_module", &nomod_ch);
    if (rv == SSM_SUCCESS)
        goto display_stuff;

    rv = SSM_HTTPParamValue(req, "module", &modID_ch);
    if (rv != SSM_SUCCESS)
        goto display_stuff;

    if (modID_ch)
    {
        /* Convert the module ID into a real module ID. */
        PR_sscanf(modID_ch, "%ld", &moduleID);
        
        /* Find the module we're looking for based on the module ID. */
        module = SECMOD_FindModuleByID((SECMODModuleID) moduleID);
        if (!module)
            goto loser;
    }

 display_stuff:
    /* Make a new top-level text gen context to chuck text back. */
    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_GetAndExpandText(cx, "adv_modules_slotlist_type", &type);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPSendOKHeader(req, NULL, type);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Chuck out part 1. */
    rv = ssm_pkcs11_chuck_property(cx, "adv_modules_slotlist_part1");
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Get the template for the JS slot list. */
    rv = SSM_GetAndExpandText(cx, "adv_modules_slotlist_js_template", &tmpl);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Iterate over the slots from this module. Put relevant info from each
       into its own copy of the wrapper text. */
    if (module)
    {
        for(i=0;i<module->slotCount;i++)
        {
            rv = ssmpkcs11_convert_slot(cx, i, module->slots[i], tmpl, 
                                        PR_FALSE);
            if (rv != SSM_SUCCESS)
                goto loser;
        }
    }

    PR_Free(tmpl);
    tmpl = NULL;
    
    /* Chuck out part 2. */
    rv = ssm_pkcs11_chuck_property(cx, "adv_modules_slotlist_part2");
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Get the template for the selectable slot list. */
    rv = SSM_GetAndExpandText(cx, "adv_modules_slotlist_select_template", &tmpl);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Iterate over the slots from this module. Put relevant info from each
       into its own copy of the wrapper text. */
    if (module)
    {
        for(i=0;i<module->slotCount;i++)
        {
            rv = ssmpkcs11_convert_slot(cx, i, module->slots[i], tmpl, 
                                        PR_FALSE);
            if (rv != SSM_SUCCESS)
                goto loser;
        }
    }

    /* Chuck out part 3. */
    rv = ssm_pkcs11_chuck_property(cx, "adv_modules_slotlist_part3");
    req->sentResponse = PR_TRUE;

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    if (cx)
        SSMTextGen_DestroyContext(cx);
    PR_FREEIF(tmpl);
    return rv;
}

SSMStatus
ssm_find_module_from_request(HTTPRequest *req, SECMODModule **mod)
{
    char *modID_ch = NULL;
    PRInt32 moduleID;
    SSMStatus rv;

    rv = SSM_HTTPParamValue(req, "module", &modID_ch);
    if (rv != SSM_SUCCESS)
        goto done;

    if (modID_ch)
    {
        /* Convert the module ID into a real module ID. */
        PR_sscanf(modID_ch, "%ld", &moduleID);
        
        /* Find the module we're looking for based on the module ID. */
        *mod = SECMOD_FindModuleByID((SECMODModuleID) moduleID);
    }
 done:
    if ((!*mod) && (rv == SSM_SUCCESS))
        rv = SSM_FAILURE;
    return rv;
}

PK11SlotInfo *
find_slot_by_ID(SECMODModule *mod, CK_SLOT_ID slotID)
{
    int i;
    PK11SlotInfo *slot;

    for (i=0; i < mod->slotCount; i++) {
        slot = mod->slots[i];
        if (slot->slotID == (CK_SLOT_ID) slotID)
            return PK11_ReferenceSlot(slot);
    }
    return NULL;

}

SSMStatus
ssm_find_slot_from_request(HTTPRequest *req, PK11SlotInfo **slot)
{
    char *slotID_ch = NULL;
    PRInt32 slotID;
    SECMODModule *mod;
    SSMStatus rv = SSM_SUCCESS;

    rv = ssm_find_module_from_request(req, &mod);
    if (rv != SSM_SUCCESS)
        goto done;

    rv = SSM_HTTPParamValue(req, "slot", &slotID_ch);
    if (rv != SSM_SUCCESS)
        goto done;

    if (slotID_ch)
    {
        /* Convert the module ID into a real module ID. */
        PR_sscanf(slotID_ch, "%ld", &slotID);

        /* Find the module we're looking for based on the module ID. */
        *slot = find_slot_by_ID(mod, (CK_SLOT_ID) slotID);
    }
 done:
    if ((!*slot) && (rv == SSM_SUCCESS))
        rv = SSM_FAILURE;
    return rv;
}

SSMStatus
ssmpkcs11_show_slot_info(HTTPRequest *req, PK11SlotInfo *slot)
{
    char *wrapperStr = NULL;
    char *tmpl = NULL;
    char *type = NULL;
    SSMTextGenContext *cx;
    SSMStatus rv;

    /* Make a new top-level text gen context to chuck text back. */
    rv = SSMTextGen_NewTopLevelContext(req, &cx);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_GetAndExpandText(cx, "adv_modules_slot_info_type", &type);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_HTTPSendOKHeader(req, NULL, type);
    if (rv != SSM_SUCCESS)
        goto loser;

    rv = SSM_GetAndExpandText(cx, "adv_modules_slot_info_content", &wrapperStr);
    if (rv != SSM_SUCCESS)
        goto loser; /* error string set by the called function */

    rv = ssmpkcs11_convert_slot(cx, 0, slot, wrapperStr, PR_FALSE);

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    if (cx)
        SSMTextGen_DestroyContext(cx);
    PR_FREEIF(tmpl);
    PR_FREEIF(type);
    PR_FREEIF(wrapperStr);
    return rv;
}

SSMStatus 
SSM_ShowSlotCommandHandler(HTTPRequest *req)
{
    SSMStatus rv;
    PK11SlotInfo *slot;

    /* Find the slot. */
    rv = ssm_find_slot_from_request(req, &slot);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Display the slot info. */
    rv = ssmpkcs11_show_slot_info(req, slot);
    req->sentResponse = PR_TRUE;

 loser:
    if (slot)
        PK11_FreeSlot(slot);
    return rv;
}

SSMStatus 
SSM_LoginSlotCommandHandler(HTTPRequest *req)
{
    SSMStatus rv;
    PK11SlotInfo *slot;

    /* Find the slot. */
    rv = ssm_find_slot_from_request(req, &slot);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Log into the slot. */
    PK11_Authenticate(slot, PR_TRUE, req->ctrlconn);

    /* Display the slot info. */
    rv = ssmpkcs11_show_slot_info(req, slot);
    req->sentResponse = PR_TRUE;

 loser:
    if (slot)
        PK11_FreeSlot(slot);
    return rv;
}

SSMStatus
SSM_LogoutSlotCommandHandler(HTTPRequest *req)
{
    SSMStatus rv;
    PK11SlotInfo *slot;

    /* Find the slot. */
    rv = ssm_find_slot_from_request(req, &slot);
    if (rv != SSM_SUCCESS)
        goto loser;

    /* Log out of the slot. */
    PK11_Logout(slot);

    /* Display the slot info. */
    rv = ssmpkcs11_show_slot_info(req, slot);
    req->sentResponse = PR_TRUE;

 loser:
    if (slot)
        PK11_FreeSlot(slot);
    return rv;
}

SSMStatus 
SSM_LogoutAllSlotsCommandHandler(HTTPRequest *req)
{
    SSMStatus rv;
    PK11SlotInfo *slot;

    /* Find the slot. */
    rv = ssm_find_slot_from_request(req, &slot);
    /* Not relevant if we find the slot here, 
       just remember to display (or not) whatever slot we have */
    if (rv != SSM_SUCCESS)
        slot = NULL;

    /* Log out of all slots. */
    PK11_LogoutAll();

    /* Display the slot info (if any). */
    rv = ssmpkcs11_show_slot_info(req, slot);

    req->sentResponse = PR_TRUE;

    if (slot)
        PK11_FreeSlot(slot);
    return rv;
}

/*
  --------------------------------------------------------- 
  FIPS mode code
  --------------------------------------------------------- 
 */

/*
  FIPS mode keyword handler.
  Syntax: {_fipsmode <true_text>,<false_text>}
  where <true_text> is displayed if FIPS mode is on, <false_text> otherwise.
 */
SSMStatus 
SSM_PKCS11FIPSModeKeywordHandler(SSMTextGenContext *cx)
{
    SSMStatus rv = SSM_SUCCESS;
    char *param = NULL;
    char *tempStr = NULL;

    PR_ASSERT(cx);
    PR_ASSERT(cx->m_params);
    PR_ASSERT(cx->m_result);
    if (!cx || !cx->m_params || !cx->m_result)
    {
        rv = PR_INVALID_ARGUMENT_ERROR;
        goto loser; 
    }
    
    /* Figure out if we're in FIPS mode. */
    if (PK11_IsFIPS())
        param = (char *) SSM_At(cx->m_params, 0);
    else
        param = (char *) SSM_At(cx->m_params, 1);

    /* Display the appropriate string. */
    rv = SSMTextGen_SubstituteString(cx, param, &tempStr);
    if (rv != SSM_SUCCESS)
        goto loser; /* error string set by the called function */

    rv = SSM_ConcatenateUTF8String(&cx->m_result, tempStr);
    if (rv == SSM_SUCCESS)
        goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
 done:
    PR_FREEIF(tempStr);
    return rv;
}


void
SSM_TrimTrailingWhitespace(char *str)
{
    char *end = &(str[strlen(str)]);
    char *start = str;

    do
    {
        end--;
    }
    while ((end >= start) && 
           ((*end == ' ') || (*end == '\0')));
    *(++end) = '\0';
}

/*
  Command handler to set FIPS mode.
  Syntax: setFIPSMode?fips={on|off}&baseRef=<baseRef>&target=<ctrlconn>
 */
SSMStatus
SSM_SetFIPSModeCommandHandler(HTTPRequest *req)
{
    char *fips_ch = NULL, *baseRef_ch = NULL;
    SECStatus srv = SECSuccess;
    PRBool oldFIPS, newFIPS;

    SSMStatus rv = SSM_SUCCESS;
    rv = SSM_HTTPParamValue(req, "fips", &fips_ch);
    if (rv != SSM_SUCCESS)
        goto loser;

    newFIPS = !PL_strncmp(fips_ch, "on", 2);
    oldFIPS = PK11_IsFIPS();

    if (newFIPS != oldFIPS)
    {
        /* 
           Turning FIPS mode on/off requires the exact same operation:
           deleting the built-in PKCS11 module. 
           
           ### mwelch We need these calls to differentiate between 
                      secmod dbs!
        */
        SECMODModule *internal;
        CK_INFO modInfo;

        internal = SECMOD_GetInternalModule();
        if (!internal)
            goto loser;

        srv = PK11_GetModInfo(internal, &modInfo);
        if (srv != SECSuccess)
            goto loser;
        SSM_TrimTrailingWhitespace((char*) modInfo.libraryDescription);

        /* Delete the {FIPS,non-FIPS} internal module, so that 
           it will be replaced by the {non-FIPS,FIPS} counterpart. */
        srv = SECMOD_DeleteInternalModule(internal->commonName);
        if (srv != SECSuccess)
            goto loser;
    }

    /* if there's a baseRef, send it back. otherwise, no content. */
    rv = SSM_HTTPParamValue(req, "baseRef", &baseRef_ch);
    if (rv == SSM_SUCCESS)
    {
        /* send what was requested */
        rv = SSM_HTTPCloseAndSleep(req);
    }

    goto done;
 loser:
    if (rv == SSM_SUCCESS) rv = SSM_FAILURE;
    SSM_HTTPReportSpecificError(req, "SetFIPSModeCommandHandler: Error %d "
                                "attempting to change FIPS mode.",
                                srv != SECSuccess ? srv : rv);
 done:
    return rv;
}

