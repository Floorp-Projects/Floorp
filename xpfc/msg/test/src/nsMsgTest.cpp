/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsMsgTest.h"
#include "nsxpfcCIID.h"
#include "nsIAppShell.h"
#include "nsMsgTestCIID.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRepository.h"
#include "nsMsgTestFactory.h"
#include "nsFont.h"
#include "nsITextWidget.h"
#include "nsITextAreaWidget.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nspr.h"
#include "plgetopt.h"
#include "nsIURL.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsCRT.h"
#include "jsapi.h"

#ifdef NS_UNIX
#include "Xm/Xm.h"
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"
#endif


static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kCTextAreaCID, NS_TEXTAREA_CID);
static NS_DEFINE_IID(kCTextFieldCID, NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);

/*
 * External refs
 */

#ifdef NS_UNIX
extern XtAppContext app_context;
extern Widget topLevel;
#endif

/*
 * Local Globals
 */

nsIMsgTest * gShell = nsnull;
nsIID kIXPCOMApplicationShellCID = NS_MSGTEST_CID ; 

#include "smtp.h"
#include "nsStream.h"
#include <time.h>
#include <stdio.h>

void SMTPTest_bdat( smtpSink_t *, int, const char * );
void SMTPTest_connect( smtpSink_t *, int, const char * );
void SMTPTest_data( smtpSink_t *, int, const char * );
void SMTPTest_ehlo( smtpSink_t *, int, const char * );
void SMTPTest_ehloComplete(smtpSink_t *);
void SMTPTest_expand( smtpSink_t *, int, const char * );
void SMTPTest_expandComplete(smtpSink_t *);
void SMTPTest_help( smtpSink_t *, int, const char * );
void SMTPTest_helpComplete(smtpSink_t *);
void SMTPTest_mailFrom( smtpSink_t *, int, const char * );
void SMTPTest_noop( smtpSink_t *, int, const char * );
void SMTPTest_quit( smtpSink_t *, int, const char * );
void SMTPTest_rcptTo( smtpSink_t *, int, const char * );
void SMTPTest_reset( smtpSink_t *, int, const char * );
void SMTPTest_send( smtpSink_t *, int, const char * );
void SMTPTest_sendCommand( smtpSink_t *, int, const char * );
void SMTPTest_sendCommandComplete(smtpSink_t *);
void SMTPTest_verify( smtpSink_t *, int, const char * );

/*Function prototype for settting sink pointers*/
void setSink( smtpSink_t * pSink );


/*
 * Useage
 */

static nsresult Usage(void)
{
  /* -s nsmail-2 -d netscape.com -f spider -t spider -h "This is a test" -m "This is subject" */
  return 1; 
} 


// All Application Must implement this function
nsresult NS_RegisterApplicationShellFactory()
{

  nsresult res = nsRepository::RegisterFactory(kIXPCOMApplicationShellCID,
                                               new nsMsgTestFactory(kIXPCOMApplicationShellCID),
                                               PR_FALSE) ;

  return res;
}


/*
 * nsMsgTest Definition
 */

nsMsgTest::nsMsgTest()
{
  NS_INIT_REFCNT();
  mShellInstance  = nsnull ;
  gShell = this;

  mServer = "";
  mFrom = "";
  mTo = "";
  mDomain = "";
  mMessage = "";
  mHeader = "";
}

/*
 * nsMsgTest dtor
 */
nsMsgTest::~nsMsgTest()
{
}

/*
 * nsISupports stuff
 */

nsresult nsMsgTest::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsISupports*)(nsIApplicationShell *)(this);                                        
  }
  else if(aIID.Equals(kIXPCOMApplicationShellCID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIMsgTest*)(this);                                        
  }
  else if(aIID.Equals(kIAppShellIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIAppShell*)(this);                                        
  }
  else if(aIID.Equals(kIStreamListenerIID)) {  //do nsIContentSink base class...
    *aInstancePtr = (nsIStreamListener*)(this);                                        
  }  
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(nsMsgTest)
NS_IMPL_RELEASE(nsMsgTest)

/*
 * Init
 */

