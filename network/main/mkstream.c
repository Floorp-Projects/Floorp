/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
 *  Do stream related stuff like push data down the
 *  stream and determine which converter to use to
 *  set up the stream.
 * Additions/Changes by Judson Valeski 1998
 */
#include "mkutils.h"
#include "mkgeturl.h"
#include "mkstream.h"
#include "mktcp.h"

/* for XP_GetString() */
#include "xpgetstr.h"
extern int XP_ALERT_CANTFIND_CONVERTER;


#ifdef PROFILE
#pragma profile on
#endif

typedef struct _net_ConverterStruct {
     XP_List       * converter_stack;  /* the ordered list of converters;
                                        * elements are net_ConverterElement's
                                        */
     char          * format_in;        /* the input (mime) type that the 
										* function accepts 
										*/
     char          * encoding_in;      /* the input content-encoding that the
										* function accepts, or 0 for `any'.
										*/
     FO_Present_Types format_out;      /* the output type of the function */
	 Bool			bAutomated;			/* this flag currently only used by Window on the
											client side. */
} net_ConverterStruct;

typedef struct _net_ConverterElement {
     NET_Converter * converter;        /* the converter function */
     void          * data_obj;         /* a converter specific data object 
										* passed in at the
                                        * time of registration 
										*/
} net_ConverterElement;

/* MWH - this is an ugly implement to get  get around the following: I need to pass
	a flag to NET_RegisterContentTypeConverter(), but I don't want to change the API.
	If we changed this API please fixed this problem.
*/

static Bool autoFlag = 0;  

static Bool needInit = TRUE;  /* this is the flag to tell us that we need to initialize
								net_converter_list, and  net_decoder_list. */

PRIVATE XP_List * net_converter_list[MAX_FORMATS_OUT];  /* a list of converters */
PRIVATE XP_List * net_decoder_list[MAX_FORMATS_OUT];    /* a list of decoders */

#define NET_MIME_EXACT_MATCH    1
#define NET_MIME_PARTIAL_MATCH  2
#define NET_WILDCARD_MATCH      3

/* will compare two mime times and return
 * NET_MIME_EXACT_MATCH if they match exactly
 * or NET_MIME_PARTIAL_MATCH if they match due
 * to a wildcard entry (image.*, or movie.*).
 *
 * if 'partial' equals '*' then
 * NET_WILDCARD_MATCH is returned
 * 
 * If there is no match then 0(zero) is returned
 */
PRIVATE int
net_compare_mime_types(char * absolute, char * partial)
{
   char *star;

   TRACEMSG(("StreamBuilder: Comparing %s and %s\n",absolute, partial));

   if(!PL_strcasecmp(absolute, partial))
	  return(NET_MIME_EXACT_MATCH);

   if(!PL_strcmp(partial, "*"))
	  return(NET_WILDCARD_MATCH);

   if((star = PL_strchr(partial, '*')) == 0)
	  return(0);  /* not a wildcard mime type */

   /* compare just the part before the slash star
    *
    */
   if(!PL_strncasecmp(absolute, partial, (star-1)-partial))
	  return(NET_MIME_PARTIAL_MATCH);

   return(0); /* no match */
}

PUBLIC XP_Bool
NET_HaveConverterForMimeType(char *content_type)
{
    net_ConverterStruct * cs_ptr;
    XP_List * ct_list_ptr = net_converter_list[FO_PRESENT];

    while ((cs_ptr=(net_ConverterStruct *) XP_ListNextObject(ct_list_ptr))
		   != 0)
    {
      if (FO_PRESENT == cs_ptr->format_out)
      {
        int compare_val = net_compare_mime_types (content_type,	cs_ptr->format_in);
        if (compare_val == NET_MIME_PARTIAL_MATCH || compare_val == NET_MIME_EXACT_MATCH)
          return TRUE;
      }
    }

    return FALSE;
}

