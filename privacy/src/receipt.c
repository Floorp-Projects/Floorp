
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
   Mothballed: 27-Jul-98
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

#undef DEBUG_dfm
#define TIME_RDF_CALLS


#ifdef DEBUG_dfm
#define TIME_RDF_CALLS
#define TIME_RDF_CALLS_OVERKILL
#endif




#ifdef TIME_RDF_CALLS

#include <sys/time.h>
#include <unistd.h>

#define MICROS_TO_SECOND 1000000
#define PROF_RDF(x) x

static struct timeval tv_rdf_start, tv_rdf_end, tv_rdf_elapsed, tv_main_start, tv_main_end;

#define PRINT_TIME(tv,label) printf("%s %d seconds, %d microseconds.\n", label, tv.tv_sec, tv.tv_usec)

#ifdef TIME_RDF_CALLS_OVERKILL
#define PRINT_TIME_OVERKILL PRINT_TIME
#else
#define PRINT_TIME_OVERKILL
#endif

#define RDF_START { gettimeofday(&tv_rdf_start, NULL); }
#define RDF_END { gettimeofday(&tv_rdf_end, NULL); \
 timersub( &tv_rdf_end, &tv_rdf_start, &tv_rdf_start ); \
 timeradd( &tv_rdf_start, &tv_rdf_elapsed, &tv_rdf_elapsed ); \
 PRINT_TIME_OVERKILL(tv_rdf_start,"This RDF call: "); }
#define PROFILING_START { gettimeofday(&tv_main_start, NULL); }
#define PROFILING_END { gettimeofday(&tv_main_end, NULL); \
 timersub( &tv_main_end, &tv_main_start, &tv_main_start ); \
 PRINT_TIME(tv_main_start,"Time elapsed: "); \
 timersub( &tv_main_start, &tv_rdf_elapsed, &tv_main_end ); \
 PRINT_TIME(tv_main_end,"Transaction code: "); \
 PRINT_TIME(tv_rdf_elapsed,"RDF code: "); } 
#else /* TIME_RDF_CALLS */

#define PROF_RDF(x)
#define RDF_START
#define RDF_END
#define PROFILING_START
#define PROFILING_END

#endif /* TIME_RDF_CALLS */



extern char *
resourceID(RDF_Resource r);

extern void
setContainerp(RDF_Resource, PRBool val);

/* extern RDF gLocalStore; */
extern RDF_NCVocab gNavCenter;
extern RDF_WDVocab gWebData;
extern RDF_CoreVocab gCoreVocab;

static char* gRdfDbStr[] = {"rdf:localStore", NULL};
static RDF gRDFDB; 

#define RDF_SetContainer(x,y) setContainerp(x,y)
#define RDF_GetResourceID(x) resourceID(x)

#define RT_STR_NOTNULL(x) ((x == NULL)?"":x)

#ifdef DEBUG_dfm
#include <stdio.h>
#define D(x) x
#else
#define D(x)
#endif

#ifdef PROFILE
#pragma profile on
#endif


#define STRLEN_SERIAL_NO 11 /* 10 digits, plus one separator char. */


char *
rt_MakeNodeID(RDF_Resource parent, int32 serial, char *name, char *type)
{
  char *nodeID;

  /* Yes, I know I'm ignoring name and type. */

  nodeID = XP_ALLOC( XP_STRLEN(resourceID(parent)) + STRLEN_SERIAL_NO + 1);
  if (!nodeID) return NULL;
  XP_SPRINTF(nodeID,"%s#%d",resourceID(parent),serial);
  return nodeID;
}


RDF_Resource
rt_RDF_MakeResource(RDF_Resource source,
                    char *name,
                    Bool container,
                    RDF_Resource relationship)
{
  RDF_Resource res;

  D(printf("Creating RDF_Resource: %s\n", name));
  
RDF_START
  
  res = RDF_GetResource(NULL, name, 0);

  if (!res) {
    res = RDF_GetResource(NULL, name, 1);
    RDF_SetContainer(res, container);
  } else {
    D(printf("Resource already existed! \n"));
  }
  if (res) {
    RDF_Assert(gRDFDB,
               source,
               relationship,
               res,
               RDF_RESOURCE_TYPE);
  }
RDF_END

  return res;
}


RDF_Resource
rt_RDF_MakeChildResource(RDF_Resource parent, char *name, Bool container)
{
  return rt_RDF_MakeResource(parent, name, container, gCoreVocab->RDF_child);
}


