/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsAppShell.h"
#include "nsIWidget.h"
#include <Pt.h>

NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()  
{ 
  NS_INIT_REFCNT();
  mDispatchListener = 0;
  mEventBufferSz = sizeof( PhEvent_t ) + 1000;
  mEvent = (PhEvent_t*) malloc( mEventBufferSz );
  NS_ASSERTION( mEvent, "Out of memory" );
}



//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Create(int* argc, char ** argv)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener) 
{
  mDispatchListener = aDispatchListener;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

nsresult nsAppShell::Run()
{
  NS_ADDREF_THIS();

  nsresult        result = NS_OK;
  PhEvent_t       *event;
  unsigned short  locEventBufferSz = sizeof( PhEvent_t ) + 1000; /* initially */

  if( nsnull != ( event = (PhEvent_t*) malloc( locEventBufferSz )))
  {
    PRBool done = PR_FALSE;

    while(!done)
    {
      switch( PhEventNext( event, locEventBufferSz ))
      {
      case Ph_EVENT_MSG:
        PtEventHandler( event );
        if( mDispatchListener )
          mDispatchListener->AfterDispatch();
        break;

      case Ph_RESIZE_MSG:
        locEventBufferSz = PhGetMsgSize( event );
        if(( event = (PhEvent_t*) realloc( event, locEventBufferSz )) == nsnull )
        {
          result = 0; // Meaningful error code?
          done = PR_TRUE;
        }
        break;

      case -1:
        break;
      }
    }

    free( event );
  }


  Release();

  return result;
}


nsresult nsAppShell::GetNativeEvent(void *& aEvent, nsIWidget* aWidget, PRBool &aIsInWindow, PRBool &aIsMouseEvent)
{
  nsresult  result = NS_ERROR_FAILURE;
  PRBool   done = PR_FALSE;

  aIsInWindow = PR_FALSE;

  while(!done)
  {
    // This method uses the class event buffer, m_Event, and the class event
    // buffer size, m_EventBufferSz. If we receive a Ph_RESIZE_MSG event, we
    // try to realloc the buffer to the new size specified in the event, then
    // we wait for another "real" event.

    switch( PhEventNext( mEvent, mEventBufferSz ))
    {
    case Ph_EVENT_MSG:
      switch( mEvent->type )
      {
        case Ph_EV_BUT_PRESS:
        case Ph_EV_BUT_RELEASE:
        case Ph_EV_BUT_REPEAT:
        case Ph_EV_PTR_MOTION_BUTTON:
        case Ph_EV_PTR_MOTION_NOBUTTON:
          aIsMouseEvent = PR_TRUE;
          break;

        default:
          aIsMouseEvent = PR_FALSE;
          break;
      }

      aEvent = mEvent;

      if( nsnull != aWidget )
      {
        // Get Native Window for dialog window
        PtWidget_t* win;
        win = (PtWidget_t*)aWidget->GetNativeData(NS_NATIVE_WINDOW);

        aIsInWindow = PR_TRUE;
      }
      done = PR_TRUE;
      break;

    case Ph_RESIZE_MSG:
      mEventBufferSz = PhGetMsgSize( mEvent );
      if(( mEvent = (PhEvent_t*)realloc( mEvent, mEventBufferSz )) == nsnull )
      {
        done = PR_TRUE;
      }
      break;

    case -1:
      done = PR_TRUE;
      break;
    }
  }

  return result;
}

nsresult nsAppShell::DispatchNativeEvent(void * aEvent)
{
  PtEventHandler( (PhEvent_t*)aEvent );

// REVISIT - not sure if we're supposed to to this here:
//  if( mDispatchListener )
//    mDispatchListener->AfterDispatch();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

NS_METHOD nsAppShell::Exit()
{
  // REVISIT - How do we do this under Photon??? 
  // PtSendEventToWidget( m_window, quit_event );

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if( aDataType == NS_NATIVE_SHELL )
  {
    return nsnull;
  }
  return nsnull;
}