/* Find a converter routine to create a stream and return the stream struct
*/
PR_IMPLEMENT(NET_StreamClass *)
NET_StreamBuilder  (FO_Present_Types format_out,
                    URL_Struct  *URL_s,
                    MWContext   *context)
{
    net_ConverterStruct * cs_ptr;
    XP_List * ct_list_ptr = net_converter_list[format_out];



    XP_List * ce_list_ptr = net_decoder_list[format_out];

	assert (URL_s->content_type);
	
    TRACEMSG(("Entering StreamBuilder:\n\
     F-in: %s\n\
    F-out: %d\n\
      Enc: %s\n",URL_s->content_type,format_out, 
    (URL_s->content_encoding ? URL_s->content_encoding : "None Specified")));

#if 0
	/* First, determine whether there is a content-type converter for this
	   document; if there isn't one, then the only way to handle it is to
	   send it straight to disk, so try that.
	 */
	if (format_out != FO_SAVE_TO_DISK)
	  {
		while ((cs_ptr=(net_ConverterStruct *) XP_ListNextObject(ct_list_ptr))
			   != 0)
		  {
			if (format_out == cs_ptr->format_out &&
				net_compare_mime_types (URL_s->content_type,
										cs_ptr->format_in))
			  break;
		  }

		if (! cs_ptr)  /* There is no content-type converter; save it. */
		  format_out = FO_SAVE_TO_DISK;
		ct_list_ptr = net_converter_list;  /* Reset for next traversal. */
	  }
#endif


    /* if there are content encodings then
     * execute decoders first
     */
    /* go through list of converters and choose
     * the first exact or partial match we
     * find.  The order of the list
     * is guarenteed by the registration
     * routines
	 *
	 * the format-out is compared as well.
	 *
	 * if there are transfer_encodings use them even before
	 * content_encodings
     */
	if ((URL_s->transfer_encoding && *URL_s->transfer_encoding)
		|| (URL_s->content_encoding && *URL_s->content_encoding))
	  {
		char *encoding;

		if(URL_s->transfer_encoding && *URL_s->transfer_encoding)
			encoding = URL_s->transfer_encoding;
		else
			encoding = URL_s->content_encoding;


		while ((cs_ptr=(net_ConverterStruct *) XP_ListNextObject(ce_list_ptr))
			   != 0)
		  {
			if (format_out == cs_ptr->format_out &&
				net_compare_mime_types (URL_s->content_type,
										cs_ptr->format_in) &&
				net_compare_mime_types (encoding,
										cs_ptr->encoding_in))
			  {
                  net_ConverterElement *elem = XP_ListPeekTopObject(cs_ptr->converter_stack);
                  PR_ASSERT(elem != (net_ConverterElement *)0);
				return ((NET_StreamClass *)
						((*elem->converter)
						 (format_out, elem->data_obj, URL_s, context)));
			  }
		  }
	  }

	/* now search for content-type converters
     */
    /* go through list of converters and choose
     * the first exact or partial match we
     * find.  The order of the list
     * is guarenteed by the registration
     * routines
     */
    while((cs_ptr=(net_ConverterStruct *) XP_ListNextObject(ct_list_ptr)) != 0)
      {
        if(format_out == cs_ptr->format_out)
            if(net_compare_mime_types(URL_s->content_type, cs_ptr->format_in))
              {
                  NET_StreamClass *ret_str;
                  net_ConverterElement *elem = XP_ListPeekTopObject(cs_ptr->converter_stack);
                  PR_ASSERT(elem != (net_ConverterElement *)0);
		  ret_str = (NET_StreamClass *) (*elem->converter) (format_out, 
					elem->data_obj, URL_s, context);
		  if (ret_str != NULL)
		    {
			  return(ret_str);
		    }
	          }
      }

    TRACEMSG(("Alert! did not find a converter or decoder\n"));
    FE_Alert(context, XP_GetString(XP_ALERT_CANTFIND_CONVERTER));

    return(0);  /* Uh-oh. didn't find a converter.  VERY VERY BAD! */
}

/* prints out all presentation mime types during successive calls
 * call with first equal true to reset to the beginning
 * and with first equal false to print out the next
 * converter in the list.  Returns zero when all converters
 * are printed out.
 */
MODULE_PRIVATE char *
XP_ListNextPresentationType(PRBool first)
{
    static XP_List * converter_list_ptr;
    net_ConverterStruct * converter;

    /* reset list pointers 
     */
    if(first)
        converter_list_ptr = net_converter_list[FO_PRESENT];

    /* print out next converter if there are any more
     */
    converter = (net_ConverterStruct *) XP_ListNextObject(converter_list_ptr);

    if(converter && converter->format_out == FO_PRESENT)
	return(converter->format_in);

    /* else */
    return(NULL);
}
	
/* prints out all encoding mime types during successive calls
 * call with first equal true to reset to the beginning
 * and with first equal false to print out the next
 * converter in the list.  Returns zero when all converters
 * are printed out.
 */
MODULE_PRIVATE char *
XP_ListNextEncodingType(PRBool first)
{
    static XP_List * decoder_list_ptr;
    net_ConverterStruct * converter;
	static int count;
	static index = 0;
	static int curArrayIndex = 0;


	while ((index < count) && (curArrayIndex < MAX_FORMATS_OUT)) {
		decoder_list_ptr = net_decoder_list[curArrayIndex];
		count = XP_ListCount(decoder_list_ptr);
		if (count > 0) {
			index = 0;
			break;
		}
		else count++;
	}
#if 0
		/* reset list pointers
		*/
		if(first)
		{
			decoder_list_ptr = net_decoder_list;
	    }
#endif
	converter = (net_ConverterStruct *) XP_ListNextObject(decoder_list_ptr);
    if(converter) {
		index++;
        return(converter->format_in);
	}

    /* else */
    return(NULL);
}
     
