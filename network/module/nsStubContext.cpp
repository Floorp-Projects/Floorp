/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsStubContext.h"

#include "net.h"
#include "mktrace.h"
#include "structs.h"
#include "ctxtfunc.h"
#include "xp_list.h"
#include "plstr.h"
#include "fe_proto.h" // Needed for prototype of FE_Alert().
#include "proto.h" // Needed for prototype of XP_FindContextOfType().

#include "nsString.h"
#include "nsIStreamListener.h"
#include "nsINetSupport.h"
#include "nsNetStream.h"


void free_stub_context(MWContext *window_id);


/****************************************************************************/
/* Beginning  of MWContext Evil!!!                                          */
/* -------------------------------                                          */
/*                                                                          */
/* Define a dummy MWContext where all of the upcalls are stubbed out.       */
/*                                                                          */
/****************************************************************************/

PRIVATE int
stub_noop(int x, ...)
{
#ifdef XP_MAC
#pragma unused (x)
#endif
/*  DebugBreak(); */
    return 0;
}

static nsIStreamListener *getStreamListener(URL_Struct *URL_s) 
{
    nsIStreamListener *res = NULL;
    
    if (URL_s && URL_s->fe_data) {
        /* 
         * Retrieve the nsConnectionInfo object from the fe_data field
         * of the URL_Struct...
         */
        nsConnectionInfo *pConn = (nsConnectionInfo *)URL_s->fe_data;
        if (pConn) {
            res = pConn->pConsumer;
        }
    }

    NS_IF_ADDREF(res);

    return res;
}

static NS_DEFINE_IID(kINetSupportIID, NS_INETSUPPORT_IID);

static nsINetSupport *getNetSupport(URL_Struct *URL_s) 
{
  nsINetSupport *netSupport = nsnull;

  /* Access the nsConnectionInfo object off of the URL Struct fe_data */
  if ((nsnull != URL_s) && (nsnull != URL_s->fe_data)) {
    nsConnectionInfo *pConn = (nsConnectionInfo *)URL_s->fe_data;

    /* Now get the nsIURL held by the nsConnectionInfo... */
    if ((nsnull != pConn) && (nsnull != pConn->pURL)) {
      nsISupports *container;

      /* The nsINetSupport interface will be implemented by the container */
      nsresult err = pConn->pURL->GetContainer(&container);
      if (NS_SUCCEEDED(err) && (nsnull != container)) {
        container->QueryInterface(kINetSupportIID, (void **) &netSupport);
        NS_RELEASE(container);
      }
    }
  }
  return netSupport;
}

PRIVATE
void stub_Alert(MWContext *context,
                const char *msg)
{
  nsINetSupport *ins;

  if (nsnull != (ins = getNetSupport(context->modular_data))) {
    nsAutoString str(msg);

    ins->Alert(str);
    NS_RELEASE(ins);;
  } 
  /* No nsINetSupport interface... */
  else {
    printf("%cAlert: %s", '\007', msg);
  }
}

extern "C" void FE_Alert(MWContext *context, const char *msg)
{
    stub_Alert(context, msg);
}

#if defined (WIN32)
extern "C" char * FE_GetProgramDirectory(char *buffer, int length) {
    printf("XXX: returning c:\\temp\\ for the FE_GetProgramDirectory!\n");
    strcpy(buffer, "c:\\temp\\");
    return buffer;
}
#elif defined (XP_UNIX)
extern "C" char * fe_GetProgramDirectory(char *buffer, int length) {
    printf("XXX: returning /tmp for the fe_GetProgramDirectory!\n");
    strcpy(buffer, "/tmp/");
    return buffer;
}
#endif

