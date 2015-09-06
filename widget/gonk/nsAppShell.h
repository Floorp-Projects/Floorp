/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef nsAppShell_h
#define nsAppShell_h

#include <queue>

#include "mozilla/Mutex.h"
#include "nsBaseAppShell.h"
#include "nsTArray.h"

#include "utils/RefBase.h"

namespace mozilla {
bool ProcessNextEvent();
void NotifyEvent();
}

extern bool gDrawRequest;

class FdHandler;
typedef void(*FdHandlerCallback)(int, FdHandler *);

class FdHandler {
public:
    FdHandler()
    {
        memset(name, 0, sizeof(name));
    }

    int fd;
    char name[64];
    FdHandlerCallback func;
    void run()
    {
        func(fd, this);
    }
};

namespace android {
class EventHub;
class InputReader;
class InputReaderThread;
}

class GeckoInputReaderPolicy;
class GeckoInputDispatcher;

class nsAppShell : public nsBaseAppShell {
public:
    nsAppShell();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOBSERVER

    nsresult Init();

    NS_IMETHOD Exit() override;

    virtual bool ProcessNextNativeEvent(bool maywait);

    void NotifyNativeEvent();

    static void NotifyScreenInitialized();
    static void NotifyScreenRotation();

    void CheckPowerKey();

protected:
    virtual ~nsAppShell();

    virtual void ScheduleNativeEventCallback();

private:
    nsresult AddFdHandler(int fd, FdHandlerCallback handlerFunc,
                          const char* deviceName);
    void InitInputDevices();

    // This is somewhat racy but is perfectly safe given how the callback works
    bool mNativeCallbackRequest;

    // This gets flipped when we observe a browser-ui-startup-complete.
    // browser-ui-startup-complete means that we're really ready to draw
    // and can stop the boot animation
    bool mEnableDraw;
    nsTArray<FdHandler> mHandlers;

    android::sp<android::EventHub>               mEventHub;
    android::sp<GeckoInputReaderPolicy> mReaderPolicy;
    android::sp<GeckoInputDispatcher>   mDispatcher;
    android::sp<android::InputReader>            mReader;
    android::sp<android::InputReaderThread>      mReaderThread;

    // Guard against checking power key after the first configuration change.
    bool mPowerKeyChecked;
};

#endif /* nsAppShell_h */