PRIVATE void
net_RegisterGenericConverterOrDecoder(XP_List        ** conv_list,
							  		  char            * format_in,
							  		  char            * encoding_in,
                              		  FO_Present_Types  format_out,
                              		  void            * data_obj,
                              		  NET_Converter   * converter_func)
{
    XP_List * list_ptr = *conv_list;
    net_ConverterStruct * converter_struct_ptr;
    net_ConverterStruct * new_converter_struct;
    net_ConverterElement *elem;
    int f_in_type;
    XP_List *pList;
	int i;

	if (needInit) {	/* MWH - this will only happen once. It is pretty ugly,
					 but I don't know how to get around it.  */
		needInit = FALSE;

		for (i = 0; i <MAX_FORMATS_OUT;i++) {
			net_converter_list[i] = net_decoder_list[i] = 0;
		}

	}
	
	pList = list_ptr = conv_list[format_out];
    /* check for an existing converter with the same format_in and format_out 
     * and add this to the list if found.
     *
     * if the list is NULL XP_ListNextObject will return 0
     */
    while((converter_struct_ptr = (net_ConverterStruct *) XP_ListNextObject(list_ptr)) != 0){
        if(format_out == converter_struct_ptr->format_out)
                    /* Rewritten to logic equivalent but should-be-faster version:*/
		    if (((!encoding_in && !converter_struct_ptr->encoding_in) ||
                         (encoding_in && converter_struct_ptr->encoding_in &&
                          !PL_strcmp (encoding_in,
                                      converter_struct_ptr->encoding_in)))
                        && !PL_strcasecmp(format_in, converter_struct_ptr->format_in)
#ifdef XP_WIN
				 && (converter_struct_ptr->bAutomated == autoFlag)
#endif
				)
                  {
                  /* Add a new converter element to the list */
                  net_ConverterElement *elem = PR_NEWZAP(net_ConverterElement);
                  if( elem == (net_ConverterElement *)0 ) return;
                  elem->converter = converter_func;
                  elem->data_obj  = data_obj;
                  XP_ListAddObject(converter_struct_ptr->converter_stack, elem);
		        return;
	          }
       }
    /* find out the type of entry format_in is
     */
    f_in_type = NET_MIME_EXACT_MATCH;
    if(PL_strchr(format_in, '*'))
      {
	    if(!PL_strcmp(format_in, "*"))
	        f_in_type = NET_WILDCARD_MATCH;
	    else
	        f_in_type = NET_MIME_PARTIAL_MATCH;
      }

    /* if existing converter not found/replaced
     * add this converter to the list in
     * its order of importance so that we can
     * take the first exact or partial fit
     */ 
    new_converter_struct = PR_NEW(net_ConverterStruct);
    if(!new_converter_struct) {
	    return;
	}

    new_converter_struct->converter_stack = XP_ListNew();
    if( new_converter_struct->converter_stack == (XP_List *)0 )
    {
        PR_Free(new_converter_struct);
        return;
    }

    elem = PR_NEWZAP(net_ConverterElement);
    if( elem == (net_ConverterElement *)0 )
    {
        XP_ListDestroy(new_converter_struct->converter_stack);
        PR_Free(new_converter_struct);
        return;
    }

    elem->converter = converter_func;
    elem->data_obj  = data_obj;
    new_converter_struct->format_in = 0;
    StrAllocCopy(new_converter_struct->format_in, format_in);
    new_converter_struct->encoding_in = (encoding_in
										 ? PL_strdup (encoding_in) : 0);
    new_converter_struct->format_out = format_out;
    XP_ListAddObject(new_converter_struct->converter_stack, elem);
	new_converter_struct->bAutomated = autoFlag;


    if(!pList) {
/*    if(!*conv_list) */
	    /* create the list and add this object
	     * don't worry about order since there
	     * can't be any 
  	     */
	    pList = XP_ListNew();
	    if(!pList)
	        return; /* big error */
		conv_list[format_out] = pList;
 	    XP_ListAddObject(pList, new_converter_struct);
	    return;
      }
    if(f_in_type == NET_MIME_EXACT_MATCH)
      {
#if defined(XP_WIN) && defined (MOZILLA_CLIENT)
        net_ConverterStruct *pRegistered;
		XP_List *tempList = pList;
		int count = XP_ListCount(tempList);
		for (i = 0; i < count; i++)	{
			pRegistered = (net_ConverterStruct *) XP_ListNextObject(tempList);
		
			if(new_converter_struct->bAutomated == TRUE)	{
				if(pRegistered->bAutomated == FALSE)	{
					XP_ListInsertObject (pList, pRegistered, new_converter_struct);
					return;
				}
			}
		
			/* if it is an exact match type 
			* then we can just add it to the beginning
			* of the list
			*/
			if (!PL_strcasecmp(pRegistered->format_in, new_converter_struct->format_in)
                && ((!pRegistered->encoding_in && !new_converter_struct->format_in)
                    || !PL_strcasecmp(pRegistered->encoding_in, new_converter_struct->encoding_in))) {
				if(new_converter_struct->bAutomated == TRUE)	{
					PR_Free( new_converter_struct);
					return;
				}
			
				//	Overwrite
				memcpy(pRegistered, new_converter_struct, sizeof(net_ConverterStruct));
				PR_Free( new_converter_struct);
				return;
			}
		}

        
        
        
        
        
        
        XP_ListInsertObject (pList, pRegistered, new_converter_struct);
#else
	    XP_ListAddObject(pList, new_converter_struct);
	    TRACEMSG(("Adding Converter to Beginning of list"));
#endif
      }
    else if(f_in_type == NET_MIME_PARTIAL_MATCH)
      {
	    /* this is a partial wildcard of the form (image/(star))
         * store it at the end of all the exact match pairs
         */
        list_ptr = pList;
        while((converter_struct_ptr = (net_ConverterStruct *) XP_ListNextObject(list_ptr)) != 0)
          {
            if(PL_strchr(converter_struct_ptr->format_in, '*'))
	          {
		        XP_ListInsertObject (pList, 
									 converter_struct_ptr, 
									 new_converter_struct); 
		        return;
              }
          }
		/* else no * objects in list */
	    XP_ListAddObjectToEnd(pList, new_converter_struct);
	    TRACEMSG(("Adding Converter to Middle of list"));
      }
    else
      {
	    /* this is a very wild wildcard match of the form "*" only.
         * It has the least precedence so store it at the end
	     * of the list.  (There must always be at least one
	     * item in the list (see above))
	     */
	    XP_ListAddObjectToEnd(pList, new_converter_struct);
	    TRACEMSG(("Adding Converter to end of list"));
      }
}