PRIVATE
XP_Bool stub_Confirm(MWContext *context,
                     const char *msg)
{
  nsINetSupport *ins;
  XP_Bool bResult = FALSE;
    
  if (nsnull != (ins = getNetSupport(context->modular_data))) {
    nsAutoString str(msg);

    bResult = ins->Confirm(str);
    NS_RELEASE(ins);
  } 
  /* No nsINetSupport interface... */
  else {
    printf("%cConfirm: %s (y/n)? ", '\007', msg);
    char c;
    for (;;) {
      c = getchar();
      if (tolower(c) == 'y') {
        bResult = TRUE;
      }
      if (tolower(c) == 'n') {
        bResult = FALSE;
      }
    }
  }
  return bResult;
}

PRIVATE
char *stub_Prompt(MWContext *context,
                  const char *msg,
                  const char *def)
{
  nsINetSupport *ins;
  char *result = nsnull;
    
  if (nsnull != (ins = getNetSupport(context->modular_data))) {
    nsAutoString str(msg);
    nsAutoString defStr(def);
    nsAutoString res;

    if (ins->Prompt(msg, defStr, res)) {
      NS_RELEASE(ins);
      result = res.ToNewCString();
    }
    NS_RELEASE(ins);

  } 
  /* No nsINetSupport interface... */
  else {
    char buf[256];

    printf("%s\n", msg);
    printf("%cPrompt (default=%s): ", '\007', def);
    fgets(buf, sizeof buf, stdin);
    if (PL_strlen(buf)) {
      result = PL_strdup(buf);
    } else {
      result = PL_strdup(def);
    }
  }

  return result;
}

PRIVATE XP_Bool
stub_PromptUsernameAndPassword(MWContext *context,
                               const char *msg,
                               char **username,
                               char **password)
{
  nsINetSupport *ins;
  XP_Bool bResult = FALSE;
    
  if (nsnull != (ins = getNetSupport(context->modular_data))) {
    nsAutoString str(msg);
    nsAutoString userStr(*username);
    nsAutoString pwdStr(*password);

    if (ins->PromptUserAndPassword(msg, userStr, pwdStr)) {
      *username = userStr.ToNewCString();
      *password = pwdStr.ToNewCString();
      bResult = TRUE;
    }
    NS_RELEASE(ins);

  } 
  /* No nsINetSupport interface... */
  else {
    char buf[256];

    printf("%s\n", msg);
    printf("%cUser (default=%s): ", '\007', *username);
    fgets(buf, sizeof buf, stdin);
    if (strlen(buf)) {
      *username = PL_strdup(buf);
    }

    printf("%cPassword (default=%s): ", '\007', *password);
    fgets(buf, sizeof buf, stdin);
    if (strlen(buf)) {
      *password = PL_strdup(buf);
    }

    if (**username) {
       bResult = TRUE;
    } else {
      PR_FREEIF(*username);
      PR_FREEIF(*password);
    }
  }

  return bResult;
}

PRIVATE
char *stub_PromptPassword(MWContext *context,
                          const char *msg)
{
  nsINetSupport *ins;
  char *result = nsnull;

  if (nsnull != (ins = getNetSupport(context->modular_data))) {
    nsAutoString str(msg);
    nsAutoString res;

    if (ins->PromptPassword(msg, res)) {
      result = res.ToNewCString();
    }
    NS_RELEASE(ins);

  } 
  /* No nsINetSupport interface... */
  else {
    char buf[256];

    printf("%s\n", msg);
    printf("%cPassword: ", '\007');
    fgets(buf, sizeof buf, stdin);
    if (PL_strlen(buf)) {
      result = PL_strdup(buf);
    }
  }

  return result;
}

PRIVATE void stub_GraphProgressInit(MWContext  *context, 
                                    URL_Struct *URL_s, 
                                    int32       content_length)
{
    nsIStreamListener *pListener;

    if (URL_s->load_background)
        return;

    if (nsnull != (pListener = getStreamListener(URL_s))) {
        nsConnectionInfo *pConn = (nsConnectionInfo *) URL_s->fe_data;
        NS_ASSERTION(content_length >= 0, "negative content_length");
        pListener->OnProgress(pConn->pURL, 0, (PRUint32)content_length);
        NS_RELEASE(pListener);
    }
}


