/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*
   privacy.c --- privacy policy functions.
   Created: David Meeker <dfm@netscape.com>, 09-Jun-98.
*/


#include "shist.h"
#include "net.h"
#include "xp.h"
#include "glhist.h"
#include "layout.h"
#include "laylayer.h"
/*  #include "receipt.h" */
#include "prefapi.h"
#include "fe_proto.h"
#include "libevent.h" /* Temporary, for FE_CheckConfirm straw-man. */
#include "proto.h"
#include "htmldlgs.h"
#include "xpgetstr.h" /* for XP_GetString() */

#ifdef DEBUG_dfm
#define D(x) x
#else
#define D(x)
#endif

#define PRVCY_ExternalContext(c) ((c->type == MWContextBrowser) || \
                                  (MAIL_NEWS_TYPE(cp->type)))


PRIVATE int32
prvcy_countEntryBoxes(lo_DocState *top_doc_state,
                   LO_FormElementStruct *ele_struct)
{
  intn form_id;
  lo_FormData *form_list;
  lo_DocLists *doc_lists;
  int32 entry_count;
  LO_Element **elements;
  LO_Element *element;
  LO_ImageStruct *image;
  int32 element_iter;

  if ((top_doc_state == NULL) || (ele_struct == NULL)) return -1;
  if (ele_struct->type == LO_IMAGE)
    {
      image = (LO_ImageStruct *)ele_struct;
      form_id = image->image_attr->form_id;
    } else {
      form_id = ele_struct->form_id;
    }

  doc_lists = lo_GetDocListsById(top_doc_state, ele_struct->layer_id);
  form_list = doc_lists->form_list;
  while(form_list != NULL)
        {
      if (form_list->id == form_id)
        break;
      form_list = form_list->next;
        }
  if (form_list == NULL) return -1;

  elements = (LO_Element **)form_list->form_elements;
  if (elements == NULL) return 0;
  entry_count = 0;
  for(element_iter = 0;
      element_iter < form_list->form_ele_cnt;
      element_iter++)
        {
      element = elements[element_iter];
      if (element == NULL) continue;
      if (element->type != LO_FORM_ELE) continue;
      if (element->lo_form.element_data == NULL) continue;
      if ((element->lo_form.element_data->type == FORM_TYPE_TEXT) &&
          !(element->lo_form.element_data->ele_text.read_only))
        entry_count++;
        }
  return entry_count;
}


PRIVATE int32
prvcy_countAreaBoxes(lo_DocState *top_doc_state,
                   LO_FormElementStruct *ele_struct)
{
  intn form_id;
  lo_FormData *form_list;
  lo_DocLists *doc_lists;
  int32 entry_count;
  LO_Element **elements;
  LO_Element *element;
  LO_ImageStruct *image;
  int32 element_iter;

  if ((top_doc_state == NULL) || (ele_struct == NULL)) return -1;
  if (ele_struct->type == LO_IMAGE)
    {
      image = (LO_ImageStruct *)ele_struct;
      form_id = image->image_attr->form_id;
    } else {
      form_id = ele_struct->form_id;
    }

  doc_lists = lo_GetDocListsById(top_doc_state, ele_struct->layer_id);
  form_list = doc_lists->form_list;
  while(form_list != NULL)
        {
      if (form_list->id == form_id)
        break;
      form_list = form_list->next;
        }
  if (form_list == NULL) return -1;

  elements = (LO_Element **)form_list->form_elements;
  if (elements == NULL) return 0;
  entry_count = 0;
  for(element_iter = 0;
      element_iter < form_list->form_ele_cnt;
      element_iter++)
        {
      element = elements[element_iter];
      if (element == NULL) continue;
      if (element->type != LO_FORM_ELE) continue;
      if (element->lo_form.element_data == NULL) continue;
      if ((element->lo_form.element_data->type == FORM_TYPE_TEXTAREA) &&
          !(element->lo_form.element_data->ele_textarea.read_only))
        entry_count++;
        }
  return entry_count;
}

