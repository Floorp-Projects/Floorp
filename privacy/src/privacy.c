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



/*
  Returns TRUE if the current page has an associated privacy policy,
  FALSE if not.
*/

PUBLIC Bool 
PRVCY_CurrentHasPrivacyPolicy(MWContext * ctxt)
{
  History_entry * entry;
  
  if (ctxt) {
    entry = SHIST_GetCurrent(&ctxt->hist);
    if (entry) {
      if (entry->privacy_policy_url)
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
    if (entry) {
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
  XP_Bool value = FALSE, *valueptr, savevalue;
  Bool returnvalue;
  
  int ret; 
  static const char *prefname = "privacy.warn_no_policy";
  
  
  if (!ctxt) return TRUE;
  entry = SHIST_GetCurrent(&ctxt->hist);
  if (!entry) return TRUE;
  if (entry->privacy_policy_url) return TRUE;
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
/*                      "Continue Submission",
                        "Cancel Submission",
                        TRUE,                                                                   */
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
}

PRIVATE Bool anonymous = FALSE;

/*
  Toggles the anonymous state
*/
PUBLIC void
PRVCY_ToggleAnonymous() {
    if (anonymous) {
	 NET_UnanonymizeCookies();
    } else {
	 NET_AnonymizeCookies();
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
 * temporary UI until FE implements this function as a single dialog box
 */
PRIVATE
XP_Bool FE_CheckConfirm (
        MWContext *pContext,
        char* pConfirmMessage,
        char* pCheckMessage,
        XP_Bool* pChecked) {

    Bool result = ET_PostMessageBox(pContext, pConfirmMessage, TRUE);
    *pChecked = ET_PostMessageBox (pContext, pCheckMessage, TRUE);
    return result;
}
/* end of temporary UI */