PRIVATE void stub_GraphProgress(MWContext  *context, 
                                URL_Struct *URL_s, 
                                int32       bytes_received,
                                int32       bytes_since_last_time,
                                int32       content_length)
{
    nsIStreamListener *pListener;

    if (URL_s->load_background)
        return;

    if (nsnull != (pListener = getStreamListener(URL_s))) {
        nsConnectionInfo *pConn = (nsConnectionInfo *) URL_s->fe_data;
        NS_ASSERTION(bytes_received >= 0, "negative bytes_received");
        NS_ASSERTION(content_length >= 0, "negative content_length");
        pListener->OnProgress(pConn->pURL, (PRUint32)bytes_received, 
                              (PRUint32)content_length);
        NS_RELEASE(pListener);
    }
}

PRIVATE void stub_GraphProgressDestroy(MWContext  *context, 
                                       URL_Struct *URL_s, 
                                       int32       content_length,
                                       int32       total_bytes_read)
{
    /*
     * XXX: Currently this function never calls OnProgress(...) because
     *      netlib calls FE_GraphProgressDestroy(...) after closing the
     *      stream...  So, OnStopBinding(...) has already been called and
     *      the nsConnectionInfo->pConsumer has been released and NULLed...
     */
    nsIStreamListener *pListener;

    if (URL_s->load_background)
        return;

    if (nsnull != (pListener = getStreamListener(URL_s))) {
        nsConnectionInfo *pConn = (nsConnectionInfo *) URL_s->fe_data;
        NS_ASSERTION(total_bytes_read >= 0, "negative total_bytes_read");
        NS_ASSERTION(content_length >= 0, "negative content_length");
        pListener->OnProgress(pConn->pURL, (PRUint32)total_bytes_read, 
                              (PRUint32)content_length);
        // XXX The comment above no longer applies, and this function does
        // get called...
        NS_RELEASE(pListener);
    }
}

PRIVATE void stub_Progress(MWContext *context, const char *msg)
{
    nsIStreamListener *pListener;

    if (nsnull != (pListener = getStreamListener(context->modular_data))) {
        nsConnectionInfo *pConn = 
            (nsConnectionInfo *) context->modular_data->fe_data;
        nsAutoString status(msg);
        pListener->OnStatus(pConn->pURL, status.GetUnicode());
        NS_RELEASE(pListener);
    } else {
        printf("%s\n", msg);
    }
}

PRIVATE void stub_AllConnectionsComplete(MWContext *context)
{
}

#define MAKE_FE_TYPES_PREFIX(func)	func##_t
#define MAKE_FE_FUNCS_TYPES
#include "mk_cx_fn.h"
#undef MAKE_FE_FUNCS_TYPES

