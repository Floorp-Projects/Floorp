/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
    AltWindowHandling.cpp
    
    Due to a hole in the OJI API, 6.x still not able to open a new
    window from Java w/o a hack.  thePluginManager2->RegisterWindow
    is doing nothing.  Here I add Patrick's workaround used in 4.x.
    File will be used from TopLevelFrame::showHide()
    This code comes from the CPluginManager class in BackwardAdaptor.cpp.
    If the API is fixed, can remove this file and the two calls to it.
    
 */

#include <Controls.h>
#include <Events.h>

#include "nsIPluginManager2.h"
#include "EventFilter.h"
#include "nsIEventHandler.h"

#include "AltWindowHandling.h"

RegisteredWindow* theRegisteredWindows = NULL;
RegisteredWindow* theActiveWindow = NULL;
Boolean mEventFiltersInstalled = nil;

Boolean EventFilter(EventRecord* event);
Boolean MenuFilter(long menuSelection);
RegisteredWindow** GetRegisteredWindow(nsPluginPlatformWindowRef window);

NS_METHOD
AltRegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
    theRegisteredWindows = new RegisteredWindow(theRegisteredWindows, handler, window);
    
#ifdef XP_MAC
    // use jGNE to obtain events for registered windows.
    if (!mEventFiltersInstalled) {
        ::InstallEventFilters(&EventFilter, &MenuFilter);
        mEventFiltersInstalled = true;
    }

    // plugin expects the window to be shown and selected at this point.
    
    SInt16 variant = ::GetWVariant(window);
    if (variant == plainDBox) {
        ::ShowHide(window, true);
        ::BringToFront(window);
    } else {
        ::ShowWindow(window);
        ::SelectWindow(window);
    }
#endif

    return NS_OK;
}

NS_METHOD
AltUnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
    RegisteredWindow** link = GetRegisteredWindow(window);
    if (link != NULL) {
        RegisteredWindow* registeredWindow = *link;
        if (registeredWindow == theActiveWindow)
            theActiveWindow = NULL;
        *link = registeredWindow->mNext;
        delete registeredWindow;
    }

#ifdef XP_MAC
    ::HideWindow(window);

    // if no windows registered, remove the filter.
    if (theRegisteredWindows == NULL) {
        ::RemoveEventFilters();
        mEventFiltersInstalled = false;
    }
#endif

    return NS_OK;
}

static void sendActivateEvent(nsIEventHandler* handler, WindowRef window, Boolean active)
{
    EventRecord event;
    ::OSEventAvail(0, &event);
    event.what = activateEvt;
    event.message = UInt32(window);
    if (active)
        event.modifiers |= activeFlag;
    else
        event.modifiers &= ~activeFlag;

    nsPluginEvent pluginEvent = { &event, window };
    PRBool handled = PR_FALSE;

    handler->HandleEvent(&pluginEvent, &handled);
}

RegisteredWindow** GetRegisteredWindow(nsPluginPlatformWindowRef window)
{
    RegisteredWindow** link = &theRegisteredWindows;
    RegisteredWindow* registeredWindow = *link;
    while (registeredWindow != NULL) {
        if (registeredWindow->mWindow == window)
            return link;
        link = &registeredWindow->mNext;
        registeredWindow = *link;
    }
    return NULL;
}
RegisteredWindow* FindRegisteredWindow(nsPluginPlatformWindowRef window);
RegisteredWindow* FindRegisteredWindow(nsPluginPlatformWindowRef window)
{
    RegisteredWindow** link = GetRegisteredWindow(window);
    return (link != NULL ? *link : NULL);
}

/**
 * This method filters events using a very low-level mechanism known as a jGNE filter.
 * This filter gets first crack at all events before they are returned by WaitNextEvent
 * or EventAvail. One trickiness is that the filter runs in all processes, so care
 * must be taken not to act on events if the browser's process isn't current.
 * So far, with activates, updates, and mouse clicks, it works quite well.
 */
