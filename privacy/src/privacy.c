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
#include "receipt.h"

#ifdef DEBUG_dfm
#define D(x) x
#else
#define D(x)
#endif

#define PRVCY_ExternalContext(c) ((c->type == MWContextBrowser) || \
                                  (MAIL_NEWS_TYPE(cp->type)))

extern Bool 
rt_formIsSignificant(MWContext *ctxt, LO_FormElementStruct *ele_struct);

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
  XP_Bool value, *valueptr, savevalue;
  Bool returnvalue;
  /*
  int ret; 
  char *prefname = "privacy.warn_no_privacy_policy";
  */
  
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
  if (!rt_formIsSignificant(ctxt, form_element)) {
    D(printf("failed.\n"));
    return TRUE;
  } else {
    D(printf("passed.\n"));
  }
 
  /* if we're here, this page deserves a privacy warning. */

  value = TRUE;

  /* 
  ret = PREF_GetBoolPref(prefname, &value);
  */   
  
  if (value)
    {
      savevalue = value;
      valueptr = &value;
      returnvalue = 
        (int) FE_SecurityDialog(ctxt, SD_NO_PRIVACY_POLICY, valueptr);
      if (value != savevalue)
        {
          /*                              
          ret = PREF_SetBoolPref(prefname, value);
          ret = PREF_SavePrefFile();
          */    
        }
      if (!returnvalue) return FALSE;
    }
	return TRUE;
}













