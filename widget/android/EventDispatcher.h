/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=4 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_EventDispatcher_h
#define mozilla_widget_EventDispatcher_h

#include "GeneratedJNINatives.h"
#include "jsapi.h"
#include "nsClassHashtable.h"
#include "nsCOMArray.h"
#include "nsIAndroidBridge.h"
#include "nsHashKeys.h"
#include "nsPIDOMWindow.h"

#include "mozilla/Mutex.h"

namespace mozilla {
namespace widget {

/**
 * EventDispatcher is the Gecko counterpart to the Java EventDispatcher class.
 * Together, they make up a unified event bus. Events dispatched from the Java
 * side may notify event listeners on the Gecko side, and vice versa.
 */
class EventDispatcher final
    : public nsIAndroidEventDispatcher
    , public java::EventDispatcher::Natives<EventDispatcher>
{
    using NativesBase = java::EventDispatcher::Natives<EventDispatcher>;

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIANDROIDEVENTDISPATCHER

    EventDispatcher() {}

    void Attach(java::EventDispatcher::Param aDispatcher,
                nsPIDOMWindowOuter* aDOMWindow);
    void Detach();

    using NativesBase::DisposeNative;

    bool HasGeckoListener(jni::String::Param aEvent);
    void DispatchToGecko(jni::String::Param aEvent,
                         jni::Object::Param aData,
                         jni::Object::Param aCallback);

private:
    java::EventDispatcher::GlobalRef mDispatcher;
    nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;

    virtual ~EventDispatcher() {}

    struct ListenersList {
        nsCOMArray<nsIAndroidEventListener> listeners{/* count */ 1};
        // 0 if the list can be modified
        uint32_t lockCount{0};
        // true if this list has a listener that is being unregistered
        bool unregistering{false};
    };

    using ListenersMap = nsClassHashtable<nsStringHashKey, ListenersList>;

    Mutex mLock{"mozilla::widget::EventDispatcher"};
    ListenersMap mListenersMap;

    using IterateEventsCallback = nsresult (EventDispatcher::*)
            (const nsAString&, nsIAndroidEventListener*);

    nsresult IterateEvents(JSContext* aCx, JS::HandleValue aEvents,
                           IterateEventsCallback aCallback,
                           nsIAndroidEventListener* aListener);
    nsresult RegisterEventLocked(const nsAString&, nsIAndroidEventListener*);
    nsresult UnregisterEventLocked(const nsAString&, nsIAndroidEventListener*);

    nsresult DispatchOnGecko(ListenersList* list, const nsAString& aEvent,
                             JS::HandleValue aData,
                             nsIAndroidEventCallback* aCallback);
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_EventDispatcher_h