struct net_encoding_converter
{
  char *encoding_in;
  void *data_obj;
  NET_Converter * converter_func;
};

static struct net_encoding_converter *net_all_encoding_converters;
static int net_encoding_converter_size = 0;
static int net_encoding_converter_fp = 0;

PUBLIC void* NET_GETDataObject(XP_List* list, char* mineType, void** obj)
{
	net_ConverterStruct* converter_struct_ptr;

    *obj = (void*)0;

	while((converter_struct_ptr = (net_ConverterStruct *)XP_ListNextObject(list)) != NULL)    {
        if(converter_struct_ptr->bAutomated)    {
	        if(!PL_strcmp(converter_struct_ptr->format_in, mineType)) {
                net_ConverterElement *elem = XP_ListPeekTopObject(converter_struct_ptr->converter_stack);
		        *obj = (void*)converter_struct_ptr;
		        return elem->data_obj;
	        }
        }
    }

	return (void*)0;
}

PUBLIC XP_List* NET_GetRegConverterList(FO_Present_Types iFormatOut)
{
	return 	net_converter_list[iFormatOut];

}
/* register a routine to decode a stream
 */

PUBLIC void
NET_RegisterEncodingConverter(char *encoding_in,
                              void          * data_obj,
                              NET_Converter * converter_func)
{

	/* Don't register anything right now, but remember that we have a
	   decoder for this so that we can register it hither and yon at
	   a later date.
	 */
  if (net_encoding_converter_size <= net_encoding_converter_fp)
	{
	  net_encoding_converter_size += 10;
	  if (net_all_encoding_converters)
		net_all_encoding_converters = (struct net_encoding_converter *)
		  PR_Realloc (net_all_encoding_converters,
					  net_encoding_converter_size
					  * sizeof (*net_all_encoding_converters));
	  else
		net_all_encoding_converters = (struct net_encoding_converter *)
		  PR_Calloc (net_encoding_converter_size,
					 sizeof (*net_all_encoding_converters));
	}

  { /* Don't register the same decoder twice; having too many
       decoders is multiplicatively expensive with respect to the number of
       calls to net_RegisterGenericConverterOrDecoder() from the loop inside
       NET_RegisterAllEncodingConverters(). */
      int i;
      for (i=0; i < net_encoding_converter_fp; i++)
	  if ((net_all_encoding_converters[i].data_obj == data_obj)
              && (net_all_encoding_converters[i].converter_func == converter_func)
              && (strcmp(net_all_encoding_converters[i].encoding_in,encoding_in)
                  == 0))
              return;
  }
  
  net_all_encoding_converters [net_encoding_converter_fp].encoding_in
	= PL_strdup (encoding_in);
  net_all_encoding_converters [net_encoding_converter_fp].data_obj
	= data_obj;
  net_all_encoding_converters [net_encoding_converter_fp].converter_func
	= converter_func;
  net_encoding_converter_fp++;

}


#ifdef MOZILLA_CLIENT

void
NET_DumpDecoders()
{
#ifdef DEBUG
    net_ConverterStruct * cs_ptr;
    XP_List *list_ptr = net_decoder_list[FO_PRESENT];

    while((cs_ptr = XP_ListNextObject(list_ptr)) != 0)
    {
        char *msg = PR_smprintf("in: %s  out: %d\n",cs_ptr->encoding_in, cs_ptr->format_out);

        TRACEMSG(("%s", msg));
        FREE(msg);
    }
#endif /* DEBUG */
}