#define stub_CreateNewDocWindow             (CreateNewDocWindow_t)stub_noop
#define stub_LayoutNewDocument              (LayoutNewDocument_t)stub_noop
#define stub_SetDocTitle                    (SetDocTitle_t)stub_noop
#define stub_FinishedLayout                 (FinishedLayout_t)stub_noop
#define stub_TranslateISOText               (TranslateISOText_t)stub_noop
#define stub_GetTextInfo                    (GetTextInfo_t)stub_noop
#define stub_MeasureText                    (MeasureText_t)stub_noop
#define stub_GetEmbedSize                   (GetEmbedSize_t)stub_noop
#define stub_GetJavaAppSize                 (GetJavaAppSize_t)stub_noop
#define stub_GetFormElementInfo             (GetFormElementInfo_t)stub_noop
#define stub_GetFormElementValue            (GetFormElementValue_t)stub_noop
#define stub_ResetFormElement               (ResetFormElement_t)stub_noop
#define stub_SetFormElementToggle           (SetFormElementToggle_t)stub_noop
#define stub_FreeFormElement                (FreeFormElement_t)stub_noop
#define stub_FreeImageElement               (FreeImageElement_t)stub_noop
#define stub_FreeEmbedElement               (FreeEmbedElement_t)stub_noop
#define stub_FreeBuiltinElement             (FreeBuiltinElement_t)stub_noop
#define stub_FreeJavaAppElement             (FreeJavaAppElement_t)stub_noop
#define stub_CreateEmbedWindow              (CreateEmbedWindow_t)stub_noop
#define stub_SaveEmbedWindow                (SaveEmbedWindow_t)stub_noop
#define stub_RestoreEmbedWindow             (RestoreEmbedWindow_t)stub_noop
#define stub_DestroyEmbedWindow             (DestroyEmbedWindow_t)stub_noop
#define stub_HideJavaAppElement             (HideJavaAppElement_t)stub_noop
#define stub_FreeEdgeElement                (FreeEdgeElement_t)stub_noop
#define stub_FormTextIsSubmit               (FormTextIsSubmit_t)stub_noop
#define stub_DisplaySubtext                 (DisplaySubtext_t)stub_noop
#define stub_DisplayText                    (DisplayText_t)stub_noop
#define stub_DisplayImage                   (DisplayImage_t)stub_noop
#define stub_DisplayEmbed                   (DisplayEmbed_t)stub_noop
#define stub_DisplayBuiltin                 (DisplayBuiltin_t)stub_noop
#define stub_DisplayJavaApp                 (DisplayJavaApp_t)stub_noop
#define stub_DisplaySubImage                (DisplaySubImage_t)stub_noop
#define stub_DisplayEdge                    (DisplayEdge_t)stub_noop
#define stub_DisplayTable                   (DisplayTable_t)stub_noop
#define stub_DisplayCell                    (DisplayCell_t)stub_noop
#define stub_InvalidateEntireTableOrCell    (InvalidateEntireTableOrCell_t)stub_noop
#define stub_DisplayAddRowOrColBorder       (DisplayAddRowOrColBorder_t)stub_noop
#define stub_DisplaySubDoc                  (DisplaySubDoc_t)stub_noop
#define stub_DisplayLineFeed                (DisplayLineFeed_t)stub_noop
#define stub_DisplayHR                      (DisplayHR_t)stub_noop
#define stub_DisplayBullet                  (DisplayBullet_t)stub_noop
#define stub_DisplayFormElement             (DisplayFormElement_t)stub_noop
#define stub_DisplayBorder                  (DisplayBorder_t)stub_noop
#define stub_UpdateEnableStates             (UpdateEnableStates_t)stub_noop
#define stub_DisplayFeedback                (DisplayFeedback_t)stub_noop
#define stub_ClearView                      (ClearView_t)stub_noop
#define stub_SetDocDimension                (SetDocDimension_t)stub_noop
#define stub_SetDocPosition                 (SetDocPosition_t)stub_noop
#define stub_GetDocPosition                 (GetDocPosition_t)stub_noop
#define stub_BeginPreSection                (BeginPreSection_t)stub_noop
#define stub_EndPreSection                  (EndPreSection_t)stub_noop
#define stub_SetProgressBarPercent          (SetProgressBarPercent_t)stub_noop
#define stub_SetBackgroundColor             (SetBackgroundColor_t)stub_noop
#define stub_Progress                       (Progress_t)stub_Progress
#define stub_Alert                          (Alert_t)stub_Alert
#define stub_SetCallNetlibAllTheTime        (SetCallNetlibAllTheTime_t)stub_noop
#define stub_ClearCallNetlibAllTheTime      (ClearCallNetlibAllTheTime_t)stub_noop
#define stub_GraphProgressInit              (GraphProgressInit_t)stub_GraphProgressInit
#define stub_GraphProgressDestroy           (GraphProgressDestroy_t)stub_GraphProgressDestroy
#define stub_GraphProgress                  (GraphProgress_t)stub_GraphProgress
#define stub_UseFancyFTP                    (UseFancyFTP_t)stub_noop
#define stub_UseFancyNewsgroupListing       (UseFancyNewsgroupListing_t)stub_noop
#define stub_FileSortMethod                 (FileSortMethod_t)stub_noop
#define stub_ShowAllNewsArticles            (ShowAllNewsArticles_t)stub_noop
#define stub_Confirm                        (Confirm_t)stub_Confirm
#define stub_CheckConfirm                   (CheckConfirm_t)stub_noop
#define stub_SelectDialog                   (SelectDialog_t)stub_noop
#define stub_Prompt                         (Prompt_t)stub_Prompt
#define stub_PromptWithCaption              (PromptWithCaption_t)stub_noop
#define stub_PromptUsernameAndPassword      (PromptUsernameAndPassword_t)stub_PromptUsernameAndPassword
#define stub_PromptPassword                 (PromptPassword_t)stub_PromptPassword
#define stub_EnableClicking                 (EnableClicking_t)stub_noop
#define stub_AllConnectionsComplete         (AllConnectionsComplete_t)stub_AllConnectionsComplete
#define stub_ImageSize                      (ImageSize_t)stub_noop
#define stub_ImageData                      (ImageData_t)stub_noop
#define stub_ImageIcon                      (ImageIcon_t)stub_noop
#define stub_ImageOnScreen                  (ImageOnScreen_t)stub_noop
#define stub_SetColormap                    (SetColormap_t)stub_noop
#ifdef LAYERS
#define stub_EraseBackground                (EraseBackground_t)stub_noop
#define stub_SetDrawable                    (SetDrawable_t)stub_noop
#define stub_GetTextFrame                   (GetTextFrame_t)stub_noop
#define stub_SetClipRegion                  (SetClipRegion_t)stub_noop
#define stub_SetOrigin	                    (SetOrigin_t)stub_noop
#define stub_GetOrigin	                    (GetOrigin_t)stub_noop
#define stub_GetTextFrame                   (GetTextFrame_t)stub_noop
#endif

