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
   receipt.c --- functions to deal with transaction receipts/form prefilling.
   Created: David Meeker <dfm@netscape.com>, 09-Jun-98.
 */

#include "xp.h"
#include "net.h"
#include "shist.h"
#include "ntypes.h"
#include "layout.h"
#include "laylayer.h"

#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#include "prinrval.h"           /* for PR_IntervalNow */
#else
#include "private/prpriv.h"     /* for NewNamedMonitor */
#endif


#include "privacy.h"
#include "prefapi.h"
#include "rdf.h"
#include "vocab.h"
#include "receipt.h"



extern void
setContainerp(RDF_Resource, PRBool val);

extern RDF gLocalStore;
extern RDF_NCVocab gNavCenter;
extern RDF_WDVocab gWebData;
extern RDF_CoreVocab gCoreVocab;

#define RDF_SetContainer(x,y) setContainerp(x,y)


#ifdef DEBUG_dfm
#include <stdio.h>
#define D(x) x
#else
#define D(x)
#endif

#ifdef PROFILE
#pragma profile on
#endif

RDF_Resource
rt_RDF_GetHostResource(char* host)
{
  char *prefixed_host;
  RDF_Resource hostResource;
  
  prefixed_host = XP_ALLOC(XP_STRLEN(host) + 
                           XP_STRLEN(RECEIPT_CONTAINER_URL_PREFIX) + 1);
  XP_STRCPY(prefixed_host,RECEIPT_CONTAINER_URL_PREFIX);
  XP_STRCAT(prefixed_host,host);

  D(printf("Creating RDF_Resource: %s\n",prefixed_host));
  
  hostResource = RDF_GetResource(NULL, host, 0);
  if (!hostResource) {
    hostResource = RDF_GetResource(NULL, host, 1);
    RDF_SetContainer(hostResource, TRUE);
    /* setResourceType(hostResource, RECEIPT_RT); */
  }
  
  if (hostResource)
    RDF_Assert(gLocalStore, 
               hostResource, 
               gCoreVocab->RDF_parent,
               gNavCenter->RDF_Receipts,
               RDF_RESOURCE_TYPE);
  return hostResource;
}

RDF_Resource
rt_RDF_GetFormResource(RDF_Resource host, 
#ifdef MOCHA
                       char *name,
#endif
                       char *action,
                       int32 ele_cnt)
{
  return NULL;

}