nsresult nsMsgTest::Init()
{

  /*
   * Register class factrories needed for application
   */

  RegisterFactories() ;

  nsresult res = nsApplicationManager::GetShellInstance(this, &mShellInstance) ;

  res = ParseCommandLine();

  if (NS_OK != res)
    return res;

    char * server   = mServer.ToNewCString();
    char * from     = mFrom.ToNewCString();
    char * to       = mTo.ToNewCString();
    char * domain   = mDomain.ToNewCString();
    char * message  = mMessage.ToNewCString();
    char * header   = mHeader.ToNewCString();

    nsString data("Subject: ");
    data += header;
    data += "\r\n";
    data += message;

    delete message;
    message  = data.ToNewCString();

    int l_nReturn;
    nsmail_inputstream_t * l_inputStream;
    smtpClient_t * pClient = NULL;
    smtpSink_t * pSink = NULL;

    buf_inputStream_create (message, nsCRT::strlen(message), &l_inputStream);

    /*Initialize the response sink.*/
    l_nReturn = smtpSink_initialize( &pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Set the function pointers on the response sink.*/
    setSink( pSink );

    /*Initialize the client passing in the response sink.*/
    l_nReturn = smtp_initialize( &pClient, pSink );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Set the internal buffer chunk size.*/
    l_nReturn = smtp_setChunkSize( pClient, 1048576 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Connect to the SMTP server.*/
    l_nReturn = smtp_connect( pClient, server, 25 );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the EHLO command passing in the domain name.*/
    l_nReturn = smtp_ehlo( pClient, domain );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the MAIL FROM command.*/
    l_nReturn = smtp_mailFrom( pClient, from, NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the RCPT TO command.*/
    l_nReturn = smtp_rcptTo( pClient, to, NULL );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the DATA command.*/

    l_nReturn = smtp_data( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /* Send the message.*/
    l_nReturn = smtp_sendStream( pClient, l_inputStream );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the EXPN command.*/
    l_nReturn = smtp_expand( pClient, from );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the HELP command.*/
    l_nReturn = smtp_help( pClient, from );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the NOOP command.*/
    l_nReturn = smtp_noop( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the RSET command.*/
    l_nReturn = smtp_reset( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send the VRFY command.*/
    l_nReturn = smtp_verify( pClient, from );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    /*Send a generic command to the server.*/
    l_nReturn = smtp_sendCommand( pClient, "HELP help" );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_quit( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    l_nReturn = smtp_processResponses( pClient );

    if ( l_nReturn != NSMAIL_OK )
    {
        return l_nReturn;
    }

    nsStream_free (l_inputStream);

    /*Free the client structure.*/
    smtp_free( &pClient );
    /*Free the sink structure.*/
    smtpSink_free( &pSink );

  delete server;
  delete from;
  delete to;
  delete domain;
  delete message;
  delete header;

  return 1 ;
}


nsresult nsMsgTest::Create(int* argc, char ** argv)
{
  return NS_OK;
}

nsresult nsMsgTest::Exit()
{
  return NS_OK;
}

nsresult nsMsgTest::Run()
{
  mShellInstance->Run();
  return NS_OK;
}

nsresult nsMsgTest::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  return NS_OK;
}

void* nsMsgTest::GetNativeData(PRUint32 aDataType)
{
#ifdef XP_UNIX
  if (aDataType == NS_NATIVE_SHELL)
    return topLevel;

  return nsnull;
#else
  return (mShellInstance->GetApplicationWidget());
#endif

}

nsresult nsMsgTest::RegisterFactories()
{
#ifdef NS_WIN32
  #define XPFC_DLL  "xpfc10.dll"
#else
  #define XPFC_DLL "libxpfc10.so"
#endif

  // register graphics classes
  static NS_DEFINE_IID(kCXPFCCanvasCID, NS_XPFC_CANVAS_CID);

  nsRepository::RegisterFactory(kCXPFCCanvasCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);
  static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);

  nsRepository::RegisterFactory(kCVectorCID, XPFC_DLL, PR_FALSE, PR_FALSE);
  nsRepository::RegisterFactory(kCVectorIteratorCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  static NS_DEFINE_IID(kCXPFCObserverManagerCID,   NS_XPFC_OBSERVERMANAGER_CID);
  nsRepository::RegisterFactory(kCXPFCObserverManagerCID, XPFC_DLL, PR_FALSE, PR_FALSE);

  return NS_OK;
}

nsresult nsMsgTest::GetWebViewerContainer(nsIWebViewerContainer ** aWebViewerContainer)
{
  return NS_OK;
}


nsEventStatus nsMsgTest::HandleEvent(nsGUIEvent *aEvent)
{

    nsEventStatus result = nsEventStatus_eConsumeNoDefault;

    switch(aEvent->message) {

        case NS_CREATE:
        {
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;

        case NS_SIZE:
        {
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;

        case NS_DESTROY:
        {
          mShellInstance->ExitApplication() ;
          return nsEventStatus_eConsumeNoDefault;
        }
        break ;
    }

    return nsEventStatus_eIgnore; 
}


nsresult nsMsgTest::StartCommandServer()
{
  return NS_OK;
}

nsresult nsMsgTest::ReceiveCommand(nsString& aCommand, nsString& aReply)
{
  return NS_OK;
}







/*Function to set the sink pointers.*/
void setSink( smtpSink_t * pSink )
{
    pSink->bdat = SMTPTest_bdat;
    pSink->connect = SMTPTest_connect;
    pSink->data = SMTPTest_data;
    pSink->ehlo = SMTPTest_ehlo;
    pSink->ehloComplete = SMTPTest_ehloComplete;
    pSink->expand = SMTPTest_expand;
    pSink->expandComplete = SMTPTest_expandComplete;
    pSink->help = SMTPTest_help;
    pSink->helpComplete = SMTPTest_helpComplete;
    pSink->mailFrom = SMTPTest_mailFrom;
    pSink->noop = SMTPTest_noop;
    pSink->quit = SMTPTest_quit;
    pSink->rcptTo = SMTPTest_rcptTo;
    pSink->reset = SMTPTest_reset;
    pSink->send = SMTPTest_send;
    pSink->sendCommand = SMTPTest_sendCommand;
    pSink->sendCommandComplete = SMTPTest_sendCommandComplete;
    pSink->verify = SMTPTest_verify;
}



/*Notification for the response to the BDAT command.*/
void SMTPTest_bdat( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the connection to the server.*/
void SMTPTest_connect( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the DATA command.*/
void SMTPTest_data( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the EHLO command.*/
void SMTPTest_ehlo( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of the EHLO command.*/
void SMTPTest_ehloComplete(smtpSink_t * in_pSink)
{
    printf( "EHLO complete\n" );
}

/*Notification for the response to a server error.*/
void SMTPTest_error(smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the EXPN command.*/
void SMTPTest_expand( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of the EXPN command.*/
void SMTPTest_expandComplete(smtpSink_t * in_pSink)
{
    printf( "EXPAND complete\n" );
}

/*Notification for the response to the HELP command.*/
void SMTPTest_help( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of the HELP command.*/
void SMTPTest_helpComplete(smtpSink_t * in_pSink)
{
    printf( "HELP complete\n" );
}

/*Notification for the response to the MAIL FROM command.*/
void SMTPTest_mailFrom( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the NOOP command.*/
void SMTPTest_noop( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the QUIT command.*/
void SMTPTest_quit( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the RCPT TO command.*/
void SMTPTest_rcptTo( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to the RSET command.*/
void SMTPTest_reset( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to sending the message.*/
void SMTPTest_send( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the response to sending a generic command.*/
void SMTPTest_sendCommand( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}

/*Notification for the completion of send a generic command.*/
void SMTPTest_sendCommandComplete(smtpSink_t * in_pSink)
{
    printf( "SENDCOMMAND complete\n" );
}

/*Notification for the response to the VRFY command.*/
void SMTPTest_verify( smtpSink_t * in_pSink, int in_responseCode, const char * in_responseMessage )
{
    printf( "%d %s\n", in_responseCode, in_responseMessage );
}



nsresult nsMsgTest::ParseCommandLine()
{
  PLOptStatus os;
  PLOptState *opt;

  PRUint32 count = 0;

  mShellInstance->GetCommandLineOptions(&opt,"h:s:d:f:t:m:");

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
  {
    if (PL_OPT_BAD == os) 
      continue;

    switch (opt->option)
    {    
      case 'h':
      {
        mHeader = opt->value;
        count++;
      }
      break;

      case 's':
      {
        mServer = opt->value;
        count++;
      }
      break;

      case 'd':
      {
        mDomain = opt->value;
        count++;
      }
      break;

      case 'f':
      {
        mFrom = opt->value;
        count++;
      }
      break;

      case 't':
      {
        mTo = opt->value;
        count++;
      }
      break;

      case 'm':
      {
        mMessage = opt->value;
        count++;
      }
      break;
      
      case '?':  /* confused */
      default:
        return Usage();
    }
  }

  if (count < 6)
    return Usage();

  return NS_OK;
}