#define stub_GetDefaultBackgroundColor      (GetDefaultBackgroundColor_t)stub_noop
#define stub_LoadFontResource               (LoadFontResource_t)stub_noop

#define stub_DrawJavaApp                    (DrawJavaApp_t)stub_noop
#define stub_HandleClippingView             (HandleClippingView_t)stub_noop

/* Just reuse the same set of context functions: */
ContextFuncs stub_context_funcs;

XP_List *stub_context_list = NULL;

MWContext *new_stub_context(URL_Struct *URL_s)
{
    static int funcsInitialized = 0;
    MWContext *context;

    if (!funcsInitialized) {
#define MAKE_FE_FUNCS_PREFIX(f) stub_##f
#define MAKE_FE_FUNCS_ASSIGN stub_context_funcs.
#include "mk_cx_fn.h"

        funcsInitialized = 1;

        stub_context_list = XP_ListNew();
    }

    context = (MWContext *)calloc(sizeof(struct MWContext_), 1);

    if (nsnull != context) {
        context->funcs        = &stub_context_funcs;
        context->type         = MWContextBrowser;
        context->modular_data = URL_s;
        context->ref_count    = 0;

        if (nsnull != stub_context_list) {
            XP_ListAddObjectToEnd(stub_context_list, context);
        }
    }

    return context;
}

void free_stub_context(MWContext *window_id)
{
    TRACEMSG(("Freeing stub context...\n"));

    if (window_id) {
        if (stub_context_list) {
            PRBool result;
        
            result = XP_ListRemoveObject(stub_context_list, window_id);
            PR_ASSERT(PR_TRUE == result);
        }
        free(window_id);
    }
}


extern "C" MWContext * XP_FindContextOfType (MWContext * context, 
                                             MWContextType type)
{
    MWContext *window_id = 0;

    /*
     * Return the context that was passed in if it is the correct type
     */
    if (nsnull != context) {
        if (context->type == type) {
            window_id = context;
        }
    }
    /*
     * Otherwise, the type MUST be a MWBrowserContext, since that is the
     * only type of context that is created...
     * 
     * Return the first stub context, since it is as good as any... :-)
     */
    else if (MWContextBrowser == type) {
        window_id = (MWContext *)XP_ListTopObject(stub_context_list);
    }

    return window_id;
}