PRIVATE Bool
prvcy_formIsSignificant(MWContext *ctxt,
                     LO_FormElementStruct *ele_struct)
{
  int32 doc_id;
  lo_TopState *top_state;

  doc_id = XP_DOCID(ctxt);
  top_state = lo_FetchTopState(doc_id);
  if (top_state == NULL) return FALSE;

  return ((prvcy_countEntryBoxes(top_state->doc_state, ele_struct) > 1) ||
         (prvcy_countAreaBoxes(top_state->doc_state, ele_struct) > 0));
}

typedef struct _prvcy_PolicyCacheStruct {
    char * host;
    Bool found;
} prvcy_PolicyCacheStruct;

PRIVATE XP_List * prvcy_policy_list = 0;

PRIVATE void
prvcy_lock_policy_list() {
}

PRIVATE void
prvcy_unlock_policy_list() {
}

PRIVATE Bool
prvcy_inCache(char *host, XP_Bool *found)
{
  XP_List * list_ptr;
  prvcy_PolicyCacheStruct * policy;
  prvcy_lock_policy_list();

  /* search through list looking for host */
  list_ptr = prvcy_policy_list;
  while((policy = (prvcy_PolicyCacheStruct *) XP_ListNextObject(list_ptr))) {
    if(!XP_STRCASECMP(host, policy->host)) {

      /* move to head of list since it is most-recently used */
      XP_ListRemoveObject(prvcy_policy_list, policy);
      XP_ListAddObject (prvcy_policy_list, policy);
      prvcy_unlock_policy_list();

      /* return TRUE since it was in the list */
      *found = policy->found;
      return TRUE;
    }
  }

  /* return false since it was not in the list */
  prvcy_unlock_policy_list();
  return FALSE;
}

#define MAX_CACHE 10
PRIVATE void
prvcy_addToCache(char *host, XP_Bool found)
{
  prvcy_PolicyCacheStruct * policy;

  /* create list if it doesn't already exist */
  if(!prvcy_policy_list) {
    prvcy_policy_list = XP_ListNew();
    if(!prvcy_policy_list) {
      return;
    }
  }

  /* remove least-recently used (last) entry if list is too full */
  if (XP_ListCount(prvcy_policy_list) > MAX_CACHE) {
    policy = XP_ListRemoveEndObject (prvcy_policy_list);
    XP_FREEIF(policy->host);
    XP_FREE(policy);
  }

  /* add to head of list */
  policy = XP_NEW(prvcy_PolicyCacheStruct);
  if (policy) {
    policy->host = XP_STRDUP(host);
    policy->found = found;
    prvcy_lock_policy_list();
    XP_ListAddObject(prvcy_policy_list, policy);
    prvcy_unlock_policy_list();
  }
}

static void
prvcy_checkStandardLocation_finished
  (URL_Struct* pUrl, int status, MWContext* context)
{
  History_entry * entry = SHIST_GetCurrent(&context->hist);
  if(entry && pUrl->server_status == 200) {
    prvcy_addToCache(pUrl->address, TRUE);
    entry->privacy_policy_url = XP_STRDUP(pUrl->address);
  } else {
    prvcy_addToCache(pUrl->address, FALSE);
  }
}

/*
  Check for privacy policy at standard location
 */

extern int PRVCY_POLICY_FILE_NAME;

PUBLIC void
PRVCY_CheckStandardLocation(MWContext * context)
{
  if (context) {
    History_entry * entry = SHIST_GetCurrent(&context->hist);
    URL_Struct *pUrl;
    char * privacyURL = NULL;
    XP_Bool found;
    if (!entry || !entry->address || entry->privacy_policy_url) {
      return;
    }

    /*
     * This routine is called from the HTTP_DONE case of NET_ProcessHTTP.  But
     * that will be repeatedly called as long as we keep issuing an http request
     * in this routine.  So, to prevent looping, we will set privacy_policy_url
     * to -1 to indicate that we already did the http request for 
     * privacy_policy.html.
     */
    entry->privacy_policy_url = (char*)(-1); /* prevents looping */

    /* Create a URL for privacy_policy.html */
    privacyURL = NET_ParseURL(entry->address, GET_PROTOCOL_PART | GET_HOST_PART);
    StrAllocCat(privacyURL, "/");
    StrAllocCat(privacyURL, XP_GetString(PRVCY_POLICY_FILE_NAME));
    if (prvcy_inCache(privacyURL, &found)) {
      if (found) {
	entry->privacy_policy_url = privacyURL;
      } else {
	XP_FREEIF(privacyURL);
      }
      return;
    }
    pUrl = NET_CreateURLStruct(privacyURL, NET_NORMAL_RELOAD);
    XP_FREEIF(privacyURL);
    if (!pUrl) {
      return;
    }

    /* See if file exists for this URL */
    pUrl->method = URL_HEAD_METHOD;
    pUrl->fe_data = entry;
    NET_GetURL(pUrl, FO_CACHE_AND_PRESENT, context, prvcy_checkStandardLocation_finished);
  }
}