/* register an encoding converter that is used for everything,
 * no exeptions
 * this is necessary for chunked encoding
 */
MODULE_PRIVATE void
NET_RegisterUniversalEncodingConverter(char *encoding_in,
                              void          * data_obj,
                              NET_Converter * converter_func)
{
    int i;

	/* for each list in the net_decoder_list array */
	for (i = 0; i < MAX_FORMATS_OUT; i++) 
	{

	  net_RegisterGenericConverterOrDecoder (net_decoder_list,
											 "*",
											 encoding_in,
											 i,  /* format out */
											 data_obj,
											 converter_func);
	}

}

void
NET_RegisterAllEncodingConverters (char *format_in,
								   FO_Present_Types format_out)
{
  int i;

#ifdef XP_UNIX
  /* On Unix at least, there should be *some* registered... */
  assert (net_encoding_converter_fp > 0);
#endif

  for (i = 0; i < net_encoding_converter_fp; i++)
	{
	  char *encoding_in = net_all_encoding_converters [i].encoding_in;
	  void *data_obj = net_all_encoding_converters [i].data_obj;
	  NET_Converter *func = net_all_encoding_converters [i].converter_func;

	  net_RegisterGenericConverterOrDecoder (net_decoder_list,
											 format_in,
											 encoding_in,
											 format_out,
											 data_obj,
											 func);

	  net_RegisterGenericConverterOrDecoder (net_decoder_list,
											 format_in,
											 encoding_in,
											 format_out | FO_CACHE_ONLY,
											 data_obj,
											 func);

	  net_RegisterGenericConverterOrDecoder (net_decoder_list,
											 format_in,
											 encoding_in,
											 format_out | FO_ONLY_FROM_CACHE,
											 data_obj,
											 func);
	}
}

#endif /* MOZILLA_CLIENT */




/*  Register a routine to convert between format_in and format_out
 *
 */
#if (defined(XP_WIN) || defined(XP_OS2)) && defined(MOZILLA_CLIENT) 
/*  Reroute XP handling of streams for windows.
 *  MWH -This routine should not be called recursively.  
	It rely on the static global variable to set the  
 *	automatedflag. The reason for this is I don't to 
	change the API on NET_RegisterContentTypeConverter
	because	some other group still depend on this API.
 *	If we decided to change API, please fix this.
 */
PUBLIC void
NET_RegContentTypeConverter (char * format_in,
                             FO_Present_Types format_out,
                             void          * data_obj,
                             NET_Converter * converter_func,
                             Bool          bAutomated)
{
	autoFlag = bAutomated;
	NET_RegisterContentTypeConverter (format_in, format_out,
											data_obj,
											converter_func);
	autoFlag = FALSE;

}

#endif

PUBLIC void
NET_RegisterContentTypeConverter (char          * format_in,
                                  FO_Present_Types format_out,
                                  void          * data_obj,
                                  NET_Converter * converter_func)
{
    net_RegisterGenericConverterOrDecoder(net_converter_list,
                                          format_in,
										  0,
                                          format_out,
                                          data_obj,
                                          converter_func);

}

#ifdef MOZILLA_CLIENT
#ifdef XP_UNIX

#include "cvview.h"
#include "cvextcon.h"

/* register a mime type and a command to be executed
 *
 * if stream_block_size is zero then the data will be completely
 * pooled and the external viewer called with the filename.
 * if stream_block_size is greater than zero the viewer will be
 * called with a popen and the stream_block_size will be used as
 * the maximum size of each buffer to pass to the viewer
 */
MODULE_PRIVATE void
NET_RegisterExternalViewerCommand(char * format_in, 
								  char * system_command, 
								  unsigned int stream_block_size) 
{
    CV_ExtViewStruct * new_obj = PR_NEW(CV_ExtViewStruct);

    if(!new_obj)
	   return;

    memset(new_obj, 0, sizeof(CV_ExtViewStruct));

    /* make a new copy of the command so it can be passed
     * as the data object
     */
    StrAllocCopy(new_obj->system_command, system_command);
    new_obj->stream_block_size = stream_block_size;

    /* register the routine
     */
    NET_RegisterContentTypeConverter(format_in, FO_PRESENT, new_obj, NET_ExtViewerConverter);

	/* Also register the content-encoding converters on this type.
	   Those get registered only on types we know how to display,
	   either internally or externally - and don't get registered
	   on types we know nothing about.
	 */
	NET_RegisterAllEncodingConverters (format_in, FO_PRESENT);
}

/* removes all external viewer commands
 */