/****************************************************************************/
/* End of MWContext Evil!!!                                                 */
/****************************************************************************/

PRIVATE
nsConnectionInfo *GetConnectionInfoFromStream(NET_StreamClass *stream)
{
    URL_Struct *URL_s = (URL_Struct *)stream->data_object;

    return (URL_s) ? (nsConnectionInfo *)URL_s->fe_data : NULL;
}


/*
 * Define a NET_StreamClass which pushes its data into an nsIStream
 * and fires off notifications through the nsIStreamListener interface
 */

PRIVATE
void stub_complete(NET_StreamClass *stream)
{
    URL_Struct *URL_s = (URL_Struct *)stream->data_object;
    nsConnectionInfo *pConn = GetConnectionInfoFromStream(stream);

    TRACEMSG(("+++ stream complete.\n"));

    /* Close the stream and remove it from the ConnectionInfo... */
    pConn->pNetStream->Close();
    NS_RELEASE(pConn->pNetStream);


    /* Notify the Data Consumer that the Binding has completed... */
    if (pConn->pConsumer) {
        nsAutoString status;
        pConn->pConsumer->OnStopBinding(pConn->pURL, NS_BINDING_SUCCEEDED, status.GetUnicode());
        pConn->mStatus = nsConnectionSucceeded;
    }

    /* Release the URL_Struct hanging off of the data_object */
    stream->data_object = NULL;
    NET_DropURLStruct(URL_s);
}

PRIVATE
void stub_abort(NET_StreamClass *stream, int status)
{
    URL_Struct *URL_s = (URL_Struct *)stream->data_object;
    nsConnectionInfo *pConn = GetConnectionInfoFromStream(stream);

    TRACEMSG(("+++ stream abort.  Status = %d\n", status));

    /* Close the stream and remove it from the ConnectionInfo... */
    pConn->pNetStream->Close();
    NS_RELEASE(pConn->pNetStream);

    /* Notify the Data Consumer that the Binding has completed... */
    /* 
     * XXX:  Currently, there is no difference between complete and
     * abort...
     */
    if (pConn->pConsumer) {
        nsAutoString status;

        pConn->pConsumer->OnStopBinding(pConn->pURL, NS_BINDING_ABORTED, status.GetUnicode());
        pConn->mStatus = nsConnectionAborted;
    }

    /* Release the URL_Struct hanging off of the data_object */
    stream->data_object = NULL;
    NET_DropURLStruct(URL_s);
}

PRIVATE
int stub_put_block(NET_StreamClass *stream, const char *buffer, int32 length)
{
    PRUint32 bytesWritten;
    nsresult errorCode;
    nsConnectionInfo *pConn = GetConnectionInfoFromStream(stream);

    TRACEMSG(("+++ stream put_block.  Length = %d\n", length));

    /*
     * XXX:  Sometimes put_block(...) will be called without having 
     *       called is_write_ready(...) first.  One case is when a stream
     *       is interrupted...  In this case, Netlib will call put_block(...)
     *       with the string "Transfer Interrupted!"
     */
    NS_ASSERTION(length >= 0, "negative length");
    errorCode = pConn->pNetStream->Write(buffer, (PRUint32)length, &bytesWritten);

    /* Abort the connection... */
    if (NS_BASE_STREAM_EOF == errorCode) {
        return -1;
    }

    if (pConn->pConsumer && (0 < bytesWritten)) {
        errorCode = pConn->pConsumer->OnDataAvailable(pConn->pURL, pConn->pNetStream, bytesWritten);
    }

	/* Abort the connection if an error occurred... */
    NS_ASSERTION(bytesWritten >= 0, "negative bytesWritten");
	if (NS_FAILED(errorCode) || ((int32)bytesWritten != length)) {
		return -1;
	}
    return 1;
}