/*
  Returns TRUE if the current page has an associated privacy policy,
  FALSE if not.
*/

PUBLIC Bool 
PRVCY_CurrentHasPrivacyPolicy(MWContext * ctxt)
{
  if (ctxt) {
    History_entry * entry = SHIST_GetCurrent(&ctxt->hist);
    if (entry) {
      if (entry->privacy_policy_url && (entry->privacy_policy_url != (char*)(-1)))
        return TRUE;
      return FALSE;
    }
  }
  return FALSE;
}

/*
  Returns the URL of the privacy policy associated with this page,
  or NULL if there isn't one. 
*/
PUBLIC char *
PRVCY_GetCurrentPrivacyPolicyURL(MWContext * ctxt)
{
  History_entry * entry;
  
  if (ctxt) {
    entry = SHIST_GetCurrent(&ctxt->hist);
    if (entry && (entry->privacy_policy_url != (char*)(-1))) {
      return entry->privacy_policy_url;
    }
  }
  return NULL;
}

/* 
  Returns true if this context isn't trustworthy.
*/

PUBLIC Bool
PRVCY_ExternalContextDescendant(MWContext *ctxt)
{
  MWContext *cp = ctxt;


  while (cp != NULL)
	{
      if (!PRVCY_ExternalContext(cp))
          return FALSE;	
	  cp = cp->grid_parent;
	}
  return TRUE;
}

/*
  Returns true if, in the current context, a form submission
  should continue with regards to privacy-policies.
*/
PUBLIC Bool 
PRVCY_PrivacyPolicyConfirmSubmit(MWContext *ctxt, 
                                 LO_FormElementStruct *form_element)
{
  History_entry *entry;
  XP_Bool value = FALSE, savevalue;
  Bool returnvalue;
  
  int ret; 
  static const char *prefname = "privacy.warn_no_policy";
  
  
  if (!ctxt) return TRUE;
  entry = SHIST_GetCurrent(&ctxt->hist);
  if (!entry) return TRUE;
  if (entry->privacy_policy_url && (entry->privacy_policy_url != (char*)(-1)))
    return TRUE;
  if (!PRVCY_ExternalContextDescendant(ctxt))
    {
      D(printf("Privacy Policies: Don't care about this context type.\n"));
      return TRUE;
    }

  D(printf("Privacy Policies: Checking Significance: "));
  if (!prvcy_formIsSignificant(ctxt, form_element)) {
    D(printf("failed.\n"));
    return TRUE;
  } else {
    D(printf("passed.\n"));
  }
 
  /* if we're here, this page deserves a privacy warning. */

  ret = PREF_GetBoolPref(prefname, &value);
  
  if (value)
    {
      savevalue = value;
/*      returnvalue = 
        (int) FE_SecurityDialog(ctxt, SD_NO_PRIVACY_POLICY, valueptr); */
      returnvalue = (int) 
        FE_CheckConfirm(ctxt, 
/*                      "Privacy Alert",                                                         */
                        "There is no privacy policy in effect for this form submission.\n"
                        "This site has not disclosed how the information you are providing\n"
                        "will be used. If you are concerned by this, you may want to cancel\n"
                        "this submission.\n",
                        "Show this alert next time?",
                        "Continue Submission",
                        "Cancel Submission",
/*                      TRUE,                                                                   */
                        &value);
      if (value != savevalue)
        {
          ret = PREF_SetBoolPref(prefname, value);
          ret = PREF_SavePrefFile();
        }
      if (returnvalue < 1) return FALSE;
    }
    return TRUE;
}