PUBLIC void
NET_ClearExternalViewerConverters(void)
{
    XP_List * list_ptr;
	XP_List *temp_list;
    net_ConverterStruct * converter_struct_ptr;
	int i;

	for (i = 0; i <MAX_FORMATS_OUT; i++) { 
		list_ptr = temp_list = net_converter_list[i];
		while((converter_struct_ptr = 
						(net_ConverterStruct *) XP_ListNextObject(list_ptr)))
	  {
          XP_List *new;
          net_ConverterElement *elem;
          
          new = XP_ListNew();
          if( new == (XP_List *)0 ) return;

          while( (elem = XP_ListRemoveTopObject(converter_struct_ptr->converter_stack)) != (net_ConverterElement *)0 )
          {
              if( elem->converter == NET_ExtViewerConverter )
              {
                  PR_FREEIF(elem->data_obj);
                  PR_Free(elem);
                  continue;
              }
              else
              {
                  XP_ListAddObjectToEnd(new, elem);
              }
          }

          XP_ListDestroy(converter_struct_ptr->converter_stack);

          if( XP_ListCount(new) > 0 )
          {
              converter_struct_ptr->converter_stack = new;
          }
          else
          {
              XP_ListDestroy(new);
              XP_ListRemoveObject(temp_list, converter_struct_ptr);
              PR_Free(converter_struct_ptr->format_in);
              PR_Free(converter_struct_ptr->encoding_in);
              PR_Free(converter_struct_ptr);
          }
	  }
	}
}

/* removes all registered converters of any kind
 */
PUBLIC void
NET_ClearAllConverters(void)
{
    net_ConverterStruct * aConverter;
	net_ConverterElement * aConverterStackElement;
	XP_List * theList;
	int i;

	/* for each list in the net_converter_list array */
	for (i = 0; i < MAX_FORMATS_OUT; i++) {
		theList = net_converter_list[i];
		/* Traverse it, removing each converter, and freeing its data */
		while( aConverter = (net_ConverterStruct *) XP_ListRemoveTopObject(theList) )
		{
			FREEIF(aConverter->format_in);
			FREEIF(aConverter->encoding_in);
			while( aConverterStackElement = XP_ListRemoveTopObject(aConverter->converter_stack) )
			{
#ifdef XP_UNIX
				if(aConverterStackElement->converter == NET_ExtViewerConverter)
					PR_FREEIF(aConverterStackElement->data_obj);
#endif /* XP_UNIX */
				PR_Free(aConverterStackElement);
			}
			XP_ListDestroy(aConverter->converter_stack);
			FREE(aConverter);
		}
		XP_ListDestroy(theList);
		net_converter_list[i] = NULL;
	}

	/* for each list in the net_decoder_list array */
	for (i = 0; i < MAX_FORMATS_OUT; i++) {
		theList = net_decoder_list[i];
		/* Traverse it, removing each converter, and freeing its data */
		while( aConverter = (net_ConverterStruct *) XP_ListRemoveTopObject(theList) )
		{
			FREEIF(aConverter->format_in);
			FREEIF(aConverter->encoding_in);
			while( aConverterStackElement = XP_ListRemoveTopObject(aConverter->converter_stack) )
			{
#ifdef XP_UNIX
				if(aConverterStackElement->converter == NET_ExtViewerConverter)
					PR_FREEIF(aConverterStackElement->data_obj);
#endif /* XP_UNIX */
				PR_Free(aConverterStackElement);
			}
			XP_ListDestroy(aConverter->converter_stack);
			FREE(aConverter);
		}
		XP_ListDestroy(theList);
		net_decoder_list[i] = NULL;
	}
}

/* register an external program to act as a content-type
 * converter
 */
MODULE_PRIVATE void
NET_RegisterExternalConverterCommand(char * format_in,
									 int    format_out,
                                     char * system_command,
                                     char * new_format)
{
    CV_ExtConverterStruct * new_obj = PR_NEW(CV_ExtConverterStruct);

    if(!new_obj)
       return;

    memset(new_obj, 0, sizeof(CV_ExtConverterStruct));

    /* make a new copy of the command so it can be passed
     * as the data object
     */
    StrAllocCopy(new_obj->command,    system_command);
    StrAllocCopy(new_obj->new_format, new_format);
    new_obj->is_encoding_converter = FALSE;

    /* register the routine
     */
    NET_RegisterContentTypeConverter(format_in, format_out, new_obj, NET_ExtConverterConverter);
}

/* register an external program to act as a content-encoding
 * converter
 */
PUBLIC void
NET_RegisterExternalDecoderCommand (char * format_in,
									char * format_out,
									char * system_command)
{
    CV_ExtConverterStruct * new_obj = PR_NEW(CV_ExtConverterStruct);

    if(!new_obj)
       return;

    memset(new_obj, 0, sizeof(CV_ExtConverterStruct));

    /* make a new copy of the command so it can be passed
     * as the data object
     */
    StrAllocCopy(new_obj->command,    system_command);
    StrAllocCopy(new_obj->new_format, format_out);
    new_obj->is_encoding_converter = TRUE;

    /* register the routine
     */
    NET_RegisterEncodingConverter (format_in, new_obj,
								   NET_ExtConverterConverter);
}


#endif /* XP_UNIX */
#endif /* MOZILLA_CLIENT */

/*  NewMKStream
 *  Utility to create a new initialized NET_StreamClass object
 */