PRIVATE
unsigned int stub_is_write_ready(NET_StreamClass *stream)
{
    nsresult errorCode;
    PRUint32 free_space = 0;
    URL_Struct *URL_s = (URL_Struct *)stream->data_object;
    nsConnectionInfo *pConn = GetConnectionInfoFromStream(stream);

    errorCode = pConn->pNetStream->GetAvailableSpace(&free_space);

    /*
     * If the InputStream has been closed...  Return 1 byte available so
     * Netlib will call put_block(...) one more time...
     */
    if (NS_BASE_STREAM_EOF == errorCode) {
        free_space = 1;
    }

    TRACEMSG(("+++ stream is_write_ready.  Returning %d\n", free_space));
    return (int)free_space;
}


extern "C" {

PUBLIC NET_StreamClass * 
NET_NGLayoutConverter(FO_Present_Types format_out,
                      void *converter_obj,
                      URL_Struct  *URL_s,
                      MWContext   *context);

/* 
 *Find a converter routine to create a stream and return the stream struct
 */
PUBLIC NET_StreamClass * 
NET_NGLayoutConverter(FO_Present_Types format_out,
                      void *converter_obj,
                      URL_Struct  *URL_s,
                      MWContext   *context)
{
    NET_StreamClass *stream = NULL;
    PRBool bSuccess = PR_TRUE;

    /* 
     * Only create a stream if an nsConnectionInfo object is 
     * available from the fe_data...
     */
    if (NULL != URL_s->fe_data) {
        stream = (NET_StreamClass *)calloc(sizeof(NET_StreamClass), 1);

        if (NULL != stream) {
            nsConnectionInfo *pConn;

            /*
             * Initialize the NET_StreamClass instance...
             */
            stream->name           = "Stub Stream";
            stream->window_id      = context;

            stream->complete       = stub_complete;
            stream->abort          = stub_abort;
            stream->put_block      = stub_put_block;
            stream->is_write_ready = stub_is_write_ready;

            /* 
             * Retrieve the nsConnectionInfo object from the fe_data field
             * of the URL_Struct...
             */
            pConn = (nsConnectionInfo *)URL_s->fe_data;

            /* Mark the connection as active... */
            pConn->mStatus = nsConnectionActive;

            /*
             * If the URL address has been rewritten by netlib then update
             * the cached info in the URL object...
             */
            if ((URL_s->address_modified) && (NULL != pConn->pURL)) {
                pConn->pURL->SetSpec(URL_s->address);
            }

            /* 
             * Create an Async stream unless a blocking stream is already
             * available in the ConnectionInfo...
             */
            if (NULL == pConn->pNetStream) {
                pConn->pNetStream = new nsAsyncStream(8192);
                if (NULL == pConn->pNetStream) {
                    free(stream);
                    return NULL;
                }
                NS_ADDREF(pConn->pNetStream);
            }

            /* Hang the URL_Struct off of the NET_StreamClass */
            NET_HoldURLStruct(URL_s);
            stream->data_object = URL_s;

#ifdef NOISY
            printf("+++ Created a stream for %s\n", URL_s->address);
#endif
            /* Notify the data consumer that Binding is beginning...*/
            if (nsnull != pConn->pConsumer) {
                nsresult rv;

                rv = pConn->pConsumer->OnStartBinding(pConn->pURL, URL_s->content_type);
                /*
                 * If OnStartBinding fails, then tear down the NET_StreamClass
                 * and release all references...
                 */
                if (NS_OK != rv) {
                    NET_DropURLStruct(URL_s);
                    free(stream);
                    stream = NULL;

                    /* 
                     * The NET_GetURL(...) exit_routine will clean up the
                     * nsConnectionInfo and associated objects.
                     */
                }
            }
        }
    }

    return stream;
}

} /* extern "C" */