Boolean EventFilter(EventRecord* event)
{
    Boolean filteredEvent = false;

    WindowRef window = WindowRef(event->message);
    nsPluginEvent pluginEvent = { event, window };
    EventRecord simulatedEvent;

    RegisteredWindow* registeredWindow;
    PRBool handled = PR_FALSE;
    
    // see if this event is for one of our registered windows.
    switch (event->what) {
    case nullEvent:
        // See if the frontmost window is one of our registered windows.
        // we want to somehow deliver mouse enter/leave events.
        window = ::FrontWindow();
        registeredWindow = FindRegisteredWindow(window);
        if (registeredWindow != NULL) {
            simulatedEvent = *event;
            simulatedEvent.what = nsPluginEventType_AdjustCursorEvent;
            pluginEvent.event = &simulatedEvent;
            pluginEvent.window = registeredWindow->mWindow;
            registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
        }
        break;
    case keyDown:
    case keyUp:
    case autoKey:
        // See if the frontmost window is one of our registered windows.
        window = ::FrontWindow();
        registeredWindow = FindRegisteredWindow(window);
        if (registeredWindow != NULL) {
            pluginEvent.window = window;
            registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
            filteredEvent = true;
        }
        break;
    case mouseDown:
        // use FindWindow to see if the click was in one our registered windows.
        short partCode = FindWindow(event->where, &window);
        switch (partCode) {
        case inContent:
        case inDrag:
        case inGrow:
        case inGoAway:
        case inZoomIn:
        case inZoomOut:
        case inCollapseBox:
        case inProxyIcon:
            registeredWindow = FindRegisteredWindow(window);
            if (registeredWindow != NULL) {
                // make sure this window has been activated before passing it the click.
                if (theActiveWindow == NULL) {
                    sendActivateEvent(registeredWindow->mHandler, window, true);
                    theActiveWindow = registeredWindow;
                }
                pluginEvent.window = window;
                registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
                filteredEvent = true;
            } else if (theActiveWindow != NULL) {
                // a click is going into an unregistered window, if we are active,
                // the browser doesn't seem to be generating a deactivate event.
                // I think this is because PowerPlant is managing the windows, dang it.
                window = theActiveWindow->mWindow;
                sendActivateEvent(theActiveWindow->mHandler, window, false);
                ::HiliteWindow(window, false);
                theActiveWindow = NULL;
            }
            break;
        }
        break;
    case activateEvt:
        registeredWindow = FindRegisteredWindow(window);
        if (registeredWindow != NULL) {
            registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
            filteredEvent = true;
            theActiveWindow = registeredWindow;
        }
        break;
    case updateEvt:
        registeredWindow = FindRegisteredWindow(window);
        if (registeredWindow != NULL) {
            GrafPtr port; GetPort(&port); SetPort(window); BeginUpdate(window);
                registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
            EndUpdate(window); SetPort(port);
            filteredEvent = true;
        }
        break;
    case osEvt:
        if ((event->message & osEvtMessageMask) == (suspendResumeMessage << 24)) {
            registeredWindow = theActiveWindow;
            if (registeredWindow != NULL) {
                window = registeredWindow->mWindow;
                Boolean active = (event->message & resumeFlag) != 0;
                sendActivateEvent(registeredWindow->mHandler, window, active);
                pluginEvent.window = window;
                registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
                ::HiliteWindow(window, active);
            }
        }
        break;
    }
    
    return filteredEvent;
}

// TODO:  find out what range of menus Communicator et. al. uses.
enum {
    kBaseMenuID = 20000,
    kBaseSubMenuID = 200
};

static PRInt16 nextMenuID = kBaseMenuID;
static PRInt16 nextSubMenuID = kBaseSubMenuID;

Boolean MenuFilter(long menuSelection)
{
    if (theActiveWindow != NULL) {
        UInt16 menuID = (menuSelection >> 16);
        if ((menuID >= kBaseMenuID && menuID < nextMenuID) || (menuID >= kBaseSubMenuID && menuID < nextSubMenuID)) {
            EventRecord menuEvent;
            ::OSEventAvail(0, &menuEvent);
            menuEvent.what = nsPluginEventType_MenuCommandEvent;
            menuEvent.message = menuSelection;

            WindowRef window = theActiveWindow->mWindow;
            nsPluginEvent pluginEvent = { &menuEvent, window };
            PRBool handled = PR_FALSE;
            theActiveWindow->mHandler->HandleEvent(&pluginEvent, &handled);
            
            return handled;
        }
    }
    return false;
}