void
rt_RDF_NameResource(RDF_Resource source, char *name)
{
  if (name == NULL) return;
  D(printf("Naming ID(%s) to (%s)\n",RDF_GetResourceID(source),name));
RDF_START
  RDF_Assert(gRDFDB, 
             source, 
             gCoreVocab->RDF_name, 
             name, 
             RDF_STRING_TYPE);
RDF_END
}

RDF_Resource
rt_RDF_getEntryResourceText(RDF_Resource form_unit,
                            int32 serial,
                            char *name,
                            char *value,
                            int32 size)
{
  RDF_Resource textResource;
  char *resName;

  if (!name) 
    {
      D(printf("Name of field was NULL.\n"));
      return NULL;
    }

  resName = rt_MakeNodeID(form_unit,serial,name,"text");

  if (!resName) return NULL;

  textResource = rt_RDF_MakeChildResource(form_unit,
                                          resName,
                                          TRUE);
  XP_FREE(resName);
  
  if (!textResource) return NULL;

  rt_RDF_NameResource(textResource, name);

RDF_START
 
  RDF_Assert(gRDFDB,
             textResource,
             gCoreVocab->RDF_stringEquals,
             RT_STR_NOTNULL(value),
             RDF_STRING_TYPE);

RDF_END 
 
  return textResource;
}

RDF_Resource
rt_RDF_getEntryResourceToggle(RDF_Resource form_unit,
                              int32 serial,
                              char *name,
                              Bool toggled,
                              char *value)
{
  char *resName;
  RDF_Resource toggleResource;

  if (!name) 
    {
      D(printf("Name of field was NULL.\n"));
      return NULL;
    }



  resName = rt_MakeNodeID(form_unit,serial,name,"toggle");

  if (!resName) return NULL;

  toggleResource = rt_RDF_MakeChildResource(form_unit,
                                            resName,
                                            FALSE);
  XP_FREE(resName);

  if (!toggleResource) return NULL;
 
  rt_RDF_NameResource(toggleResource, name);

  /* Assert the toggle state and value off this node */

RDF_START

  RDF_Assert(gRDFDB,
	     toggleResource,
	     gCoreVocab->RDF_stringEquals,
	     RT_STR_NOTNULL(value),
	     RDF_STRING_TYPE);

  RDF_Assert(gRDFDB,
             toggleResource,
             gCoreVocab->RDF_equals,
             /* URGENT -- SHOULD THIS BE A POINTER TO THE INT, 
                OR THE INT ITSELF? */
             &toggled,
             RDF_INT_TYPE);
  
RDF_END

  return toggleResource;

}
 
RDF_Resource
rt_RDF_getEntryResourceSelect(RDF_Resource form_unit,
                              int32 serial,
                              char *name,
                              int32 option_cnt)
{
  char *resName;
  RDF_Resource selectResource;
  
  
  if (!name) 
    {
      D(printf("Name of field was NULL.\n"));
      return NULL;
    }


  resName = rt_MakeNodeID(form_unit,serial,name,"select");

  if (!resName) return NULL;

  selectResource = rt_RDF_MakeChildResource(form_unit,
                                            resName,
                                            TRUE);
  XP_FREE(resName);

  if (!selectResource) return NULL;

  rt_RDF_NameResource(selectResource, name);
 
  /* Assert the option count off this node; children will be added after
     this function terminates. */
  
  /* Not necessary right now.. */

  return selectResource;
}
                  
RDF_Resource
rt_RDF_chainSubelement(RDF_Resource entry_unit,
                       int32 serial,
                       char *text_value,
                       char *value,
                       Bool selected)
{
  RDF_Resource subelementResource;
  char *resName;

  if (!value) 
    {
      D(printf("Value of subelement field was NULL.\n"));
      return NULL;
    }


  resName = rt_MakeNodeID(entry_unit, serial, value, "subelement");

  if (!resName) return NULL;

  subelementResource = rt_RDF_MakeChildResource(entry_unit,
                                                resName,
                                                FALSE);
  XP_FREE(resName);

  if (!subelementResource) return NULL;
  
  rt_RDF_NameResource(subelementResource,value);

RDF_START

  RDF_Assert(gRDFDB,
             subelementResource,
             gCoreVocab->RDF_stringEquals,
             RT_STR_NOTNULL(value),
             RDF_STRING_TYPE);

  /*
  RDF_Assert(gRDFDB,
             subelementResource,
             gCoreVocab->RDF_equals,
             RT_STR_NOTNULL(text_value),
             RDF_STRING_TYPE);
  */

  RDF_Assert(gRDFDB,
             subelementResource,
             gCoreVocab->RDF_equals,
             /* URGENT -- SHOULD THIS BE AN INT* OR INT? */
             &selected,
             RDF_INT_TYPE);

RDF_END

  return subelementResource;
}
           
