/* -*- Mode: C++; tab-width: 2; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Ashford <arougthopher@lizardland.net>
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDebug.h"
#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsIRegion.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include "prtime.h"
#include "nsReadableUtils.h"
#include "nsVoidArray.h"

#include <Application.h>
#include <InterfaceDefs.h>
#include <Region.h>
#include <Debug.h>
#include <MenuBar.h>
#include <ScrollBar.h>
#include <app/Message.h>
#include <app/MessageRunner.h>
#include <support/String.h>
#include <Screen.h>

#include <nsBeOSCursors.h>

#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"

// See comments in nsWindow.h as to why we override these calls from nsBaseWidget
NS_IMPL_THREADSAFE_ADDREF(nsWindow)
NS_IMPL_THREADSAFE_RELEASE(nsWindow)

static NS_DEFINE_IID(kIWidgetIID,       NS_IWIDGET_IID);

//-------------------------------------------------------------------------
// Global Definitions
//-------------------------------------------------------------------------

// Rollup Listener - static variable defintions
static nsIRollupListener * gRollupListener           = nsnull;
static nsIWidget         * gRollupWidget             = nsnull;
static PRBool              gRollupConsumeRollupEvent = PR_FALSE;
// Preserve Mozilla's shortcut for closing 
static PRBool              gGotQuitShortcut          = PR_FALSE;
// Tracking last activated BWindow
static BWindow           * gLastActiveWindow = NULL;

// BCursor objects can't be created until they are used.  Some mozilla utilities, 
// such as regxpcom, do not create a BApplication object, and therefor fail to run.,
// since a BCursor requires a vaild BApplication (see Bug#129964).  But, we still want
// to cache them for performance.  Currently, there are 18 cursors available;
static nsVoidArray		gCursorArray(18);
// Used in contrain position.  Specifies how much of a window must remain on screen
#define kWindowPositionSlop 20
// BeOS does not provide this information, so we must hard-code it
#define kWindowBorderWidth 5
#define kWindowTitleBarHeight 24

// TODO: make a #def for using OutLine view or not (see TODO below)


//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsBaseWidget()
{
	rgb_color	back = ui_color(B_PANEL_BACKGROUND_COLOR);

	mView               = 0;
	mBackground         = NS_RGB(back.red, back.green, back.blue);
	mForeground         = NS_RGB(0x00,0x00,0x00);
	mIsDestroying       = PR_FALSE;
	mOnDestroyCalled    = PR_FALSE;
	mPreferredWidth     = 0;
	mPreferredHeight    = 0;
	mFont               = nsnull;
	mIsVisible          = PR_FALSE;
	mWindowType         = eWindowType_child;
	mBorderStyle        = eBorderStyle_default;
	mBorderlessParent   = 0;
	mEnabled            = PR_TRUE;
	mJustGotActivate    = PR_FALSE;
	mJustGotDeactivate  = PR_FALSE;
}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
	mIsDestroying = PR_TRUE;

	// If the widget was released without calling Destroy() then the native
	// window still exists, and we need to destroy it
	if (NULL != mView) 
	{
		Destroy();
	}

	// FIXME: Check if this is really needed or not, if not, remove it
	//XXX Temporary: Should not be caching the font
	delete mFont;
}


//-------------------------------------------------------------------------
//
// Default for height modification is to do nothing
//
//-------------------------------------------------------------------------

PRInt32 nsWindow::GetHeight(PRInt32 aProposedHeight)
{
	return(aProposedHeight);
}

NS_METHOD nsWindow::BeginResizingChildren(void)
{
	if(mView && mView->LockLooper())
	{
		//It appears that only effective method in Be API to avoid invalidation chain overhead
		//while performing action on BView's children is to use B_DRAW_ON_CHILDREN flag
		//together with implementation of BView::DrawAfterChildren() hook
		mView->SetFlags(mView->Flags() | B_DRAW_ON_CHILDREN);
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
	if(mView && mView->LockLooper())
	{
		mView->SetFlags(mView->Flags() & ~B_DRAW_ON_CHILDREN);
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
	BPoint	point;
	point.x = aOldRect.x;
	point.y = aOldRect.y;
	if(mView && mView->LockLooper())
	{
		mView->ConvertToScreen(&point);
		mView->UnlockLooper();
	}
	aNewRect.x = nscoord(point.x);
	aNewRect.y = nscoord(point.y);
	aNewRect.width = aOldRect.width;
	aNewRect.height = aOldRect.height;
	return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
	BPoint	point;
	point.x = aOldRect.x;
	point.y = aOldRect.y;
	if(mView && mView->LockLooper())
	{
		mView->ConvertFromScreen(&point);
		mView->UnlockLooper();
	}
	aNewRect.x = nscoord(point.x);
	aNewRect.y = nscoord(point.y);
	aNewRect.width = aOldRect.width;
	aNewRect.height = aOldRect.height;
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Convert nsEventStatus value to a windows boolean
//
//-------------------------------------------------------------------------

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
	switch(aStatus) 
	{
		case nsEventStatus_eIgnore:
			return PR_FALSE;
		case nsEventStatus_eConsumeNoDefault:
			return PR_TRUE;
		case nsEventStatus_eConsumeDoDefault:
			return PR_FALSE;
		default:
			NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
			break;
	}
	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------
void nsWindow::InitEvent(nsGUIEvent& event, nsPoint* aPoint)
{
	NS_ADDREF(event.widget);

	if (nsnull == aPoint) // use the point from the event
	{
		// get the message position in client coordinates and in twips
		event.point.x = 0;
		event.point.y = 0;
	}
	else // use the point override if provided
	{
		event.point.x = aPoint->x;
		event.point.y = aPoint->y;
	}
	event.time = PR_IntervalNow();
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
	aStatus = nsEventStatus_eIgnore;

	nsCOMPtr <nsIWidget> mWidget = event->widget;

	if (mEventCallback)
		aStatus = (*mEventCallback)(event);

	if ((aStatus != nsEventStatus_eIgnore) && (mEventListener))
		aStatus = mEventListener->ProcessEvent(*event);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Dispatch Window Event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
	nsEventStatus status;
	DispatchEvent(event, status);
	return ConvertStatus(status);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg)
{
	nsGUIEvent event(aMsg, this);
	InitEvent(event);

	PRBool result = DispatchWindowEvent(&event);
	NS_RELEASE(event.widget);
	return result;
}

//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
nsresult nsWindow::StandardWindowCreate(nsIWidget *aParent,
                                        const nsRect &aRect,
                                        EVENT_CALLBACK aHandleEventFunction,
                                        nsIDeviceContext *aContext,
                                        nsIAppShell *aAppShell,
                                        nsIToolkit *aToolkit,
                                        nsWidgetInitData *aInitData,
                                        nsNativeWidget aNativeParent)
{
	nsIWidget *baseParent = aInitData &&
	                        (aInitData->mWindowType == eWindowType_dialog ||
	                         aInitData->mWindowType == eWindowType_toplevel ||
	                         aInitData->mWindowType == eWindowType_invisible) ?
	                        nsnull : aParent;

	mIsTopWidgetWindow = (nsnull == baseParent);

	BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
	           aAppShell, aToolkit, aInitData);

	// Switch to the "main gui thread" if necessary... This method must
	// be executed on the "gui thread"...
	//

	nsToolkit* toolkit = (nsToolkit *)mToolkit;
	if (toolkit)
	{
		if (!toolkit->IsGuiThread())
		{
			uint32 args[7];
			args[0] = (uint32)aParent;
			args[1] = (uint32)&aRect;
			args[2] = (uint32)aHandleEventFunction;
			args[3] = (uint32)aContext;
			args[4] = (uint32)aAppShell;
			args[5] = (uint32)aToolkit;
			args[6] = (uint32)aInitData;

			if (nsnull != aParent)
			{
				// nsIWidget parent dispatch
				MethodInfo info(this, this, nsWindow::CREATE, 7, args);
				toolkit->CallMethod(&info);
				return NS_OK;
			}
			else
			{
				// Native parent dispatch
				MethodInfo info(this, this, nsWindow::CREATE_NATIVE, 5, args);
				toolkit->CallMethod(&info);
				return NS_OK;
			}
		}
	}

	BView *parent;
	if (nsnull != aParent) // has a nsIWidget parent
	{
		parent = ((aParent) ? (BView *)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
	}
	else // has a nsNative parent
	{
		parent = (BView *)aNativeParent;
	}

	if (nsnull != aInitData)
	{
		SetWindowType(aInitData->mWindowType);
		SetBorderStyle(aInitData->mBorderStyle);
	}

	// Only popups have mBorderlessParents
	mBorderlessParent = NULL;

	mView = CreateBeOSView();
	if(mView)
	{
#ifdef MOZ_DEBUG_WINDOW_CREATE
		printf("nsWindow::StandardWindowCreate : type = ");
#endif
		if (mWindowType == eWindowType_child)
		{
#ifdef MOZ_DEBUG_WINDOW_CREATE
			printf("child window\n");
#endif			
			// create view only
			bool mustunlock=false;

			if(parent->LockLooper())
			{
				mustunlock = true;
			}

			parent->AddChild(mView);
			mView->MoveTo(aRect.x, aRect.y);
			mView->ResizeTo(aRect.width-1, GetHeight(aRect.height)-1);

			if(mustunlock)
			{
				parent->UnlockLooper();
			}

		}
		else // if eWindowType_Child
		{
			nsWindowBeOS *w;
			BRect winrect = BRect(aRect.x, aRect.y, aRect.x + aRect.width - 1, aRect.y + aRect.height - 1);
			window_look look = B_TITLED_WINDOW_LOOK;
			window_feel feel = B_NORMAL_WINDOW_FEEL;
			// Set all windows to use outline resize, since currently, the redraw of the window during resize
			// is very "choppy"
			uint32 flags = B_ASYNCHRONOUS_CONTROLS | B_FRAME_EVENTS;
			
#ifdef MOZ_DEBUG_WINDOW_CREATE
			printf("%s\n", (mWindowType == eWindowType_popup) ? "popup" :
			               (mWindowType == eWindowType_dialog) ? "dialog" :
			               (mWindowType == eWindowType_toplevel) ? "toplevel" : "unknown");
#endif					
			switch (mWindowType)
			{
				case eWindowType_popup:
				{
					mBorderlessParent = parent;
					flags |= B_NOT_CLOSABLE | B_AVOID_FOCUS | B_NO_WORKSPACE_ACTIVATION;
					look = B_NO_BORDER_WINDOW_LOOK;
					break;
				}
					
				case eWindowType_dialog:
				{
					if (mBorderStyle == eBorderStyle_default)
					{
						flags |= B_NOT_ZOOMABLE;
					}
					// don't break here
				}
				case eWindowType_toplevel:
				case eWindowType_invisible:
				{
					// This was never documented, so I'm not sure why we did it
					//winrect.OffsetBy( 10, 30 );

#ifdef MOZ_DEBUG_WINDOW_CREATE
					printf("\tBorder Style : ");
#endif					
					// Set the border style(s)
					switch (mBorderStyle)
					{
						case eBorderStyle_default:
						{
#ifdef MOZ_DEBUG_WINDOW_CREATE
							printf("default\n");
#endif							
							break;
						}
						case eBorderStyle_all:
						{
#ifdef MOZ_DEBUG_WINDOW_CREATE
							printf("all\n");
#endif							
							break;
						}
						
						case eBorderStyle_none:
						{
#ifdef MOZ_DEBUG_WINDOW_CREATE
							printf("none\n");
#endif							
							look = B_NO_BORDER_WINDOW_LOOK;
							break;
						}	
						
						default:
						{
							if (!(mBorderStyle & eBorderStyle_resizeh))
							{
#ifdef MOZ_DEBUG_WINDOW_CREATE
								printf("no_resize ");
#endif							
								flags |= B_NOT_RESIZABLE;
							}
							if (!(mBorderStyle & eBorderStyle_title))
							{
#ifdef MOZ_DEBUG_WINDOW_CREATE
								printf("no_titlebar ");
#endif							
								look = B_BORDERED_WINDOW_LOOK;
							}
							if (!(mBorderStyle & eBorderStyle_minimize) || !(mBorderStyle & eBorderStyle_maximize))
							{
#ifdef MOZ_DEBUG_WINDOW_CREATE
								printf("no_zoom ");
#endif							
								flags |= B_NOT_ZOOMABLE;
							}
							if (!(mBorderStyle & eBorderStyle_close))
							{
#ifdef MOZ_DEBUG_WINDOW_CREATE
								printf("no_close ");
#endif							
								flags |= B_NOT_CLOSABLE;
							}
#ifdef MOZ_DEBUG_WINDOW_CREATE
								printf("\n");
#endif							
						}
					} // case (mBorderStyle)
					break;
				} // eWindowType_toplevel
					
				default:
				{
					NS_ASSERTION(false, "Unhandled Window Type in nsWindow::StandardWindowCreate!");
					break;
				}
			} // case (mWindowType)
			
			w = new nsWindowBeOS(this, winrect, "", look, feel, flags);
			if(w)
			{
				w->AddChild(mView);

				// FIXME: we have to use the window size because
				// the window might not like sizes less then 30x30 or something like that
				mView->MoveTo(0, 0);
				mView->ResizeTo(w->Bounds().Width(), w->Bounds().Height());
				mView->SetResizingMode(B_FOLLOW_ALL);
				w->Hide();
				w->Show();
			}
		} // if eWindowType_Child

		// call the event callback to notify about creation
		DispatchStandardEvent(NS_CREATE);
		return(NS_OK);
	} // if mView

	return NS_ERROR_OUT_OF_MEMORY;
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Create(nsIWidget *aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
	return(StandardWindowCreate(aParent, aRect, aHandleEventFunction,
	                            aContext, aAppShell, aToolkit, aInitData,
	                            nsnull));
}


//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::Create(nsNativeWidget aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
	return(StandardWindowCreate(nsnull, aRect, aHandleEventFunction,
	                            aContext, aAppShell, aToolkit, aInitData,
	                            aParent));
}

BView *nsWindow::CreateBeOSView()
{
	return new nsViewBeOS(this, BRect(0,0,0,0), "", 0, B_WILL_DRAW | B_FRAME_EVENTS);
}

//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Destroy()
{
	// Switch to the "main gui thread" if necessary... This method must
	// be executed on the "gui thread"...
	nsToolkit* toolkit = (nsToolkit *)mToolkit;
	if (toolkit != nsnull && !toolkit->IsGuiThread())
	{
		MethodInfo info(this, this, nsWindow::DESTROY);
		toolkit->CallMethod(&info);
		return NS_ERROR_FAILURE;
	}

	//our windows can be subclassed by
	//others and these namless, faceless others
	//may not let us know about WM_DESTROY. so,
	//if OnDestroy() didn't get called, just call
	//it now. MMP
	if (PR_FALSE == mOnDestroyCalled)
		OnDestroy();
		
	// destroy the BView
	if (mView)
	{
		// prevent the widget from causing additional events
		mEventCallback = nsnull;

		if(mView->LockLooper())
		{
			// destroy from inside
			BWindow	*w = mView->Window();
			w->Sync();
			if(mView->Parent())
			{
				mView->Parent()->RemoveChild(mView);
				if (eWindowType_child != mWindowType)
					w->Quit();
				else
					w->Unlock();
			}
			else
			{
				w->RemoveChild(mView);
				w->Quit();
			}
		}

		// window is already gone
		while(mView->ChildAt(0))
			mView->RemoveChild(mView->ChildAt(0));
		delete mView;
		mView = NULL;
		
		// Ok, now tell the nsBaseWidget class to clean up what it needs to
		if (!mIsDestroying)
		{
			nsBaseWidget::Destroy();
		}
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
	nsIWidget	*widget = 0;
	BView		*parent;
	if(mView && (parent = mView->Parent()) != 0)
	{
		nsIWidgetStore *ws = dynamic_cast<nsIWidgetStore *>(parent);
		NS_ASSERTION(ws != 0, "view must be derived from nsIWidgetStore");
		if((widget = ws->GetMozillaWidget()) != 0)
		{
			// If the widget is in the process of being destroyed then
			// do NOT return it
			if(((nsWindow *)widget)->mIsDestroying)
				widget = 0;
			else
				NS_ADDREF(widget);
		}
	}

	return widget;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
	if (!mEnabled)
		return NS_OK;
	if (mView && mView->LockLooper())
	{
		switch (mWindowType)
		{
			case eWindowType_popup:
			{
				if(PR_FALSE == bState)
				{
					// XXX BWindow::Hide() is needed ONLY for popups. No need to hide views for popups
					if (mView->Window() && !mView->Window()->IsHidden())
						mView->Window()->Hide();
				}
				else
				{
					if (mView->Window() && mView->Window()->IsHidden())
						mView->Window()->Show();
				}
				break;
			}

			case eWindowType_child:
			{
				// XXX No BWindow deals for children
				if(PR_FALSE == bState)
				{
					if (!mView->IsHidden())
						mView->Hide();
				}
				else
				{
					if (mView->IsHidden())
						mView->Show();              
				}
				break;
			}

			case eWindowType_dialog:
			case eWindowType_toplevel:
			{
				if (bState == PR_FALSE)
				{
					if (mView->Window() && !mView->Window()->IsHidden())
						mView->Window()->Hide();
				}
				else
				{
					if (mView->Window() && mView->Window()->IsHidden())
						mView->Window()->Show();
				}
				break;
			}
			
			default: // toplevel and dialog
			{
				NS_ASSERTION(false, "Unhandled Window Type in nsWindow::Show()!");
				break;
			}
		} //end switch	

		mView->UnlockLooper();
		mIsVisible = bState;	
	}
	return NS_OK;
}
//-------------------------------------------------------------------------
// Set/unset mouse capture
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
	if (mView && mView->LockLooper())
	{
		if (PR_TRUE == aCapture)
			mView->SetEventMask(B_POINTER_EVENTS);
		else
			mView->SetEventMask(0);
		mView->UnlockLooper();
	}
	return NS_OK;
}
//-------------------------------------------------------------------------
// Capture Roolup Events
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
{
	if (!mEnabled)
		return NS_OK;
		
	if (aDoCapture) 
	{
		// we haven't bothered carrying a weak reference to gRollupWidget because
		// we believe lifespan is properly scoped. this next assertion helps 
		// assure that remains true.
		NS_ASSERTION(!gRollupWidget, "rollup widget reassigned before release");
		gRollupConsumeRollupEvent = aConsumeRollupEvent;
		NS_IF_RELEASE(gRollupListener);
		NS_IF_RELEASE(gRollupWidget);
		gRollupListener = aListener;
		NS_ADDREF(aListener);
		gRollupWidget = this;
		NS_ADDREF(this);
	} 
	else 
	{
		NS_IF_RELEASE(gRollupListener);
		NS_IF_RELEASE(gRollupWidget);
	}

	return NS_OK;
}

//-------------------------------------------------------------------------
// Check if event happened inside the given nsWindow
//-------------------------------------------------------------------------
PRBool nsWindow::EventIsInsideWindow(nsWindow* aWindow, nsPoint pos)
{
	BRect r;
	BView *view = (BView *) aWindow->GetNativeData(NS_NATIVE_WIDGET);
	if (view && view->LockLooper() ) {
		r = view->ConvertToScreen(view->Bounds());
		view->UnlockLooper();
	}

	if (pos.x < r.left || pos.x > r.right ||
	        pos.y < r.top || pos.y > r.bottom) {
		return PR_FALSE;
	}

	return PR_TRUE;
}

//-------------------------------------------------------------------------
// DealWithPopups
//
// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
//-------------------------------------------------------------------------
PRBool
nsWindow::DealWithPopups(uint32 methodID, nsPoint pos)
{
	if (gRollupListener && gRollupWidget) 
	{
		// Rollup if the event is outside the popup.
		PRBool rollup = !nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget, pos);

		// If we're dealing with menus, we probably have submenus and we don't
		// want to rollup if the click is in a parent menu of the current submenu.
		if (rollup) 
		{
			nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
			if ( menuRollup ) 
			{
				nsCOMPtr<nsISupportsArray> widgetChain;
				menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
				if ( widgetChain ) 
				{
					PRUint32 count = 0;
					widgetChain->Count(&count);
					for ( PRUint32 i = 0; i < count; ++i ) 
					{
						nsCOMPtr<nsISupports> genericWidget;
						widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
						nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
						if ( widget ) 
						{
							nsIWidget* temp = widget.get();
							if ( nsWindow::EventIsInsideWindow((nsWindow*)temp, pos) ) 
							{
								rollup = PR_FALSE;
								break;
							}
						}
					} // foreach parent menu widget
				} // if widgetChain
			} // if rollup listener knows about menus
		} // if rollup

		if (rollup) 
		{
			gRollupListener->Rollup();

			if (gRollupConsumeRollupEvent) 
			{
				return PR_TRUE;
			}
		}
	} // if rollup listeners registered

	return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// IsVisible
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
	bState = mIsVisible;
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Sanity check potential move coordinates
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
	if (mIsTopWidgetWindow && mView->Window()) 
	{
		BScreen screen;
		// If no valid screen, just return
		if (! screen.IsValid()) return NS_OK;
		
		BRect screen_rect = screen.Frame();
		BRect win_bounds = mView->Window()->Frame();

#ifdef DEBUG_CONSTRAIN_POSITION
		printf("ConstrainPosition: allowSlop=%s, x=%d, y=%d\n\tScreen :", (aAllowSlop?"T":"F"),*aX,*aY);
		screen_rect.PrintToStream();
		printf("\tWindow: ");
		win_bounds.PrintToStream();
#endif
		
		if (aAllowSlop) 
		{
			if (*aX < kWindowPositionSlop - win_bounds.IntegerWidth() + kWindowBorderWidth)
				*aX = kWindowPositionSlop - win_bounds.IntegerWidth() + kWindowBorderWidth;
			else if (*aX > screen_rect.IntegerWidth() - kWindowPositionSlop - kWindowBorderWidth)
				*aX = screen_rect.IntegerWidth() - kWindowPositionSlop - kWindowBorderWidth;
				
			if (*aY < kWindowPositionSlop - win_bounds.IntegerHeight() + kWindowTitleBarHeight)
				*aY = kWindowPositionSlop - win_bounds.IntegerHeight() + kWindowTitleBarHeight;
			else if (*aY > screen_rect.IntegerHeight() - kWindowPositionSlop - kWindowBorderWidth)
				*aY = screen_rect.IntegerHeight() - kWindowPositionSlop - kWindowBorderWidth;
				
		} 
		else 
		{
			
			if (*aX < kWindowBorderWidth)
				*aX = kWindowBorderWidth;
			else if (*aX > screen_rect.IntegerWidth() - win_bounds.IntegerWidth() - kWindowBorderWidth)
				*aX = screen_rect.IntegerWidth() - win_bounds.IntegerWidth() - kWindowBorderWidth;
				
			if (*aY < kWindowTitleBarHeight)
				*aY = kWindowTitleBarHeight;
			else if (*aY > screen_rect.IntegerHeight() - win_bounds.IntegerHeight() - kWindowBorderWidth)
				*aY = screen_rect.IntegerHeight() - win_bounds.IntegerHeight() - kWindowBorderWidth;
		}
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
nsresult nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
	// Only perform this check for non-popup windows, since the positioning can
	// in fact change even when the x/y do not.  We always need to perform the
	// check. See bug #97805 for details.
	if (mWindowType != eWindowType_popup && (mBounds.x == aX) && (mBounds.y == aY))
	{
		// Nothing to do, since it is already positioned correctly.
		return NS_OK;    
	}
	
	bool mustunlock = false;
	bool havewindow = false;

	// Set cached value for lightweight and printing
	mBounds.x = aX;
	mBounds.y = aY;

	if(mView)
	{
		// Popup window should be placed relative to its parent window.
		if (mWindowType == eWindowType_popup && mBorderlessParent) 
		{
			BWindow *parentwindow = mBorderlessParent->Window();
			if (parentwindow && parentwindow->Lock()) 
			{
				BPoint p = mBorderlessParent->ConvertToScreen(BPoint(aX,aY));
				aX = (nscoord)p.x;
				aY = (nscoord)p.y;
				parentwindow->Unlock();
			}
		}
		if(mView->LockLooper())
			mustunlock = true;

		if(mustunlock && mView->Parent() == 0)
			havewindow = true;

		if(mView->Parent() || ! havewindow)
		{
			mView->MoveTo(aX, aY);
		}
		else
		{
				mView->Window()->MoveTo(aX, aY);
		}
		if(mustunlock)
			mView->UnlockLooper();
	}
	else
		OnMove(aX,aY);

	return NS_OK;
}



//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
	bool mustunlock = false;
	bool havewindow = false;

	if(aWidth < 0 || aHeight < 0)
		return NS_OK;

	// Set cached value for lightweight and printing
	mBounds.width  = aWidth;
	mBounds.height = aHeight;

	if(mView)
	{
		if(mView->LockLooper())
			mustunlock = true;

		if(mustunlock && mView->Parent() == 0)
			havewindow = true;

		if(mView->Parent() || ! havewindow)
			mView->ResizeTo(aWidth-1, GetHeight(aHeight)-1);
		else
			((nsWindowBeOS *)mView->Window())->ResizeTo(aWidth-1, GetHeight(aHeight)-1);

		//This caused senseless blinking on page reload. Do we need it?
		//if (aRepaint && mustunlock)
		//	mView->Invalidate();
			
		if(mustunlock)
			mView->UnlockLooper();
		//No feedback from BView/BWindow in this case.
		//Informing xp layer directly
		if(!mIsVisible || !mView->Window()->IsActive())
			OnResize(mBounds);
	}
	else
	{
		//inform the xp layer of the change in size
		OnResize(mBounds);
	}

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aX,
                           PRInt32 aY,
                           PRInt32 aWidth,
                           PRInt32 aHeight,
                           PRBool   aRepaint)
{
	Move(aX,aY);
	Resize(aWidth,aHeight,aRepaint);
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool aState)
{
	//TODO: Needs real corect implementation in future
	if(mView && mView->LockLooper()) 
	{
		if (mView->Window()) 
		{
			uint flags = mView->Window()->Flags();
			if (aState == PR_TRUE) {
				flags &= ~B_AVOID_FOCUS;
			} else {
				flags |= B_AVOID_FOCUS;
			}
			mView->Window()->SetFlags(flags);
		}
		mView->UnlockLooper();
	}
	mEnabled = aState;
	return NS_OK;
}


NS_METHOD nsWindow::IsEnabled(PRBool *aState)
{
	NS_ENSURE_ARG_POINTER(aState);
	// looks easy enough, but...
	*aState = mEnabled;
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
	//
	// Switch to the "main gui thread" if necessary... This method must
	// be executed on the "gui thread"...
	//
	nsToolkit* toolkit = (nsToolkit *)mToolkit;
	if (!toolkit->IsGuiThread()) 
	{
		uint32 args[1];
		args[0] = (uint32)aRaise;
		MethodInfo info(this, this, nsWindow::SET_FOCUS, 1, args);
		toolkit->CallMethod(&info);
		return NS_ERROR_FAILURE;
	}
	
	// Don't set focus on disabled widgets or popups
	if (!mEnabled || eWindowType_popup == mWindowType)
		return NS_OK;
		
	if(mView && mView->LockLooper())
	{
		if (mView->Window() && 
		    aRaise == PR_TRUE &&
		    eWindowType_popup != mWindowType && 
			  !mView->Window()->IsActive() && 
			  gLastActiveWindow != mView->Window())
			mView->Window()->Activate(true);
			
		mView->MakeFocus(true);
		mView->UnlockLooper();
	}

	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
	if(mView && mView->LockLooper())
	{
		BRect r = mView->Frame();
		aRect.x = nscoord(r.left);
		aRect.y = nscoord(r.top);
		aRect.width  = r.IntegerWidth()+1;
		aRect.height = r.IntegerHeight()+1;
		mView->UnlockLooper();
	} 
	else 
	{
		aRect = mBounds;
	}

	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this component size and position in screen coordinates
//
//-------------------------------------------------------------------------    
NS_IMETHODIMP nsWindow::GetScreenBounds(nsRect &aRect)
{
	// A window's Frame() value is cached, so locking is not needed
	if(mView && mView->Window()) 
	{
		BRect r = mView->Window()->Frame();
		aRect.x = nscoord(r.left);
		aRect.y = nscoord(r.top);
		aRect.width  = r.IntegerWidth()+1;
		aRect.height = r.IntegerHeight()+1;
	} 
	else 
	{
		aRect = mBounds;
	}
	return NS_OK;
}  

//-------------------------------------------------------------------------
//
// Set the background/foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
	nsBaseWidget::SetBackgroundColor(aColor);

	if(mView && mView->LockLooper())
	{
		mView->SetViewColor(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor), NS_GET_A(aColor));
		mView->SetLowColor(B_TRANSPARENT_COLOR);
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD nsWindow::SetForegroundColor(const nscolor &aColor)
{
	nsBaseWidget::SetBackgroundColor(aColor);

	if(mView && mView->LockLooper())
	{
		mView->SetHighColor(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor), NS_GET_A(aColor));
		mView->UnlockLooper();
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
	NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
	return NULL;
}


//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
  // Cache Font for owner draw
  if (mFont == nsnull) {
    if (!(mFont = new nsFont(aFont)))
      return NS_ERROR_OUT_OF_MEMORY;
  } else {
    *mFont = aFont;
  }

  // Bail out if there is no context
  if (nsnull == mContext) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFontMetrics> metrics;
  mContext->GetMetricsFor(aFont, *getter_AddRefs(metrics));
  nsFontHandle fontHandle;
  metrics->GetFontHandle(fontHandle);
  BFont *font = (BFont*)fontHandle;
  if(font && mView && mView->LockLooper())
  {
    mView->SetFont(font, B_FONT_ALL);
    mView->UnlockLooper();
  }

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
	if(!mView)
		return NS_ERROR_FAILURE;

	// Only change cursor if it's changing
	if (aCursor != mCursor) 
	{
		BCursor const *newCursor = B_CURSOR_SYSTEM_DEFAULT;
		
		// Check to see if the array has been loaded, if not, do it.
		if (gCursorArray.Count() == 0) 
		{
			gCursorArray.InsertElementAt((void*) new BCursor(cursorHyperlink),0);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorHorizontalDrag),1);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorVerticalDrag),2);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorUpperRight),3);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorLowerLeft),4);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorUpperLeft),5);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorLowerRight),6);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorTop),7);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorBottom),8);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorLeft),9);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorRight),10);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCrosshair),11);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorHelp),12);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorGrab),13);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorGrabbing),14);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCopy),15);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorAlias),16);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorWatch2),17);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCell),18);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCountUp),19);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCountDown),20);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCountUpDown),21);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorZoomIn),22);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorZoomOut),23);
		}

		switch (aCursor) 
		{
			case eCursor_standard:
			case eCursor_wait:
			case eCursor_move:
				newCursor = B_CURSOR_SYSTEM_DEFAULT;
				break;
	
			case eCursor_select:
				newCursor = B_CURSOR_I_BEAM;
				break;
	
			case eCursor_hyperlink:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(0);
				break;
	
			case eCursor_sizeWE:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(1);
				break;
	
			case eCursor_sizeNS:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(2);
				break;
	
			case eCursor_sizeNW:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(3);
				break;
	
			case eCursor_sizeSE:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(4);
				break;
	
			case eCursor_sizeNE:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(5);
				break;
	
			case eCursor_sizeSW:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(6);
				break;
	
			case eCursor_arrow_north:
			case eCursor_arrow_north_plus:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(7);
				break;
	
			case eCursor_arrow_south:
			case eCursor_arrow_south_plus:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(8);
				break;
	
			case eCursor_arrow_east:
			case eCursor_arrow_east_plus:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(9);
				break;
	
			case eCursor_arrow_west:
			case eCursor_arrow_west_plus:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(10);
				break;
	
			case eCursor_crosshair:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(11);
				break;
	
			case eCursor_help:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(12);
				break;
	
			case eCursor_copy:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(15);
				break;
	
			case eCursor_alias:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(16);
				break;

			case eCursor_context_menu:
				break;
				
			case eCursor_cell:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(18);
				break;

			case eCursor_grab:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(13);
				break;
	
			case eCursor_grabbing:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(14);
				break;
	
			case eCursor_spinning:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(17);
				break;
	
			case eCursor_count_up:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(19);
				break;

			case eCursor_count_down:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(20);
				break;

			case eCursor_count_up_down:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(21);
				break;

			case eCursor_zoom_in:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(22);
				break;

			case eCursor_zoom_out:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(23);
				break;

			default:
				NS_ASSERTION(0, "Invalid cursor type");
				break;
		}
		NS_ASSERTION(newCursor != nsnull, "Cursor not stored in array properly!");
		mCursor = aCursor;
		be_app->SetCursor(newCursor, true);
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
	nsresult rv = NS_ERROR_FAILURE;

	if (PR_TRUE == aIsSynchronous)
	{
		//Synchronous painting is using direct call of OnPaint() now,
		//to avoid senseless overhead of callbacks and messages
		rv = OnPaint(mBounds) ? NS_OK : NS_ERROR_FAILURE;
	}
	else
	{
		//Asynchronous painting is performed with implicit usage of BWindow/app_server queues, via Draw() calls. 
		//All update rects are collected in nsViewBeOS member  "paintregion".
		//Flushing and cleanup of paintregion happens in nsViewBeOS::GetPaintRegion() when it
		//is called in nsWindow::CallMethod() in case of ONPAINT.
		if (mView && mView->LockLooper())
		{
			mView->Draw(mView->Bounds());
			mView->UnlockLooper();
			rv = NS_OK;
		}
	}
	return rv;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
	nsresult rv = NS_ERROR_FAILURE;

	if (PR_TRUE == aIsSynchronous)
	{
		rv = OnPaint((nsRect &)aRect) ? NS_OK : NS_ERROR_FAILURE;
	}
	else
	{
		if (mView && mView->LockLooper()) 
		{
			BRect	r(aRect.x, 
					aRect.y, 
					aRect.x + aRect.width - 1, 
					aRect.y + aRect.height - 1);
			mView->Draw(r);
			rv = NS_OK;
		}
		mView->UnlockLooper();
	}
	return rv;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
	
	nsRect r;
	nsRegionRectSet *rectSet = nsnull;
	if (!aRegion)
		return NS_ERROR_FAILURE;
	nsresult rv = ((nsIRegion *)aRegion)->GetRects(&rectSet);
	if (NS_FAILED(rv))
		return rv;

	for (PRUint32 i=0; i< rectSet->mRectsLen; ++i)
	{
		r.x = rectSet->mRects[i].x;
		r.y = rectSet->mRects[i].y;
		r.width = rectSet->mRects[i].width;
		r.height = rectSet->mRects[i].height;
		if (aIsSynchronous)
		{
			rv = OnPaint(r) ? NS_OK : NS_ERROR_FAILURE;
		}
		else
		{
			if (mView && mView->LockLooper())
			{
				mView->Draw(BRect(r.x, r.y, r.x + r.width -1, r.y + r.height -1));
				mView->UnlockLooper();
				rv = NS_OK;
			}
		}
	}

	return rv;
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
	nsresult rv = NS_ERROR_FAILURE;
	if( mView && mView->Window())
	{
		mView->Window()->UpdateIfNeeded();
		rv = NS_OK;
	}
	return rv;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
	switch(aDataType) 
	{
		case NS_NATIVE_WIDGET:
		case NS_NATIVE_WINDOW:
		case NS_NATIVE_PLUGIN_PORT:
		case NS_NATIVE_GRAPHIC:
			return (void *)((BView *)mView);
		case NS_NATIVE_COLORMAP:
		default:
			break;
	}

	return NULL;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
	NS_WARNING("nsWindow::SetColorMap - not implemented");
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
	if(mView && mView->LockLooper())
	{
		BRect src;
		BRect b = mView->Bounds();
		if(aClipRect)
		{
			src.left = aClipRect->x;
			src.top = aClipRect->y;
			src.right = aClipRect->XMost() - 1;
			src.bottom = aClipRect->YMost() - 1;
		}
		else
		{
			src = b;
		}

		BRegion	invalid;
		invalid.Include(src);

		// make sure we only reference visible bits
		// so we don't trigger a BView invalidate
		if(src.left + aDx < 0)
			src.left = -aDx;
		if(src.right + aDx > b.right)
			src.right = b.right - aDx;
		if(src.top + aDy < 0)
			src.top = -aDy;
		if(src.bottom + aDy > b.bottom)
			src.bottom = b.bottom - aDy;
		BRect dest = src.OffsetByCopy(aDx, aDy);

		mView->ConstrainClippingRegion(&invalid);

		if(src.IsValid() && dest.IsValid())
			mView->CopyBits(src, dest);

		invalid.Exclude(dest);		
		//Preventing main view invalidation loop-chain  when children are moving
		//by forcing call of DrawAfterChildren() method instead Draw()
		//We don't use BeginResizingChildren here in order to avoid extra locking
		mView->SetFlags(mView->Flags() | B_DRAW_ON_CHILDREN);
		//Now moving children
		for(BView *child = mView->ChildAt(0); child; child = child->NextSibling())
		{
			child->MoveBy(aDx, aDy);
		}
		//Returning to normal Draw()
		mView->SetFlags(mView->Flags() & ~B_DRAW_ON_CHILDREN);
		mView->ConstrainClippingRegion(&invalid);
		//We'll call OnPaint() without locking mView - OnPaint does it itself
		mView->UnlockLooper();
		// scan through rects and paint them directly
		// so we avoid going through the callback stuff
		int32 rects = invalid.CountRects();
		for(int32 i = 0; i < rects; i++)
		{
			BRect	curr = invalid.RectAt(i);
			nsRect	r;
			r.x = (nscoord)curr.left;
			r.y = (nscoord)curr.top;
			r.width = (nscoord)curr.Width() + 1;
			r.height = (nscoord)curr.Height() + 1;
			OnPaint(r);
		}
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Every function that needs a thread switch goes through this function
// by calling SendMessage (..WM_CALLMETHOD..) in nsToolkit::CallMethod.
//
//-------------------------------------------------------------------------
bool nsWindow::CallMethod(MethodInfo *info)
{
	bool bRet = TRUE;

	switch (info->methodId)
	{
	case nsWindow::CREATE:
		NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
		Create((nsIWidget*)(info->args[0]),
		       (nsRect&)*(nsRect*)(info->args[1]),
		       (EVENT_CALLBACK)(info->args[2]),
		       (nsIDeviceContext*)(info->args[3]),
		       (nsIAppShell *)(info->args[4]),
		       (nsIToolkit*)(info->args[5]),
		       (nsWidgetInitData*)(info->args[6]));
		break;

	case nsWindow::CREATE_NATIVE:
		NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
		Create((nsNativeWidget)(info->args[0]),
		       (nsRect&)*(nsRect*)(info->args[1]),
		       (EVENT_CALLBACK)(info->args[2]),
		       (nsIDeviceContext*)(info->args[3]),
		       (nsIAppShell *)(info->args[4]),
		       (nsIToolkit*)(info->args[5]),
		       (nsWidgetInitData*)(info->args[6]));
		break;

	case nsWindow::DESTROY:
		NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
		Destroy();
		break;

	case nsWindow::CLOSEWINDOW :
		NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
		if (gGotQuitShortcut == PR_TRUE)
		{
			gGotQuitShortcut = PR_FALSE;
			return false;
		}
		if (eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
			DealWithPopups(CLOSEWINDOW,nsPoint(0,0));
		DispatchStandardEvent(NS_DESTROY);
		break;

	case nsWindow::SET_FOCUS:
		NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");
		if (!mEnabled)
			return false;
		SetFocus(((PRBool *)info->args)[0]);
		break;

	case nsWindow::GOT_FOCUS:
		NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
		if (!mEnabled)
			return false;
		DispatchFocus(NS_GOTFOCUS);
		break;

	case nsWindow::KILL_FOCUS:
		NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
		DispatchFocus(NS_LOSTFOCUS);
		break;

	case nsWindow::BTNCLICK :
		{
			NS_ASSERTION(info->nArgs == 5, "Wrong number of arguments to CallMethod");
			if (!mEnabled)
				return false;
			// close popup when clicked outside of the popup window
			uint32 eventID = ((int32 *)info->args)[0];
			bool rollup = false;

			if ((eventID == NS_MOUSE_LEFT_BUTTON_DOWN ||
			        eventID == NS_MOUSE_RIGHT_BUTTON_DOWN ||
			        eventID == NS_MOUSE_MIDDLE_BUTTON_DOWN) &&
			        mView && mView->LockLooper())
			{
				BPoint p(((int32 *)info->args)[1], ((int32 *)info->args)[2]);
				mView->ConvertToScreen(&p);
				if (DealWithPopups(nsWindow::ONMOUSE, nsPoint(p.x, p.y)))
					rollup = true;
				mView->UnlockLooper();
			}

			DispatchMouseEvent(((int32 *)info->args)[0],
			                   nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
			                   ((int32 *)info->args)[3],
			                   ((int32 *)info->args)[4]);

			if (((int32 *)info->args)[0] == NS_MOUSE_RIGHT_BUTTON_DOWN)
			{
				DispatchMouseEvent (NS_CONTEXTMENU,
				                    nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
				                    ((int32 *)info->args)[3],
				                    ((int32 *)info->args)[4]);
			}
		}
		break;

	case nsWindow::ONWHEEL :
		{
			NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");

			nsMouseScrollEvent scrollEvent(NS_MOUSE_SCROLL, this);

			scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;

			scrollEvent.delta = (info->args)[0];

			scrollEvent.time      = PR_IntervalNow();

			// XXX implement these items?
			scrollEvent.point.x = 100;
			scrollEvent.point.y = 100;

			// we don't use the mIsXDown bools because
			// they get reset on Gecko reload (makes it harder
			// to use stuff like Alt+Wheel)
			uint32 mod (modifiers());

			scrollEvent.isControl = mod & B_CONTROL_KEY;
			scrollEvent.isShift = mod & B_SHIFT_KEY;
			scrollEvent.isAlt   = mod & B_COMMAND_KEY;
			scrollEvent.isMeta  = mod & B_OPTION_KEY;

			nsEventStatus rv;
			DispatchEvent (&scrollEvent, rv);
		}
		break;

	case nsWindow::ONKEY :
		NS_ASSERTION(info->nArgs == 6, "Wrong number of arguments to CallMethod");
		if (((int32 *)info->args)[0] == NS_KEY_DOWN)
		{
			OnKeyDown(((int32 *)info->args)[0],
			          (const char *)(&((uint32 *)info->args)[1]), ((int32 *)info->args)[2],
			          ((uint32 *)info->args)[3], ((uint32 *)info->args)[4], ((int32 *)info->args)[5]);
		}
		else
		{
			if (((int32 *)info->args)[0] == NS_KEY_UP)
			{
				OnKeyUp(((int32 *)info->args)[0],
				        (const char *)(&((uint32 *)info->args)[1]), ((int32 *)info->args)[2],
				        ((uint32 *)info->args)[3], ((uint32 *)info->args)[4], ((int32 *)info->args)[5]);
			}
		}
		break;

	case nsWindow::ONPAINT :
		NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
		if(mView)
		{
			BRegion reg;
			reg.MakeEmpty();
			nsViewBeOS *bv = dynamic_cast<nsViewBeOS *>(mView);
			if(bv && bv->GetPaintRegion(&reg) && reg.Frame().IsValid())
			{
				//TODO: provide region for OnPaint(). Not only rect.
				BRect br = reg.Frame();
				nsRect r(nscoord(br.left), nscoord(br.top), 
						nscoord(br.IntegerWidth() + 1), nscoord(br.IntegerHeight() + 1));
				OnPaint(r);
			}
		}
		break;

	case nsWindow::ONRESIZE :
		{
			NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod");
			if(eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
				DealWithPopups(ONRESIZE,nsPoint(0,0));
			nsRect r;
			r.width=(nscoord)info->args[0];
			r.height=(nscoord)info->args[1];

			OnResize(r);
		}
		break;

	case nsWindow::ONSCROLL:
		NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
		if (mEnabled)
			return false;
		OnScroll();
		break;

	case nsWindow::ONMOUSE :
		NS_ASSERTION(info->nArgs == 4, "Wrong number of arguments to CallMethod");
		if (!mEnabled)
			return false;
		DispatchMouseEvent(((int32 *)info->args)[0],
		                   nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
		                   0,
		                   ((int32 *)info->args)[3]);
		break;

	case nsWindow::ONACTIVATE:
		NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod");
		if (!mEnabled || eWindowType_popup == mWindowType || 0 == mView->Window())
			return false;
		if((BWindow *)info->args[1] != mView->Window())
			return false;
		if (*mEventCallback || eWindowType_child == mWindowType)
		{
			bool active = (bool)info->args[0];
			if(!active) 
			{
				if (eWindowType_dialog == mWindowType || 
				    eWindowType_toplevel == mWindowType)
					DealWithPopups(ONACTIVATE,nsPoint(0,0));

				//Testing if BWindow is really deactivated.
				if(!mView->Window()->IsActive())
					DispatchFocus(NS_DEACTIVATE);
			} 
			else 
			{
				if(mView->Window()->IsActive())
					DispatchFocus(NS_ACTIVATE);
					
				if (mView && mView->Window())
					gLastActiveWindow = mView->Window();
			}
		}
		break;

	case nsWindow::ONMOVE:
		NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod");
		if(eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
			DealWithPopups(ONMOVE,nsPoint(0,0));
		PRInt32 aX,  aY;
		aX=(nscoord)info->args[0];
		aY=(nscoord)info->args[1];
		OnMove(aX,aY);
		break;

	case ONWORKSPACE:
		NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod");
		if(eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
			DealWithPopups(ONWORKSPACE,nsPoint(0,0));
			
		break;	default:
		bRet = FALSE;
		break;
	}

	return bRet;
}

//-------------------------------------------------------------------------
//
// Key code translation related data
//
//-------------------------------------------------------------------------

struct nsKeyConverter {
	int vkCode; // Platform independent key code
	char bekeycode; // BeOS key code
};

//
// Netscape keycodes are defined in widget/public/nsGUIEvent.h
// BeOS keycodes can be viewd at
// http://www.be.com/documentation/be_book/Keyboard/KeyboardKeyCodes.html
//

struct nsKeyConverter nsKeycodesBeOS[] = {
	        //  { NS_VK_CANCEL,     GDK_Cancel },
	        { NS_VK_BACK,       0x1e },
	        { NS_VK_TAB,        0x26 },
	        //  { NS_VK_TAB,        GDK_ISO_Left_Tab },
	        //  { NS_VK_CLEAR,      GDK_Clear },
	        { NS_VK_RETURN,     0x47 },
	        { NS_VK_SHIFT,      0x4b },
	        { NS_VK_SHIFT,      0x56 },
	        { NS_VK_CONTROL,    0x5c },
	        { NS_VK_CONTROL,    0x60 },
	        { NS_VK_ALT,        0x5d },
	        { NS_VK_ALT,        0x5f },
	        { NS_VK_PAUSE,      0x22 },
	        { NS_VK_CAPS_LOCK,  0x3b },
	        { NS_VK_ESCAPE,     0x1 },
	        { NS_VK_SPACE,      0x5e },
	        { NS_VK_PAGE_UP,    0x21 },
	        { NS_VK_PAGE_DOWN,  0x36 },
	        { NS_VK_END,        0x35 },
	        { NS_VK_HOME,       0x20 },
	        { NS_VK_LEFT,       0x61 },
	        { NS_VK_UP,         0x57 },
	        { NS_VK_RIGHT,      0x63 },
	        { NS_VK_DOWN,       0x62 },
	        { NS_VK_PRINTSCREEN, 0xe },
	        { NS_VK_INSERT,     0x1f },
	        { NS_VK_DELETE,     0x34 },

	        // The "Windows Key"
	        { NS_VK_META,       0x66 },
	        { NS_VK_META,       0x67 },

	        // keypad keys (constant keys)
	        { NS_VK_MULTIPLY,   0x24 },
	        { NS_VK_ADD,        0x3a },
	        //  { NS_VK_SEPARATOR,   }, ???
	        { NS_VK_SUBTRACT,   0x25 },
	        { NS_VK_DIVIDE,     0x23 },
	        { NS_VK_RETURN,     0x5b },

	        { NS_VK_COMMA,      0x53 },
	        { NS_VK_PERIOD,     0x54 },
	        { NS_VK_SLASH,      0x55 },
	        { NS_VK_BACK_SLASH, 0x33 },
	        { NS_VK_BACK_SLASH, 0x6a }, // got this code on japanese keyboard
	        { NS_VK_BACK_SLASH, 0x6b }, // got this code on japanese keyboard
	        { NS_VK_BACK_QUOTE, 0x11 },
	        { NS_VK_OPEN_BRACKET, 0x31 },
	        { NS_VK_CLOSE_BRACKET, 0x32 },
	        { NS_VK_SEMICOLON, 0x45 },
	        { NS_VK_QUOTE, 0x46 },

	        // NS doesn't have dash or equals distinct from the numeric keypad ones,
	        // so we'll use those for now.  See bug 17008:
	        { NS_VK_SUBTRACT, 0x1c },
	        { NS_VK_EQUALS, 0x1d },

	        { NS_VK_F1, B_F1_KEY },
	        { NS_VK_F2, B_F2_KEY },
	        { NS_VK_F3, B_F3_KEY },
	        { NS_VK_F4, B_F4_KEY },
	        { NS_VK_F5, B_F5_KEY },
	        { NS_VK_F6, B_F6_KEY },
	        { NS_VK_F7, B_F7_KEY },
	        { NS_VK_F8, B_F8_KEY },
	        { NS_VK_F9, B_F9_KEY },
	        { NS_VK_F10, B_F10_KEY },
	        { NS_VK_F11, B_F11_KEY },
	        { NS_VK_F12, B_F12_KEY },

	        { NS_VK_1, 0x12 },
	        { NS_VK_2, 0x13 },
	        { NS_VK_3, 0x14 },
	        { NS_VK_4, 0x15 },
	        { NS_VK_5, 0x16 },
	        { NS_VK_6, 0x17 },
	        { NS_VK_7, 0x18 },
	        { NS_VK_8, 0x19 },
	        { NS_VK_9, 0x1a },
	        { NS_VK_0, 0x1b },

	        { NS_VK_A, 0x3c },
	        { NS_VK_B, 0x50 },
	        { NS_VK_C, 0x4e },
	        { NS_VK_D, 0x3e },
	        { NS_VK_E, 0x29 },
	        { NS_VK_F, 0x3f },
	        { NS_VK_G, 0x40 },
	        { NS_VK_H, 0x41 },
	        { NS_VK_I, 0x2e },
	        { NS_VK_J, 0x42 },
	        { NS_VK_K, 0x43 },
	        { NS_VK_L, 0x44 },
	        { NS_VK_M, 0x52 },
	        { NS_VK_N, 0x51 },
	        { NS_VK_O, 0x2f },
	        { NS_VK_P, 0x30 },
	        { NS_VK_Q, 0x27 },
	        { NS_VK_R, 0x2a },
	        { NS_VK_S, 0x3d },
	        { NS_VK_T, 0x2b },
	        { NS_VK_U, 0x2d },
	        { NS_VK_V, 0x4f },
	        { NS_VK_W, 0x28 },
	        { NS_VK_X, 0x4d },
	        { NS_VK_Y, 0x2c },
	        { NS_VK_Z, 0x4c }
        };

// keycode of keypad when num-locked
struct nsKeyConverter nsKeycodesBeOSNumLock[] = {
	        { NS_VK_NUMPAD0, 0x64 },
	        { NS_VK_NUMPAD1, 0x58 },
	        { NS_VK_NUMPAD2, 0x59 },
	        { NS_VK_NUMPAD3, 0x5a },
	        { NS_VK_NUMPAD4, 0x48 },
	        { NS_VK_NUMPAD5, 0x49 },
	        { NS_VK_NUMPAD6, 0x4a },
	        { NS_VK_NUMPAD7, 0x37 },
	        { NS_VK_NUMPAD8, 0x38 },
	        { NS_VK_NUMPAD9, 0x39 },
	        { NS_VK_DECIMAL, 0x65 }
        };

// keycode of keypad when not num-locked
struct nsKeyConverter nsKeycodesBeOSNoNumLock[] = {
	        { NS_VK_LEFT,       0x48 },
	        { NS_VK_RIGHT,      0x4a },
	        { NS_VK_UP,         0x38 },
	        { NS_VK_DOWN,       0x59 },
	        { NS_VK_PAGE_UP,    0x39 },
	        { NS_VK_PAGE_DOWN,  0x5a },
	        { NS_VK_HOME,       0x37 },
	        { NS_VK_END,        0x58 },
	        { NS_VK_INSERT,     0x64 },
	        { NS_VK_DELETE,     0x65 }
        };

//-------------------------------------------------------------------------
//
// Translate key code
// Input is BeOS keyboard key-code; output is in NS_VK format
//
//-------------------------------------------------------------------------

static int TranslateBeOSKeyCode(int32 bekeycode, bool isnumlock)
{
#ifdef KB_DEBUG
	printf("TranslateBeOSKeyCode: bekeycode = 0x%x\n",bekeycode);
#endif
	int i;
	int length = sizeof(nsKeycodesBeOS) / sizeof(struct nsKeyConverter);
	int length_numlock = sizeof(nsKeycodesBeOSNumLock) / sizeof(struct nsKeyConverter);
	int length_nonumlock = sizeof(nsKeycodesBeOSNoNumLock) / sizeof(struct nsKeyConverter);

	// key code conversion
	for (i = 0; i < length; i++) {
		if (nsKeycodesBeOS[i].bekeycode == bekeycode)
			return(nsKeycodesBeOS[i].vkCode);
	}
	// numpad keycode vary with numlock
	if (isnumlock) {
		for (i = 0; i < length_numlock; i++) {
			if (nsKeycodesBeOSNumLock[i].bekeycode == bekeycode)
				return(nsKeycodesBeOSNumLock[i].vkCode);
		}
	} else {
		for (i = 0; i < length_nonumlock; i++) {
			if (nsKeycodesBeOSNoNumLock[i].bekeycode == bekeycode)
				return(nsKeycodesBeOSNoNumLock[i].vkCode);
		}
	}
#ifdef KB_DEBUG
	printf("TranslateBeOSKeyCode: ####### Translation not Found #######\n");
#endif
	return((int)0);
}

//-------------------------------------------------------------------------
//
// OnKeyDown
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnKeyDown(PRUint32 aEventType, const char *bytes,
                           int32 numBytes, PRUint32 mod, PRUint32 bekeycode, int32 rawcode)
{
	PRUint32 aTranslatedKeyCode;
	PRBool result = PR_FALSE;

	mIsShiftDown   = (mod & B_SHIFT_KEY) ? PR_TRUE : PR_FALSE;
	mIsControlDown = (mod & B_CONTROL_KEY) ? PR_TRUE : PR_FALSE;
	mIsAltDown     = ((mod & B_COMMAND_KEY) && !(mod & B_RIGHT_OPTION_KEY))? PR_TRUE : PR_FALSE;
	mIsMetaDown    = (mod & B_LEFT_OPTION_KEY) ? PR_TRUE : PR_FALSE;	
	bool IsNumLocked = ((mod & B_NUM_LOCK) != 0);
	if(B_COMMAND_KEY && (rawcode == 'w' || rawcode == 'W'))
		gGotQuitShortcut = PR_TRUE;
	else
		gGotQuitShortcut = PR_FALSE;

	aTranslatedKeyCode = TranslateBeOSKeyCode(bekeycode, IsNumLocked);

	if (numBytes <= 1)
	{
		result = DispatchKeyEvent(NS_KEY_DOWN, 0, aTranslatedKeyCode);
	} else {
		//   non ASCII chars
	}

	// ------------  On Char  ------------
	PRUint32	uniChar;

	if ((mIsControlDown || mIsAltDown || mIsMetaDown) && rawcode >= 'a' && rawcode <= 'z') {
		if (mIsShiftDown)
			uniChar = rawcode + 'A' - 'a';
		else
			uniChar = rawcode;
		aTranslatedKeyCode = 0;
	} else {
		if (numBytes == 0) // deal with unmapped key
			return result;

		switch((unsigned char)bytes[0])
		{
		case 0xc8://System Request
		case 0xca://Break
			return result;// do not send 'KEY_PRESS' message

		case B_INSERT:
		case B_ESCAPE:
		case B_FUNCTION_KEY:
		case B_HOME:
		case B_PAGE_UP:
		case B_END:
		case B_PAGE_DOWN:
		case B_UP_ARROW:
		case B_LEFT_ARROW:
		case B_DOWN_ARROW:
		case B_RIGHT_ARROW:
		case B_TAB:
		case B_DELETE:
		case B_BACKSPACE:
		case B_ENTER:
			uniChar = 0;
			break;

		default:
			// UTF-8 to unicode conversion
			if (numBytes >= 1 && (bytes[0] & 0x80) == 0) {
				// 1 byte utf-8 char
				uniChar = bytes[0];
			} else
				if (numBytes >= 2 && (bytes[0] & 0xe0) == 0xc0) {
					// 2 byte utf-8 char
					uniChar = ((uint16)(bytes[0] & 0x1f) << 6) | (uint16)(bytes[1] & 0x3f);
				} else
					if (numBytes >= 3 && (bytes[0] & 0xf0) == 0xe0) {
						// 3 byte utf-8 char
						uniChar = ((uint16)(bytes[0] & 0x0f) << 12) | ((uint16)(bytes[1] & 0x3f) << 6)
						          | (uint16)(bytes[2] & 0x3f);
					} else {
						//error
						uniChar = 0;
						NS_WARNING("nsWindow::OnKeyDown() error: bytes[] has not enough chars.");
					}

			aTranslatedKeyCode = 0;
			break;
		}
	}

	PRBool result2 = DispatchKeyEvent(NS_KEY_PRESS, uniChar, aTranslatedKeyCode);

	return result && result2;
}

//-------------------------------------------------------------------------
//
// OnKeyUp
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnKeyUp(PRUint32 aEventType, const char *bytes,
                         int32 numBytes, PRUint32 mod, PRUint32 bekeycode, int32 rawcode)
{
	PRUint32 aTranslatedKeyCode;
	bool IsNumLocked = ((mod & B_NUM_LOCK) != 0);

	mIsShiftDown   = (mod & B_SHIFT_KEY) ? PR_TRUE : PR_FALSE;
	mIsControlDown = (mod & B_CONTROL_KEY) ? PR_TRUE : PR_FALSE;
	mIsAltDown     = ((mod & B_COMMAND_KEY) && !(mod & B_RIGHT_OPTION_KEY))? PR_TRUE : PR_FALSE;
	mIsMetaDown    = (mod & B_LEFT_OPTION_KEY) ? PR_TRUE : PR_FALSE;	

	aTranslatedKeyCode = TranslateBeOSKeyCode(bekeycode, IsNumLocked);

	PRBool result = DispatchKeyEvent(NS_KEY_UP, 0, aTranslatedKeyCode);
	return result;

}

//-------------------------------------------------------------------------
//
// DispatchKeyEvent
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchKeyEvent(PRUint32 aEventType, PRUint32 aCharCode,
                                  PRUint32 aKeyCode)
{
	nsKeyEvent event(aEventType, this);
	nsPoint point;

	point.x = 0;
	point.y = 0;

	InitEvent(event, &point); // this add ref's event.widget

	event.charCode = aCharCode;
	event.keyCode  = aKeyCode;

#ifdef KB_DEBUG
	static int cnt=0;
	printf("%d DispatchKE Type: %s charCode 0x%x  keyCode 0x%x ", cnt++,
	       (NS_KEY_PRESS == aEventType)?"PRESS":(aEventType == NS_KEY_UP?"Up":"Down"),
	       event.charCode, event.keyCode);
	printf("Shift: %s Control %s Alt: %s Meta: %s\n",  
	       (mIsShiftDown?"D":"U"), 
	       (mIsControlDown?"D":"U"), 
	       (mIsAltDown?"D":"U"),
	       (mIsMetaDown?"D":"U"));
#endif

	event.isShift   = mIsShiftDown;
	event.isControl = mIsControlDown;
	event.isMeta   =  mIsMetaDown;
	event.isAlt     = mIsAltDown;

	PRBool result = DispatchWindowEvent(&event);
	NS_RELEASE(event.widget);

	return result;
}

//---------------------------------------------------------
//
// Enabled/disable file drop for this component
//
//---------------------------------------------------------
NS_METHOD nsWindow::EnableFileDrop(PRBool aEnable)
{
	NS_NOTYETIMPLEMENTED("EnableFileDrop not yet implemented"); // to be implemented
	return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
	mOnDestroyCalled = PR_TRUE;

	// release references to children, device context, toolkit, and app shell
	nsBaseWidget::OnDestroy();

	// dispatch the event
	if (!mIsDestroying) 
	{
		// dispatching of the event may cause the reference count to drop to 0
		// and result in this object being destroyed. To avoid that, add a reference
		// and then release it after dispatching the event
		AddRef();
		DispatchStandardEvent(NS_DESTROY);
		Release();
	}
}

//-------------------------------------------------------------------------
//
// Move
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnMove(PRInt32 aX, PRInt32 aY)
{
	nsGUIEvent event(NS_MOVE, this);
	InitEvent(event);
	event.point.x = aX;
	event.point.y = aY;

	PRBool result = DispatchWindowEvent(&event);
	NS_RELEASE(event.widget);
	return result;
}

//-------------------------------------------------------------------------
//
// Paint
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnPaint(nsRect &r)
{
	PRBool result = PR_FALSE;

	if((r.width || r.height) && mEventCallback)
	{
		if(mView && mView->LockLooper())
		{
			// set clipping
			BRegion invalid;
			invalid.Include(BRect(r.x, r.y, r.x + r.width - 1, r.y + r.height - 1));
			mView->ConstrainClippingRegion(&invalid);
			mView->Flush();
			mView->UnlockLooper();
			nsPaintEvent event(NS_PAINT, this);

			InitEvent(event);
			event.region = nsnull;
			event.rect = &r;

			static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
			static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

			if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&event.renderingContext))
			{
				event.renderingContext->Init(mContext, this);
				result = DispatchWindowEvent(&event);

				NS_RELEASE(event.renderingContext);
				result = PR_TRUE;
			}
			else
				result = PR_FALSE;

			NS_RELEASE(event.widget);

		}
	}
	return result;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(nsRect &aWindowRect)
{
	// call the event callback
	if (mEventCallback)
	{
		nsSizeEvent event(NS_SIZE, this);
		InitEvent(event);
		event.windowSize = &aWindowRect;
		if(mView && mView->LockLooper())
		{
			BRect r = mView->Bounds();
			event.mWinWidth  = r.IntegerWidth()+1;
			event.mWinHeight = r.IntegerHeight()+1;
			mView->UnlockLooper();
		} else {
			event.mWinWidth  = 0;
			event.mWinHeight = 0;
		}
		PRBool result = DispatchWindowEvent(&event);
		NS_RELEASE(event.widget);
		return result;
	}

	return PR_FALSE;
}



//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, nsPoint aPoint, PRUint32 clicks, PRUint32 mod)
{
	PRBool result = PR_FALSE;
	if(nsnull != mEventCallback || nsnull != mMouseListener)
	{
		nsMouseEvent event(aEventType, this);
		InitEvent (event, &aPoint);
		event.isShift   = mod & B_SHIFT_KEY;
		event.isControl = mod & B_CONTROL_KEY;
		event.isAlt     = mod & B_COMMAND_KEY;
		event.isMeta     = mod & B_OPTION_KEY;
		event.clickCount = clicks;

		// call the event callback
		if(nsnull != mEventCallback)
		{
			result = DispatchWindowEvent(&event);
			NS_RELEASE(event.widget);
			return result;
		}
		else
		{
			switch(aEventType)
			{
			case NS_MOUSE_MOVE :
				result = ConvertStatus(mMouseListener->MouseMoved(event));
				break;

			case NS_MOUSE_LEFT_BUTTON_DOWN :
			case NS_MOUSE_MIDDLE_BUTTON_DOWN :
			case NS_MOUSE_RIGHT_BUTTON_DOWN :
				result = ConvertStatus(mMouseListener->MousePressed(event));
				break;

			case NS_MOUSE_LEFT_BUTTON_UP :
			case NS_MOUSE_MIDDLE_BUTTON_UP :
			case NS_MOUSE_RIGHT_BUTTON_UP :
				result = ConvertStatus(mMouseListener->MouseReleased(event)) && ConvertStatus(mMouseListener->MouseClicked(event));
				break;
			}
			NS_RELEASE(event.widget);
			return result;
		}
	}

	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Deal with focus messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchFocus(PRUint32 aEventType)
{
	// call the event callback
	if (mEventCallback) {
		return(DispatchStandardEvent(aEventType));
	}

	return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll()
{
	return PR_FALSE;
}

NS_METHOD nsWindow::SetTitle(const nsAString& aTitle)
{
	const char *text = ToNewUTF8String(aTitle);
	if(text && mView->LockLooper())
	{
		mView->Window()->SetTitle(text);
		mView->UnlockLooper();
	}
	delete [] text;
	return NS_OK;
}

PRBool nsWindow::AutoErase()
{
	return(PR_FALSE);
}


//----------------------------------------------------
//
// Get/Set the preferred size
//
//----------------------------------------------------
NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
	// TODO:  Check to see how often this is called.  If too much, leave as is,
	// otherwise, call mView->GetPreferredSize
	aWidth  = mPreferredWidth;
	aHeight = mPreferredHeight;
	return NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
	mPreferredWidth  = aWidth;
	mPreferredHeight = aHeight;
	return NS_OK;
}

//----------------------------------------------------
// Special Sub-Class
//----------------------------------------------------
nsIWidgetStore::nsIWidgetStore( nsIWidget *aWidget )
		: mWidget( aWidget )
{
	// NS_ADDREF/NS_RELEASE is not needed here.
	// This class is used as internal (BeOS native) object of nsWindow,
	// so it must not addref/release nsWindow here.
	// Otherwise, nsWindow object will leak. (Makoto Hamanaka)
}

nsIWidgetStore::~nsIWidgetStore()
{
}

nsIWidget *nsIWidgetStore::GetMozillaWidget(void)
{
	return mWidget;
}

//----------------------------------------------------
// BeOS Sub-Class Window
//----------------------------------------------------

nsWindowBeOS::nsWindowBeOS( nsIWidget *aWidgetWindow, BRect aFrame, const char *aName, window_look aLook,
                            window_feel aFeel, int32 aFlags, int32 aWorkspace )
		: BWindow( aFrame, aName, aLook, aFeel, aFlags, aWorkspace ),
		nsIWidgetStore( aWidgetWindow )
{
	//placeholder for initialization
}

nsWindowBeOS::~nsWindowBeOS()
{
	//placeholder for clean up
}

bool nsWindowBeOS::QuitRequested( void )
{
	// tells nsWindow to kill me
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = nsnull;
		if(nsnull != (info = new MethodInfo(w, w, nsWindow::CLOSEWINDOW)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
	return false;
}

void nsWindowBeOS::MessageReceived(BMessage *msg)
{
	//Placeholder for possible improvements, e.g. DND and quitting
	BWindow::MessageReceived(msg);
}

// This function calls KeyDown() for Alt+whatever instead of app_server
void nsWindowBeOS::DispatchMessage(BMessage *msg, BHandler *handler)
{
	if (msg->what == B_KEY_DOWN && modifiers() & B_COMMAND_KEY) {
		BString bytes;
		if (B_OK == msg->FindString("bytes", &bytes)) {
			BView *view = this->CurrentFocus();
			if (view)
				view->KeyDown(bytes.String(), bytes.Length());
		}
	}
	BWindow::DispatchMessage(msg, handler);
}

//This method serves single purpose here - allows Mozilla to save current window position,
//and restore position on new start. 
void nsWindowBeOS::FrameMoved(BPoint origin)
{	

	//determine if the window position actually changed
	if (origin.x == lastWindowPoint.x && origin.x == lastWindowPoint.x) 
	{
		//it didn't - don't bother
		return;
	}
	lastWindowPoint = origin;
	nsWindow  *w = (nsWindow *)GetMozillaWidget();
	nsToolkit *t;
	if(w && (t = w->GetToolkit()) != 0) 
	{
		uint32 args[2];
		args[0] = (uint32)origin.x;
		args[1] = (uint32)origin.y;
		MethodInfo *info = nsnull;
		if(nsnull != (info = new MethodInfo(w, w, nsWindow::ONMOVE, 2, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void nsWindowBeOS::WindowActivated(bool active)
{
// Calls method ONACTIVATE to dispatch focus ACTIVATE messages
	nsWindow        *w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32	args[2];
		args[0] = (uint32)active;
		args[1] = (uint32)this;
		MethodInfo *info = nsnull;
		if(nsnull != (info = new MethodInfo(w, w, nsWindow::ONACTIVATE, 2, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void  nsWindowBeOS::WorkspacesChanged(uint32 oldworkspace, uint32 newworkspace)
{
	if (oldworkspace == newworkspace)
		return;
	nsWindow        *w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32	args[2];
		args[0] = newworkspace;
		args[1] = oldworkspace;
		MethodInfo *info = nsnull;
		if(nsnull != (info = new MethodInfo(w, w, nsWindow::ONWORKSPACE, 2, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}	
}

//----------------------------------------------------
// BeOS Sub-Class View
//----------------------------------------------------

nsViewBeOS::nsViewBeOS(nsIWidget *aWidgetWindow, BRect aFrame, const char *aName, uint32 aResizingMode, uint32 aFlags)
		: BView(aFrame, aName, aResizingMode, aFlags), nsIWidgetStore(aWidgetWindow), buttons(0)
{
}

void nsViewBeOS::AttachedToWindow()
{
	lastViewWidth = Bounds().Width();
	lastViewHeight = Bounds().Height();
	nsWindow *w = (nsWindow *)GetMozillaWidget();
	SetHighColor(255, 255, 255);
	FillRect(Bounds());
	if(! w->AutoErase())
		// view shouldn't erase
		SetViewColor(B_TRANSPARENT_32_BIT);
}

void nsViewBeOS::Draw(BRect updateRect)
{
	DoDraw(updateRect);
}

void nsViewBeOS::DrawAfterChildren(BRect updateRect)
{
//Stub for B_DRAW_ON_CHILDREN flags
//If some problem appears, line below may be uncommented.

	//DoDraw(updateRect);
} 

void nsViewBeOS::DoDraw(BRect updateRect)
{
	//Collecting update rects here in paintregion
	paintregion.Include(updateRect);
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = nsnull;
		info = new MethodInfo(w, w, nsWindow::ONPAINT);
		if(info)
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

// Method to get update rects for asynchronous drawing.
bool nsViewBeOS::GetPaintRegion(BRegion *r)
{
	if(paintregion.CountRects() == 0)
		return false;
	r->Include(&paintregion);
	paintregion.MakeEmpty();
	return true;
}

void nsViewBeOS::MouseDown(BPoint point)
{
	SetMouseEventMask(B_POINTER_EVENTS | B_KEYBOARD_EVENTS);
	uint32 clicks = 0;
	Window()->CurrentMessage()->FindInt32("buttons", (int32 *)&buttons);
	Window()->CurrentMessage()->FindInt32("clicks", (int32 *)&clicks);

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		if(0 != buttons)
		{
			int32 ev = (buttons & B_PRIMARY_MOUSE_BUTTON) ? NS_MOUSE_LEFT_BUTTON_DOWN :
			           ((buttons & B_SECONDARY_MOUSE_BUTTON) ? NS_MOUSE_RIGHT_BUTTON_DOWN :
			            NS_MOUSE_MIDDLE_BUTTON_DOWN);
			uint32	args[5];
			args[0] = ev;
			args[1] = (uint32)point.x;
			args[2] = (uint32)point.y;
			args[3] = clicks;
			args[4] = modifiers();
			MethodInfo *info = nsnull;
			if(nsnull != (info = new MethodInfo(w, w, nsWindow::BTNCLICK, 5, args)))
				t->CallMethodAsync(info);
		}
		NS_RELEASE(t);
	}
}

void nsViewBeOS::MouseMoved(BPoint point, uint32 transit, const BMessage *msg)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32	args[4];
		args[1] = (uint32)point.x;
		args[2] = (uint32)point.y;
		args[3] = modifiers();

		if(transit == B_ENTERED_VIEW)
		{
			args[0] = NS_MOUSE_ENTER;
			MethodInfo *enterInfo = nsnull;
			if(nsnull != (enterInfo = new MethodInfo(w, w, nsWindow::ONMOUSE, 4, args)))
				t->CallMethodAsync(enterInfo);
		}

		args[0] = NS_MOUSE_MOVE;
		MethodInfo *moveInfo = nsnull;
		if(nsnull != (moveInfo = new MethodInfo(w, w, nsWindow::ONMOUSE, 4, args)))
			t->CallMethodAsync(moveInfo);

		if(transit == B_EXITED_VIEW)
		{
			args[0] = NS_MOUSE_EXIT;
			MethodInfo *exitInfo = nsnull;
			if(nsnull != (exitInfo = new MethodInfo(w, w, nsWindow::ONMOUSE, 4, args)))
				t->CallMethodAsync(exitInfo);
		}
		NS_RELEASE(t);
	}
}

void nsViewBeOS::MouseUp(BPoint point)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		if(0 != buttons)
		{
			int32 ev = (buttons & B_PRIMARY_MOUSE_BUTTON) ? NS_MOUSE_LEFT_BUTTON_UP :
			           ((buttons & B_SECONDARY_MOUSE_BUTTON) ? NS_MOUSE_RIGHT_BUTTON_UP :
			            NS_MOUSE_MIDDLE_BUTTON_UP);
			uint32	args[5];
			args[0] = ev;
			args[1] = (uint32)point.x;
			args[2] = (int32)point.y;
			args[3] = 0;
			args[4] = modifiers();
			MethodInfo *info = nsnull;
			if(nsnull != (info = new MethodInfo(w, w, nsWindow::BTNCLICK, 5, args)))
				t->CallMethodAsync(info);
		}
		NS_RELEASE(t);
	}
	buttons = 0;
}

void nsViewBeOS::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
	case B_UNMAPPED_KEY_DOWN:
		//printf("unmapped_key_down\n");
		this->KeyDown(NULL, 0);
		break;

	case B_UNMAPPED_KEY_UP:
		//printf("unmapped_key_up\n");
		this->KeyUp(NULL, 0);
		break;

	case B_MOUSE_WHEEL_CHANGED:
		{
			float wheel_y;

			msg->FindFloat ("be:wheel_delta_y", &wheel_y);

			// make sure there *was* movement on the y axis
			if(wheel_y == 0)
				break;

			nsWindow    *w = (nsWindow *)GetMozillaWidget();
			nsToolkit   *t;

			if(w && (t = w->GetToolkit()) != 0)
			{
				uint32 args[1];
				if (wheel_y > 0)
					args[0] = (uint32)3;
				else
					args[0] = (uint32)-3;
				MethodInfo *info = nsnull;
				if(nsnull != (info = new MethodInfo(w, w, nsWindow::ONWHEEL, 1, args)))
					t->CallMethodAsync(info);
				NS_RELEASE(t);
			}
		}
		break;

	default :
		BView::MessageReceived(msg);
		break;
	}
}

void nsViewBeOS::KeyDown(const char *bytes, int32 numBytes)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	int32 keycode = 0;
	int32 rawcode = 0;

	BMessage *msg = this->Window()->CurrentMessage();
	if (msg) {
		msg->FindInt32("key", &keycode);
		msg->FindInt32("raw_char", &rawcode);
	}

	if(w && (t = w->GetToolkit()) != 0) {
		uint32 bytebuf = 0;
		uint8 *byteptr = (uint8 *)&bytebuf;
		for(int32 i = 0; i < numBytes; i++)
			byteptr[i] = bytes[i];

		uint32	args[6];
		args[0] = NS_KEY_DOWN;
		args[1] = bytebuf;
		args[2] = numBytes;
		args[3] = modifiers();
		args[4] = keycode;
		args[5] = rawcode;
		MethodInfo *info = nsnull;
		if(nsnull != (info = new MethodInfo(w, w, nsWindow::ONKEY, 6, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void nsViewBeOS::KeyUp(const char *bytes, int32 numBytes)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	int32 keycode = 0;
	int32 rawcode = 0;
	BMessage *msg = this->Window()->CurrentMessage();
	if (msg) {
		msg->FindInt32("key", &keycode);
		msg->FindInt32("raw_char", &rawcode);
	}

	if(w && (t = w->GetToolkit()) != 0) {
		uint32 bytebuf = 0;
		uint8 *byteptr = (uint8 *)&bytebuf;
		for(int32 i = 0; i < numBytes; i++)
			byteptr[i] = bytes[i];

		uint32	args[6];
		args[0] = NS_KEY_UP;
		args[1] = (int32)bytebuf;
		args[2] = numBytes;
		args[3] = modifiers();
		args[4] = keycode;
		args[5] = rawcode;
		MethodInfo *info = nsnull;
		if(nsnull != (info = new MethodInfo(w, w, nsWindow::ONKEY, 6, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void nsViewBeOS::MakeFocus(bool focused)
{
	if (!IsFocus() && focused)
		BView::MakeFocus(focused);

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = nsnull;
		if(nsnull != (info = new MethodInfo(w, w, (focused)? nsWindow::GOT_FOCUS : nsWindow::KILL_FOCUS)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void nsViewBeOS::FrameResized(float width, float height)
{
	//determine if the window size actually changed
	if (width==lastViewWidth && height==lastViewHeight)
	{
		//it didn't - don't bother
		return;
	}
	//remember new size
	lastViewWidth=width;
	lastViewHeight=height;
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0) 
	{
		//the window's new size needs to be passed to OnResize() -
		//note that the values passed to FrameResized() are the
		//dimensions as reported by Bounds().Width() and
		//Bounds().Height(), which are one pixel smaller than the
		//window's true size
		uint32 args[2];
		args[0]=(uint32)width+1;
		args[1]=(uint32)height+1;

		MethodInfo *info = 0;
		if (nsnull != (info = new MethodInfo(w, w, nsWindow::ONRESIZE, 2, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}
void nsViewBeOS::FrameMoved(BPoint origin)
{
	//determine if the window position actually changed
	if (origin.x == lastViewPoint.x && origin.x == lastViewPoint.x)
		return;
	lastViewPoint.x = origin.x;
	lastViewPoint.y = origin.y;
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0) 
	{
		uint32 args[2];
		args[0] = (uint32)origin.x;
		args[1] = (uint32)origin.y;
		MethodInfo *info = 0;
		if (nsnull != (info = new MethodInfo(w, w, nsWindow::ONMOVE, 2, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}