NET_StreamClass *
NET_NewStream          (char                 *name,
                        MKStreamWriteFunc     process,
                        MKStreamCompleteFunc  complete,
                        MKStreamAbortFunc     abort,
                        MKStreamWriteReadyFunc ready,
                        void                 *streamData,
                        MWContext            *windowID)
{
    NET_StreamClass *stream = PR_NEW (NET_StreamClass);
    if (!stream)
        return nil;
        
    stream->name        = name;
    stream->window_id   = windowID;
    stream->data_object = streamData;
    stream->put_block   = process;
    stream->complete    = complete;
    stream->abort       = abort;
    stream->is_write_ready = ready;
    
    return stream;
}

PRIVATE XP_List *
net_GetConverterOrDecoderList(XP_List        ** conv_list,
                              char            * format_in,
                              char            * encoding_in,
                              FO_Present_Types  format_out)
{
    XP_List * list_ptr = *conv_list;
    net_ConverterStruct * converter_struct_ptr;

    while((converter_struct_ptr = (net_ConverterStruct *) XP_ListNextObject(list_ptr)) != 0)
	    if(format_out == converter_struct_ptr->format_out)
	        if(!PL_strcasecmp(format_in, converter_struct_ptr->format_in)
			   && ((!encoding_in && !converter_struct_ptr->encoding_in) ||
				   (encoding_in && converter_struct_ptr->encoding_in &&
					!PL_strcmp (encoding_in,
								converter_struct_ptr->encoding_in)))
			   )
	          {
                  return converter_struct_ptr->converter_stack;
              }

    return (XP_List *)0;
}

PUBLIC void
NET_DeregisterContentTypeConverter(char *format_in, FO_Present_Types format_out)
{
    XP_List *l;
    net_ConverterElement *elem;

    l = net_GetConverterOrDecoderList(net_converter_list, format_in, 0, format_out);
    if( l == (XP_List *)0 ) return;

    elem = XP_ListRemoveTopObject(l);
# ifdef XP_UNIX
    /* total kludge!! */
    if( elem->converter == NET_ExtViewerConverter )
        PR_FREEIF(elem->data_obj);
# endif
    PR_Free(elem);

    return;
}