RDF_Resource
rt_RDF_getEntryResourceArea(RDF_Resource form_unit,
                            int32 serial,
                            char *name,
                            char *text)
{
  char *resName;
  RDF_Resource areaResource;

  if (!name) 
    {
      D(printf("Name of textarea field was NULL.\n"));
      return NULL;
    }

  resName = rt_MakeNodeID(form_unit,serial,name,"area");

  if (!resName) return NULL;

  areaResource = rt_RDF_MakeChildResource(form_unit,
                                          resName,
                                          FALSE);
  XP_FREE(resName);

  if (!areaResource) return NULL;

  rt_RDF_NameResource(areaResource, name);
 
  /* Assert the text value off this node */

RDF_START

  RDF_Assert(gRDFDB,
             areaResource,
             gCoreVocab->RDF_equals,
             RT_STR_NOTNULL(text),
             RDF_STRING_TYPE);

RDF_END
  
  return areaResource;
}




RDF_Resource
rt_RDF_GetHostResource(char* host)
{
  char *prefixed_host;
  RDF_Resource hostResource;
  
  prefixed_host = XP_ALLOC(XP_STRLEN(host) + 
                           XP_STRLEN(RECEIPT_CONTAINER_URL_PREFIX) + 1);
  XP_STRCPY(prefixed_host,RECEIPT_CONTAINER_URL_PREFIX);
  XP_STRCAT(prefixed_host,host);

  hostResource = rt_RDF_MakeChildResource(gNavCenter->RDF_Receipts,
                                          prefixed_host,
                                          TRUE);

  XP_FREE(prefixed_host);
  /* Should I name this resource here? */
  return hostResource;
}

RDF_Resource
rt_RDF_GetReceiptResource(RDF_Resource host,
                          char *url)
{
  char *receipt_name;
  RDF_Resource receiptResource;

  /* This needs to have a timestamp in it, too. */
  receipt_name = XP_ALLOC(XP_STRLEN(resourceID(host)) + XP_STRLEN(url) + 1);
  XP_STRCPY(receipt_name,resourceID(host));
  XP_STRCAT(receipt_name,url);

  receiptResource = rt_RDF_MakeChildResource(host,receipt_name,TRUE);
  /* Name this receipt. */
 
  XP_FREE(receipt_name);

  return receiptResource;
}

RDF_Resource
rt_RDF_GetFormResource(RDF_Resource receipt, 
                       char *name,
                       char *action,
                       int32 ele_cnt)
{
  char *form_name = NULL;
  char *form_url;
  RDF_Resource formResource;

  form_name = name;

  if (form_name == NULL) form_name = "";

  form_url = XP_ALLOC(XP_STRLEN(resourceID(receipt)) + XP_STRLEN(form_name) + 2);
  XP_STRCPY(form_url,resourceID(receipt));
  XP_STRCAT(form_url,"#");
  XP_STRCAT(form_url,form_name);

  formResource = 
    rt_RDF_MakeResource(receipt, 
                        form_url, 
                        TRUE, 
                        gCoreVocab->RDF_content);

  XP_FREE(form_url);

  return formResource;
}