/*
  Returns the URL of the privacy tutorial
*/
PUBLIC const char *
PRVCY_TutorialURL()
{
  return "http://people.netscape.com/morse/privacy/index.html";
/*return "http://warp/h/peoplestage/d/homepages/morse/privacy/index.html";*/
}

PRIVATE Bool anonymous = FALSE;

/*
  Toggles the anonymous state
*/
PUBLIC void
PRVCY_ToggleAnonymous() {
    if (anonymous) {
	 NET_UnanonymizeCookies();
#ifdef SingleSignon
     SI_UnanonymizeSignons(); 
#endif
	 /*
	  * Global history changes are not complete yet.  Need to modify
	  * lib/libmisc/glhist.c so it uses a different file when in
	  * anonymous mode.  The change is to intruduce an xpGlobalHistory2
	  * that is used in place of xpGlobalHistory when in anonymous mode
	  */
	 GH_SaveGlobalHistory();
    } else {
	 NET_AnonymizeCookies();
#ifdef SingleSignon
     SI_AnonymizeSignons(); 
#endif
	 GH_SaveGlobalHistory();
	 GH_FreeGlobalHistory();
    }
    anonymous = !anonymous;
}

/*
  Returns the anonymous state
*/
PUBLIC Bool
PRVCY_IsAnonymous() {
    return anonymous;
}

/*
  Display information about the site
*/
extern int XP_EMPTY_STRINGS;
extern int PRVCY_HAS_A_POLICY;
extern int PRVCY_HAS_NO_POLICY;
extern int PRVCY_CANNOT_SET_COOKIES;
extern int PRVCY_CAN_SET_COOKIES;
extern int PRVCY_NEEDS_PERMISSION_TO_SET_COOKIES;
extern int PRVCY_HAS_SET_COOKIES;
extern int PRVCY_HAS_NOT_SET_COOKIES;

#define BUFLEN 5000
#define FLUSH_BUFFER			\
    if (buffer) {			\
	StrAllocCat(buffer2, buffer);	\
        buffer[0] = '\0';               \
	g = 0;				\
    }

PR_STATIC_CALLBACK(PRBool)
prvcy_SiteInfoDialogDone(XPDialogState* state, char** argv, int argc,
	unsigned int button) {
    if (button != XP_DIALOG_OK_BUTTON) {
	/* OK button not pressed, this must be request for cookie viewer */
	NET_DisplayCookieInfoOfSiteAsHTML((MWContext *)(state->arg),
	    ((MWContext *)(state->arg))->hist.cur_doc_ptr->address);
	return	PR_TRUE;
    }
    return PR_FALSE;
}

static const char *pref_cookieBehavior = "network.cookie.cookieBehavior";
static const char *pref_warnAboutCookies = "network.cookie.warnAboutCookies";