#ifdef DEBUG
MODULE_PRIVATE void
NET_DisplayStreamInfoAsHTML(ActiveEntry *cur_entry)
{
	char *buffer = (char*)PR_Malloc(2048);
   	NET_StreamClass * stream;
	int i;

    static char *fo_names[] = {
        /*  0 */    "(unknown)",
        /*  1 */    "Present",
        /*  2 */    "Internal Image",
        /*  3 */    "Out to Proxy Client",
        /*  4 */    "View Source",
        /*  5 */    "Save As",
        /*  6 */    "Save as Text",
        /*  7 */    "Save as Postscript",
        /*  8 */    "Quote Message",
        /*  9 */    "Mail To",
        /* 10 */    "OLE Network",
        /* 11 */    "Multipart Image",
        /* 12 */    "Embed",
        /* 13 */    "Print",
        /* 14 */    "Plugin",
        /* 15 */    "Append to Folder",
        /* 16 */    "Do Java",
        /* 17 */    "Byterange",
        /* 18 */    "EDT Save Image",
        /* 19 */    "Edit",
        /* 20 */    "Load HTML Help Map File",
        /* 21 */    "Save to News DB",
        /* 22 */    "Open Draft",
    };

	if(!buffer)
	  {
		cur_entry->status = MK_UNABLE_TO_CONVERT;
		return;
	  }

	StrAllocCopy(cur_entry->URL_s->content_type, TEXT_HTML);

	cur_entry->format_out = CLEAR_CACHE_BIT(cur_entry->format_out);
	stream = NET_StreamBuilder(cur_entry->format_out, 
							   cur_entry->URL_s, 
							   cur_entry->window_id);

	if(!stream)
	  {
		cur_entry->status = MK_UNABLE_TO_CONVERT;
		FREE(buffer);
		return;
	  }


	/* define a macro to push a string up the stream
	 * and handle errors
	 */
#define PUT_PART(part)													\
cur_entry->status = (*stream->put_block)(stream,			\
										part ? part : "Unknown",		\
										part ? PL_strlen(part) : 7);	\
if(cur_entry->status < 0)												\
  goto END;

    sprintf(buffer, 
"<html><head><title>Information about the Netscape streams configuration</title></head>\n"
"<body><h1>Information about the Netscape streams configuration</h1>\n"
);

    PUT_PART(buffer);

    sprintf(buffer, "<h2>Converter List</h2>\n<ul>");
    PUT_PART(buffer);

    for( i = 0; i < MAX_FORMATS_OUT; i++ )
    {
        int j = i & ~FO_CACHE_ONLY & ~FO_ONLY_FROM_CACHE;
        XP_List *list_ptr = net_converter_list[i];
        net_ConverterStruct *csp;

        sprintf(buffer, "<li><ul><p>%s", j < sizeof(fo_names)/sizeof(fo_names[0]) ? 
                   fo_names[j] : fo_names[0]);
        PUT_PART(buffer);

        if( i & FO_CACHE_ONLY )
        {
            sprintf(buffer, " | Cache Only");
            PUT_PART(buffer);
        }

        if( i & FO_ONLY_FROM_CACHE )
        {
            sprintf(buffer, " | Only From Cache");
            PUT_PART(buffer);
        }

        sprintf(buffer, " [%d]</p>", i);
        PUT_PART(buffer);

        while( (csp = (net_ConverterStruct *)XP_ListNextObject(list_ptr)) != (net_ConverterStruct *)0 )
        {
            XP_List *stack = csp->converter_stack;
            int stack_count = XP_ListCount(stack);
            net_ConverterElement *elem;
            sprintf(buffer, 
                       "<li>Format In: %s<br>\n"
                       "Encoding In: %s<br>\n"
                       "Present Type: %d<br>\n"
                       "%s<br><ol>%d Converter%s:\n",
                       (csp->format_in != (char *)0) ? csp->format_in : "(null)",
                       (csp->encoding_in != (char *)0) ? csp->encoding_in : "(null)",
                       csp->format_out,
                       csp->bAutomated ? "Automated" : "Not automated",
                       stack_count, (stack_count == 1) ? "" : "s");
            PUT_PART(buffer);

            while( (elem = (net_ConverterElement *)XP_ListNextObject(stack)) != (net_ConverterElement *)0 )
            {
#if defined(__sun) && !defined(SVR4)
                sprintf(buffer, "<li>Converter: %lu<br>Object: %lu<br>\n",
                           elem->converter, elem->data_obj);
#else
                sprintf(buffer, "<li>Converter: %p<br>Object: %p<br>\n",
                           elem->converter, elem->data_obj);
#endif
                PUT_PART(buffer);
            }

            sprintf(buffer, "<br></ol>\n");
            PUT_PART(buffer);
        }

        sprintf(buffer, "</ul>\n");
        PUT_PART(buffer);
    }

    sprintf(buffer, "</ul>\n");
    PUT_PART(buffer);

    sprintf(buffer, "<h2>Decoder List</h2>\n<ul>");
    PUT_PART(buffer);

    for( i = 0; i < MAX_FORMATS_OUT; i++ )
    {
        int j = i & ~FO_CACHE_ONLY & ~FO_ONLY_FROM_CACHE;
        XP_List *list_ptr = net_decoder_list[i];
        net_ConverterStruct *csp;

        sprintf(buffer, "<li><ul><p>%s", j < sizeof(fo_names)/sizeof(fo_names[0]) ? 
                   fo_names[j] : fo_names[0]);
        PUT_PART(buffer);

        if( i & FO_CACHE_ONLY )
        {
            sprintf(buffer, " | Cache Only");
            PUT_PART(buffer);
        }

        if( i & FO_ONLY_FROM_CACHE )
        {
            sprintf(buffer, " | Only From Cache");
            PUT_PART(buffer);
        }

        sprintf(buffer, " [%d]</p>", i);
        PUT_PART(buffer);

        while( (csp = (net_ConverterStruct *)XP_ListNextObject(list_ptr)) != (net_ConverterStruct *)0 )
        {
            XP_List *stack = csp->converter_stack;
            int stack_count = XP_ListCount(stack);
            net_ConverterElement *elem;
            sprintf(buffer, 
                       "<li>Format In: %s<br>\n"
                       "Encoding In: %s<br>\n"
                       "Present Type: %d<br>\n"
                       "%s<br><ol>%d Converter%s:\n",
                       (csp->format_in != (char *)0) ? csp->format_in : "(null)",
                       (csp->encoding_in != (char *)0) ? csp->encoding_in : "(null)",
                       csp->format_out,
                       csp->bAutomated ? "Automated" : "Not automated",
                       stack_count, (stack_count == 1) ? "" : "s");
            PUT_PART(buffer);

            while( (elem = (net_ConverterElement *)XP_ListNextObject(stack)) != (net_ConverterElement *)0 )
            {
#if defined(__sun) && !defined(SVR4)
                sprintf(buffer, "<li>Converter: %lu<br>Object: %lu<br>\n",
                           elem->converter, elem->data_obj);
#else
                sprintf(buffer, "<li>Converter: %p<br>Object: %p<br>\n",
                           elem->converter, elem->data_obj);
#endif
                PUT_PART(buffer);
            }

            sprintf(buffer, "<br></ol>\n");
            PUT_PART(buffer);
        }

        sprintf(buffer, "</ul>\n");
        PUT_PART(buffer);
    }

    sprintf(buffer, "</ul>\n");
    PUT_PART(buffer);

  END:
    PR_Free(buffer);
    if( cur_entry->status < 0 )
        (*stream->abort)(stream, cur_entry->status);
    else
        (*stream->complete)(stream);

    return;
}

#endif /* DEBUG */

#ifdef PROFILE
#pragma profile off
#endif