void
rt_saveFormElement(MWContext *context, 
                   LO_FormElementData *ele, 
                   RDF_Resource form_unit,
                   int32 ele_serial)
{
  int32 iter;
  RDF_Resource entry_unit = NULL;
  
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
      rt_RDF_getEntryResourceText(form_unit,
                                  ele_serial,
                                  (char *)E(text,name), 
                                  (char *)E(text,current_text), 
                                  E(text,size));
      /*      D(printf("\tgot text-type field.\n\t\tName: %s"
              "\n\t\tCurrent_Text: %s\n\t\tSize: %d\n",
              (char *)E(text,name), 
              (char *)E(text,current_text),
              E(text,size))); */
      break;	
      
    case FORM_TYPE_RADIO:
    case FORM_TYPE_CHECKBOX:
      rt_RDF_getEntryResourceToggle(form_unit, 
                                    ele_serial,
                                    (char *)E(toggle,name), 
                                    E(toggle,toggled), 
                                    (char *)E(toggle,value));
      /*      D(printf("\tgot toggle-type field.\n\t\tName: %s"
              "\n\t\tToggled: %d\n\t\tValue: %s\n", 
              (char *)E(toggle,name),
              E(toggle,toggled),
              (char *)E(toggle,value))); */ 
      break;
      
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
        entry_unit = rt_RDF_getEntryResourceSelect(form_unit, 
                                                   ele_serial,
                                                   (char *)E(select,name), 
                                                   E(select,option_cnt));
        /*      D(printf("\tgot select-type field.\n\t\tName: %s"
                "\n\t\tOption-Count: %d\n",
                (char *)E(select,name),
                E(select,option_cnt))); */
        for(iter = 0; iter < E(select,option_cnt); iter++)
          {
            rt_RDF_chainSubelement(entry_unit,
                                   iter,
                                   (char *)ES(iter,text_value),
                                   (char *)ES(iter,value),
                                   ES(iter,selected));
                                   
            /*            D(printf("\t\tsubelement.\n\t\t\t"
                          "Text_value: %s\n\t\t\t"
                          "Value: %s\n\t\t\tSelected: %d\n",
                          (char *)ES(iter,text_value),
                          (char *)ES(iter,value),
                          ES(iter,selected))); */	
          }
        break; 
      }
    case FORM_TYPE_TEXTAREA:
      rt_RDF_getEntryResourceArea(form_unit,
                                  ele_serial,
                                  (char *)E(textarea,name),
                                  (char *)E(textarea,current_text));
      /*      D(printf("\tgot textarea-type field.\n\t\tName: %s"
              "\n\t\tCurrent_text: %s\n",
              (char *)E(textarea, name),
              (char *)E(textarea, current_text))); */
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
                    RDF_Resource receipt_unit)
{
  LO_Element **elements;
  LO_Element *element;
  int32 element_iter;
  RDF_Resource form_unit;


  if (form == NULL) return;
  elements = (LO_Element **)form->form_elements;
  if (elements == NULL) return;
  form_unit = rt_RDF_GetFormResource(receipt_unit,
                                     (char *)form->name,
                                     (char *)form->action,
                                     form->form_ele_cnt);

                                         

  for(element_iter = 0; element_iter < form->form_ele_cnt; element_iter++)
	{
      D(printf("iterating over form element %d\n",element_iter));
      element = elements[element_iter];
      if (element == NULL) continue;
      if (element->type != LO_FORM_ELE) continue;
      rt_saveFormElement(context, 
                         element->lo_form.element_data, 
                         form_unit,
                         element_iter);
	}
}

void 
rt_saveFormList (MWContext *context, 
                 lo_FormData *form_list, 
                 RDF_Resource receipt_unit)
{
  lo_FormData *form = form_list;
  
  while(form != NULL)
    {
      D(printf("iterating over form %d\n",form->id));
      D(printf(" -- it is named %s\n",(char *)form->name));
      D(printf(" -- it has %d elements\n",form->form_ele_cnt));
      rt_saveFormElements(context,form,receipt_unit);
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
  char *url, *hostname;
  RDF_Resource host_unit = NULL, receipt_unit = NULL;
  


  if (!PRVCY_ExternalContextDescendant(context))
    {
      D(printf("Transaction Receipts: Don't care about this context.\n"));
      return;
    }

  /*
   * See if the form is significant.
   */
  D(printf("Transaction Receipts: Checking Significance: "));
  if (!prvcy_formIsSignificant(context,form_element))
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

  url = NET_ParseURL(top_state->url, /* GET_PROTOCOL_PART | GET_HOST_PART | */
                                     GET_PATH_PART);

  hostname = NET_ParseURL(top_state->url, GET_HOST_PART);

PROFILING_START

  D(printf("Initializing RDF db.\n"));

RDF_START

  gRDFDB = RDF_GetDB((const char **)gRdfDbStr);

RDF_END
  
  if (!gRDFDB) 
  /* XXX Error Message */
    return;

  D(printf("Processing URL: %s\n",url)); 
  D(printf("Requesting Host Resource for: %s\n",hostname));
  
  if (hostname) 
  	host_unit = rt_RDF_GetHostResource(hostname);


  /* This should also pass in the doc title -- where do I get that?? */
  if (host_unit)
	receipt_unit = rt_RDF_GetReceiptResource(host_unit,RT_STR_NOTNULL(url));

  if (!receipt_unit) {
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
      rt_saveFormList(context,form_list,receipt_unit);
    }

  D(printf("Releasing RDF db.\n"));

RDF_START

  RDF_ReleaseDB(gRDFDB);

RDF_END

PROFILING_END

  gRDFDB = NULL;

  XP_FREE(url);
  XP_FREE(hostname);


}

PUBLIC Bool
RT_GetMakeReceiptToggle(MWContext *m_context)
{
    return FALSE;
}

PUBLIC Bool
RT_GetMakeReceiptEnabled(MWContext *m_context)
{
    return FALSE;
}


PUBLIC Bool 
RT_ToggleMakeReceipt(MWContext *m_context)
{
    return FALSE;
}