int32
rt_countEntryBoxes(lo_DocState *top_doc_state, 
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

Bool 
rt_formIsSignificant(MWContext *ctxt, 
                     LO_FormElementStruct *ele_struct)
{
  int32 doc_id;
  lo_TopState *top_state;
  
  doc_id = XP_DOCID(ctxt);
  top_state = lo_FetchTopState(doc_id);
  if (top_state == NULL) return FALSE;
  
  return rt_countEntryBoxes(top_state->doc_state, ele_struct) > 1;
}

void
rt_saveFormElement(MWContext *context, LO_FormElementData *ele)
{
  
#define E(x,y) (ele->ele_##x.y)
#define ES(x,y) (((lo_FormElementOptionData *)ele->ele_select.options)[x].y)

  if (ele == NULL) return;

  switch (ele->type)
	{
    case FORM_TYPE_NONE:
      D(printf("\tignored NONE\n")); break;
    case FORM_TYPE_TEXT:
    case FORM_TYPE_PASSWORD:
    case FORM_TYPE_FILE:
      D(printf("\tgot text-type field.\n\t\tName: %s"
               "\n\t\tCurrent_Text: %s\n\t\tSize: %d\n",
               (char *)E(text,name), 
               (char *)E(text,current_text),
               E(text,size))); break;	
      
    case FORM_TYPE_RADIO:
    case FORM_TYPE_CHECKBOX:
      D(printf("\tgot toggle-type field.\n\t\tName: %s"
               "\n\t\tToggled: %d\n\t\tValue: %s\n", 
               (char *)E(toggle,name),
               E(toggle,toggled),
               (char *)E(toggle,value))); break;
      
    case FORM_TYPE_HIDDEN:
    case FORM_TYPE_SUBMIT:
    case FORM_TYPE_RESET:
    case FORM_TYPE_BUTTON:
    case FORM_TYPE_JOT:
      D(printf("\tignored non-stateful field.\n"));
      break;
    case FORM_TYPE_SELECT_ONE:
    case FORM_TYPE_SELECT_MULT:
      { 
        int iter;
        D(printf("\tgot select-type field.\n\t\tName: %s"
                 "\n\t\tOption-Count: %d\n",
                 (char *)E(select,name),
                 E(select,option_cnt)));
        for(iter = 0; iter < E(select,option_cnt); iter++)
          {
            D(printf("\t\tsubelement.\n\t\t\t"
                     "Text_value: %s\n\t\t\t"
                     "Value: %s\n\t\t\tSelected: %d\n",
                     (char *)ES(iter,text_value),
                     (char *)ES(iter,value),
                     ES(iter,selected)));	
          }
        break; 
      }
    case FORM_TYPE_TEXTAREA:
      D(printf("\tgot textarea-type field.\n\t\tName: %s"
               "\n\t\tCurrent_text: %s\n",
               (char *)E(textarea, name),
               (char *)E(textarea, current_text)));
      break;
    case FORM_TYPE_ISINDEX:
    case FORM_TYPE_IMAGE:
    case FORM_TYPE_KEYGEN:
    case FORM_TYPE_READONLY:
    case FORM_TYPE_OBJECT:
    default:
      D(printf("\nignored non-stateful field.\n"));
	}
}


void 
rt_saveFormElements(MWContext *context, 
                    lo_FormData *form, 
                    RDF_Resource host_resource)
{
  LO_Element **elements;
  LO_Element *element;
  int32 element_iter;
  RDF_Resource form_resource;


  if (form == NULL) return;
  elements = (LO_Element **)form->form_elements;
  if (elements == NULL) return;
  form_resource = rt_RDF_GetFormResource(host_resource,
#ifdef MOCHA
                                         (char *)form->name,
#endif /* MOCHA */
                                         (char *)form->action,
                                         form->form_ele_cnt);

                                         

  for(element_iter = 0; element_iter < form->form_ele_cnt; element_iter++)
	{
      D(printf("iterating over form element %d\n",element_iter));
      element = elements[element_iter];
      if (element == NULL) continue;
      if (element->type != LO_FORM_ELE) continue;
      rt_saveFormElement(context, element->lo_form.element_data);
	}
}

void 
rt_saveFormList (MWContext *context, 
                 lo_FormData *form_list, 
                 RDF_Resource host_resource)
{
  lo_FormData *form = form_list;
  
  while(form != NULL)
	{
      D(printf("iterating over form %d\n",form->id));
#ifdef MOCHA
      D(printf(" -- it is named %s\n",(char *)form->name));
#endif
      D(printf(" -- it has %d elements\n",form->form_ele_cnt));
      rt_saveFormElements(context,form,host_resource);
      form = form->next;
	}
  
  
}

PUBLIC void
RT_SaveDocument (MWContext *context, LO_FormElementStruct *form_element)
{
  int32 layer_id, doc_id;
  lo_LayerDocState *layer_state;
  lo_TopState *top_state;
  lo_DocLists *doc_lists;
  lo_FormData *form_list;
  char *url;
  RDF_Resource host_unit;
  


  if (!PRVCY_ExternalContextDescendant(context))
    {
      D(printf("Transaction Receipts: Don't care about this context.\n"));
      return;
    }

  /*
   * See if the form is significant.
   */
  D(printf("Transaction Receipts: Checking Significance: "));
  if (!rt_formIsSignificant(context,form_element))
  {
    D(printf("failed.\n"));
    return;
  } else {
    D(printf("passed.\n")); 
  }


  /*
   * Get the unique document ID, and retrieve this
   * documents layout state.
   */
  doc_id = XP_DOCID(context);
  top_state = lo_FetchTopState(doc_id);
  
  if (top_state->url == NULL) {
    D(printf("Uh-oh. No URL. Can't make a receipt.\n"));
    return;
  }

  url = NET_ParseURL(top_state->url, GET_PROTOCOL_PART | GET_HOST_PART | 
                                     GET_PATH_PART);

  D(printf("Processing URL: %s\n",url)); 
  
  host_unit = rt_RDF_GetHostResource(url);

  if (!host_unit) {
    D(printf("Unable to get RDF Host Unit!\n"));
    /* XXX -- BUG BUG BUG -- Move me to allxpstr */
    FE_Alert(context,"Running low on memory -- \n"
                     "no receipt will be made\n"
                     "for this transaction.");
  }


/*  D(printf("processing base document\n")); */
/*  rt_saveFormList(context,top_state->doc_lists.form_list); */
  
  for(layer_id = 0; layer_id <= top_state->max_layer_num; layer_id++)
    {
      D(printf("iterating over layer %d\n",layer_id));
      layer_state = top_state->layers[layer_id];
      if (!layer_state) continue;
      XP_ASSERT(layer_state->doc_lists);
      doc_lists = layer_state->doc_lists;
      form_list = doc_lists->form_list;
      rt_saveFormList(context,form_list,host_unit);
    }

  XP_FREE(url);

}