PUBLIC void
PRVCY_SiteInfo(MWContext *context) {
    char *buffer = (char*)XP_ALLOC(BUFLEN);
    char *buffer2 = 0;
    char *argument = 0;
    char *argument2 = 0;
    int32 cookieBehavior;
    Bool warn;
    int g = 0;

    static XPDialogInfo dialogInfo = {
	XP_DIALOG_OK_BUTTON,
	prvcy_SiteInfoDialogDone,
	400,
	280
    };

    XPDialogStrings* strings;
    strings = XP_GetDialogStrings(XP_EMPTY_STRINGS);
    if (!strings) {
	return;
    }
    StrAllocCopy(buffer2, "");

    /* create javascript */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"<script>\n"
"  function Policy() {\n"
"    window.open(\"%s\");\n"
"  }\n"
"  function ViewCookies(){\n"
"    parent.clicker(this,window.parent);\n"
"  }\n"
"</script>\n",
	PRVCY_GetCurrentPrivacyPolicyURL(context)
    );
    FLUSH_BUFFER

    /* report privacy policy for site */
    if (PRVCY_CurrentHasPrivacyPolicy(context)) {
	StrAllocCopy (argument, XP_GetString(PRVCY_HAS_A_POLICY));
    } else {
	StrAllocCopy (argument, XP_GetString(PRVCY_HAS_NO_POLICY));
    }

    g += PR_snprintf(buffer+g, BUFLEN-g,
"%s<br><br>\n",
	argument
    );
    FLUSH_BUFFER
    XP_FREEIF(argument);

    /* report cookie-setting ability of site */
    PREF_GetIntPref(pref_cookieBehavior, &cookieBehavior);
    if (cookieBehavior == NET_DontUse) {
	StrAllocCopy (argument, XP_GetString(PRVCY_CANNOT_SET_COOKIES));
    } else {
	PREF_GetBoolPref(pref_warnAboutCookies, &warn);
	if (!warn) {
	    StrAllocCopy (argument, XP_GetString(PRVCY_CAN_SET_COOKIES));
	} else {
#if defined(CookieManagement)
	    switch (NET_CookiePermission(context)) {
		case -1:
		    StrAllocCopy (argument,
			XP_GetString(PRVCY_CANNOT_SET_COOKIES));
		    break;
		case 0:
		    StrAllocCopy (argument,
			XP_GetString(PRVCY_NEEDS_PERMISSION_TO_SET_COOKIES));
		    break;
		case 1:
		    StrAllocCopy (argument,
			XP_GetString(PRVCY_CAN_SET_COOKIES));
		    break;
	    }
#else
	    StrAllocCopy (argument,
		XP_GetString(PRVCY_NEEDS_PERMISSION_TO_SET_COOKIES));
#endif
	}
    }

    g += PR_snprintf(buffer+g, BUFLEN-g,
"%s<br><br>\n",
	argument
    );
    FLUSH_BUFFER
    XP_FREEIF(argument);


#if defined(CookieManagement)
    /* report cookies that site has set */
    if (!context || !(context->hist.cur_doc_ptr) ||
	    !(context->hist.cur_doc_ptr->address)) {
	StrAllocCopy (argument, XP_GetString(PRVCY_HAS_NOT_SET_COOKIES));
    } else if (NET_CookieCount(context->hist.cur_doc_ptr->address) > 0) {
	StrAllocCopy (argument, XP_GetString(PRVCY_HAS_SET_COOKIES));
    } else {
	StrAllocCopy (argument, XP_GetString(PRVCY_HAS_NOT_SET_COOKIES));
    }

    g += PR_snprintf(buffer+g, BUFLEN-g,
"%s<br><br>\n",
	argument
    );
    FLUSH_BUFFER
    XP_FREEIF(argument);
#endif

if (0) {
    /* horizontal line separating what the site can do from what it knows */
    g += PR_snprintf(buffer+g, BUFLEN-g,
"<hr align=center width=98%><br><br>"
    );
    FLUSH_BUFFER

    /* report the referer field */
    if (!context || !(context->hist.cur_doc_ptr) ||
	    !(context->hist.cur_doc_ptr->referer)) {
	StrAllocCopy (argument,
            "This site was not told where you were when you clicked on the link that brought you to this site.<br>\n");
        StrAllocCopy  (argument2, "");
    } else {
	StrAllocCopy (argument,
            "This site was told you were at %s when you clicked on the link that brought you to this site.<br>\n");
	StrAllocCopy  (argument2, context->hist.cur_doc_ptr->referer);
    }

    g += PR_snprintf(buffer+g, BUFLEN-g,
	argument,argument2
    );
    FLUSH_BUFFER
    XP_FREEIF(argument);
    XP_FREEIF(argument2);
}

    /* free buffer since it is no longer needed */
    XP_FREEIF(buffer);

    /* do html dialog */
    if (buffer2) {
	XP_CopyDialogString(strings, 0, buffer2);
	XP_FREE(buffer2);
	buffer2 = NULL;
    }
    XP_MakeHTMLDialog(context, &dialogInfo, 0,
		strings, context, PR_FALSE);

    return;
}
