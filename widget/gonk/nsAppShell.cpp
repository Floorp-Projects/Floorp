/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 sts=4 tw=80 et: */
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
 * The Original Code is Gonk.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
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

#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "mozilla/Hal.h"
#include "nscore.h"
#include "mozilla/FileUtils.h"
#include "mozilla/HalSensor.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "nsAppShell.h"
#include "nsDOMTouchEvent.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"
#include "nsIObserverService.h"
#include "nsIScreen.h"
#include "nsScreenManagerGonk.h"
#include "nsWindow.h"

#include "android/log.h"
#include "libui/EventHub.h"
#include "libui/InputReader.h"
#include "libui/InputDispatcher.h"

#define LOG(args...)                                            \
    __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#ifdef VERBOSE_LOG_ENABLED
# define VERBOSE_LOG(args...)                           \
    __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#else
# define VERBOSE_LOG(args...)                   \
    (void)0
#endif

using namespace mozilla;
using namespace android;
using namespace hal;

bool gDrawRequest = false;
static nsAppShell *gAppShell = NULL;
static int epollfd = 0;
static int signalfds[2] = {0};

namespace mozilla {

bool ProcessNextEvent()
{
    return gAppShell->ProcessNextNativeEvent(true);
}

void NotifyEvent()
{
    gAppShell->NotifyNativeEvent();
}

}

static void
pipeHandler(int fd, FdHandler *data)
{
    ssize_t len;
    do {
        char tmp[32];
        len = read(fd, tmp, sizeof(tmp));
    } while (len > 0);
}

struct Touch {
    int32_t id;
    PointerCoords coords;
};

struct UserInputData {
    uint64_t timeMs;
    enum {
        MOTION_DATA,
        KEY_DATA
    } type;
    int32_t action;
    int32_t flags;
    int32_t metaState;
    union {
        struct {
            int32_t keyCode;
            int32_t scanCode;
        } key;
        struct {
            int32_t touchCount;
            Touch touches[MAX_POINTERS];
        } motion;
    };
};

static void
sendMouseEvent(PRUint32 msg, uint64_t timeMs, int x, int y)
{
    nsMouseEvent event(true, msg, NULL,
                       nsMouseEvent::eReal, nsMouseEvent::eNormal);

    event.refPoint.x = x;
    event.refPoint.y = y;
    event.time = timeMs;
    event.button = nsMouseEvent::eLeftButton;
    if (msg != NS_MOUSE_MOVE)
        event.clickCount = 1;

    nsWindow::DispatchInputEvent(event);
}

static void
addDOMTouch(UserInputData& data, nsTouchEvent& event, int i)
{
    const Touch& touch = data.motion.touches[i];
    event.touches.AppendElement(
        new nsDOMTouch(touch.id,
                       nsIntPoint(touch.coords.getX(), touch.coords.getY()),
                       nsIntPoint(touch.coords.getAxisValue(AMOTION_EVENT_AXIS_SIZE),
                                  touch.coords.getAxisValue(AMOTION_EVENT_AXIS_SIZE)),
                       0,
                       touch.coords.getAxisValue(AMOTION_EVENT_AXIS_PRESSURE))
    );
}

static nsEventStatus
sendTouchEvent(UserInputData& data)
{
    PRUint32 msg;
    int32_t action = data.action & AMOTION_EVENT_ACTION_MASK;
    switch (action) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        msg = NS_TOUCH_START;
        break;
    case AMOTION_EVENT_ACTION_MOVE:
        msg = NS_TOUCH_MOVE;
        break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
        msg = NS_TOUCH_END;
        break;
    case AMOTION_EVENT_ACTION_OUTSIDE:
    case AMOTION_EVENT_ACTION_CANCEL:
        msg = NS_TOUCH_CANCEL;
        break;
    }

    nsTouchEvent event(true, msg, NULL);

    event.time = data.timeMs;

    int32_t i;
    if (msg == NS_TOUCH_END) {
        i = data.action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK;
        i >>= AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        addDOMTouch(data, event, i);
    } else {
        for (i = 0; i < data.motion.touchCount; ++i)
            addDOMTouch(data, event, i);
    }

    return nsWindow::DispatchInputEvent(event);
}

static nsEventStatus
sendKeyEventWithMsg(PRUint32 keyCode,
                    PRUint32 msg,
                    uint64_t timeMs,
                    PRUint32 flags)
{
    nsKeyEvent event(true, msg, NULL);
    event.keyCode = keyCode;
    event.time = timeMs;
    event.flags |= flags;
    return nsWindow::DispatchInputEvent(event);
}

static void
sendKeyEvent(PRUint32 keyCode, bool down, uint64_t timeMs)
{
    nsEventStatus status =
        sendKeyEventWithMsg(keyCode, down ? NS_KEY_DOWN : NS_KEY_UP, timeMs, 0);
    if (down) {
        sendKeyEventWithMsg(keyCode, NS_KEY_PRESS, timeMs,
                            status == nsEventStatus_eConsumeNoDefault ?
                            NS_EVENT_FLAG_NO_DEFAULT : 0);
    }
}

static void
maybeSendKeyEvent(int keyCode, bool pressed, uint64_t timeMs)
{
    switch (keyCode) {
    case KEY_BACK:
        sendKeyEvent(NS_VK_ESCAPE, pressed, timeMs);
        break;
    case KEY_MENU:
         sendKeyEvent(NS_VK_CONTEXT_MENU, pressed, timeMs);
        break;
    case KEY_SEARCH:
        sendKeyEvent(NS_VK_F5, pressed, timeMs);
        break;
    case KEY_HOME:
        sendKeyEvent(NS_VK_HOME, pressed, timeMs);
        break;
    case KEY_POWER:
        sendKeyEvent(NS_VK_SLEEP, pressed, timeMs);
        break;
    case KEY_VOLUMEUP:
        sendKeyEvent(NS_VK_PAGE_UP, pressed, timeMs);
        break;
    case KEY_VOLUMEDOWN:
        sendKeyEvent(NS_VK_PAGE_DOWN, pressed, timeMs);
        break;
    default:
        VERBOSE_LOG("Got unknown key event code. type 0x%04x code 0x%04x value %d",
                    keyCode, pressed);
    }
}

class GeckoInputReaderPolicy : public InputReaderPolicyInterface {
    InputReaderConfiguration mConfig;
public:
    GeckoInputReaderPolicy() {}

    virtual void getReaderConfiguration(InputReaderConfiguration* outConfig);
    virtual sp<PointerControllerInterface> obtainPointerController(int32_t
deviceId) { return NULL; };
    void setDisplayInfo();

protected:
    virtual ~GeckoInputReaderPolicy() {}
};

class GeckoInputDispatcher : public InputDispatcherInterface {
public:
    GeckoInputDispatcher()
        : mQueueLock("GeckoInputDispatcher::mQueueMutex")
    {}

    virtual void dump(String8& dump);

    virtual void monitor() {}

    // Called on the main thread
    virtual void dispatchOnce();

    // notify* methods are called on the InputReaderThread
    virtual void notifyConfigurationChanged(const NotifyConfigurationChangedArgs* args);
    virtual void notifyKey(const NotifyKeyArgs* args);
    virtual void notifyMotion(const NotifyMotionArgs* args);
    virtual void notifySwitch(const NotifySwitchArgs* args);
    virtual void notifyDeviceReset(const NotifyDeviceResetArgs* args);

    virtual int32_t injectInputEvent(const InputEvent* event,
            int32_t injectorPid, int32_t injectorUid, int32_t syncMode, int32_t timeoutMillis,
            uint32_t policyFlags);

    virtual void setInputWindows(const Vector<sp<InputWindowHandle> >& inputWindowHandles);
    virtual void setFocusedApplication(const sp<InputApplicationHandle>& inputApplicationHandle);

    virtual void setInputDispatchMode(bool enabled, bool frozen);
    virtual void setInputFilterEnabled(bool enabled) {}
    virtual bool transferTouchFocus(const sp<InputChannel>& fromChannel,
            const sp<InputChannel>& toChannel) { return true; }

    virtual status_t registerInputChannel(const sp<InputChannel>& inputChannel,
            const sp<InputWindowHandle>& inputWindowHandle, bool monitor);
    virtual status_t unregisterInputChannel(const sp<InputChannel>& inputChannel);



protected:
    virtual ~GeckoInputDispatcher() {}

private:
    // mQueueLock should generally be locked while using mEventQueue.
    // UserInputData is pushed on on the InputReaderThread and
    // popped and dispatched on the main thread.
    mozilla::Mutex mQueueLock;
    std::queue<UserInputData> mEventQueue;
};

// GeckoInputReaderPolicy
void
GeckoInputReaderPolicy::setDisplayInfo()
{
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_0_DEG ==
                      DISPLAY_ORIENTATION_0,
                      "Orientation enums not matched!");
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_90_DEG ==
                      DISPLAY_ORIENTATION_90,
                      "Orientation enums not matched!");
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_180_DEG ==
                      DISPLAY_ORIENTATION_180,
                      "Orientation enums not matched!");
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_270_DEG ==
                      DISPLAY_ORIENTATION_270,
                      "Orientation enums not matched!");

    mConfig.setDisplayInfo(0, false, gScreenBounds.width, gScreenBounds.height, nsScreenGonk::GetRotation());
}

void GeckoInputReaderPolicy::getReaderConfiguration(InputReaderConfiguration* outConfig)
{
    *outConfig = mConfig;
}


// GeckoInputDispatcher
void
GeckoInputDispatcher::dump(String8& dump)
{
}

void
GeckoInputDispatcher::dispatchOnce()
{
    UserInputData data;
    {
        MutexAutoLock lock(mQueueLock);
        if (mEventQueue.empty())
            return;
        data = mEventQueue.front();
        mEventQueue.pop();
        if (!mEventQueue.empty())
            gAppShell->NotifyNativeEvent();
    }

    switch (data.type) {
    case UserInputData::MOTION_DATA: {
        nsEventStatus status = sendTouchEvent(data);
        if (status == nsEventStatus_eConsumeNoDefault)
            break;

        PRUint32 msg;
        switch (data.action & AMOTION_EVENT_ACTION_MASK) {
        case AMOTION_EVENT_ACTION_DOWN:
            msg = NS_MOUSE_BUTTON_DOWN;
            break;
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_MOVE:
            msg = NS_MOUSE_MOVE;
            break;
        case AMOTION_EVENT_ACTION_OUTSIDE:
        case AMOTION_EVENT_ACTION_CANCEL:
        case AMOTION_EVENT_ACTION_UP:
            msg = NS_MOUSE_BUTTON_UP;
            break;
        }
        sendMouseEvent(msg,
                       data.timeMs,
                       data.motion.touches[0].coords.getX(),
                       data.motion.touches[0].coords.getY());
        break;
    }
    case UserInputData::KEY_DATA:
        maybeSendKeyEvent(data.key.scanCode,
                          data.action == AKEY_EVENT_ACTION_DOWN,
                          data.timeMs);
        break;
    }
}


void
GeckoInputDispatcher::notifyConfigurationChanged(const NotifyConfigurationChangedArgs*)
{
}

static uint64_t
nanosecsToMillisecs(nsecs_t nsecs)
{
    return nsecs / 1000000;
}

void
GeckoInputDispatcher::notifyKey(const NotifyKeyArgs* args)
{
    UserInputData data;
    data.timeMs = nanosecsToMillisecs(args->eventTime);
    data.type = UserInputData::KEY_DATA;
    data.action = args->action;
    data.flags = args->flags;
    data.metaState = args->metaState;
    data.key.keyCode = args->keyCode;
    data.key.scanCode = args->scanCode;
    {
        MutexAutoLock lock(mQueueLock);
        mEventQueue.push(data);
    }
    gAppShell->NotifyNativeEvent();
}


void
GeckoInputDispatcher::notifyMotion(const NotifyMotionArgs* args)
{
    UserInputData data;
    data.timeMs = nanosecsToMillisecs(args->eventTime);
    data.type = UserInputData::MOTION_DATA;
    data.action = args->action;
    data.flags = args->flags;
    data.metaState = args->metaState;
    MOZ_ASSERT(args->pointerCount <= MAX_POINTERS);
    data.motion.touchCount = args->pointerCount;
    for (uint32_t i = 0; i < args->pointerCount; ++i) {
        Touch& touch = data.motion.touches[i];
        touch.id = args->pointerProperties[i].id;
        memcpy(&touch.coords, &args->pointerCoords[i], sizeof(*args->pointerCoords));
    }
    {
        MutexAutoLock lock(mQueueLock);
        if (!mEventQueue.empty() &&
             mEventQueue.back().type == UserInputData::MOTION_DATA &&
            (mEventQueue.back().action & AMOTION_EVENT_ACTION_MASK) ==
             AMOTION_EVENT_ACTION_MOVE)
            mEventQueue.back() = data;
        else
            mEventQueue.push(data);
    }
    gAppShell->NotifyNativeEvent();
}



void GeckoInputDispatcher::notifySwitch(const NotifySwitchArgs* args)
{
}

void GeckoInputDispatcher::notifyDeviceReset(const NotifyDeviceResetArgs* args)
{
}

int32_t GeckoInputDispatcher::injectInputEvent(
    const InputEvent* event,
    int32_t injectorPid, int32_t injectorUid, int32_t syncMode,
    int32_t timeoutMillis, uint32_t policyFlags)
{
    return INPUT_EVENT_INJECTION_SUCCEEDED;
}

void
GeckoInputDispatcher::setInputWindows(const Vector<sp<InputWindowHandle> >& inputWindowHandles)
{
}

void
GeckoInputDispatcher::setFocusedApplication(const sp<InputApplicationHandle>& inputApplicationHandle)
{
}

void
GeckoInputDispatcher::setInputDispatchMode(bool enabled, bool frozen)
{
}

status_t
GeckoInputDispatcher::registerInputChannel(const sp<InputChannel>& inputChannel,
                                           const sp<InputWindowHandle>& inputWindowHandle, bool monitor)
{
    return OK;
}

status_t
GeckoInputDispatcher::unregisterInputChannel(const sp<InputChannel>& inputChannel)
{
    return OK;
}

class ScreenRotateEvent : public nsRunnable {
public:
  ScreenRotateEvent(nsIScreen* aScreen, PRUint32 aRotation)
    : mScreen(aScreen),
      mRotation(aRotation) {
  }
  NS_IMETHOD Run() {
    return mScreen->SetRotation(mRotation);
  }

private:
  nsCOMPtr<nsIScreen> mScreen;
  PRUint32 mRotation;
};

class OrientationSensorObserver : public ISensorObserver {
public:
  OrientationSensorObserver ()
    : mLastUpdate(0) {
  }
  void Notify(const SensorData& aSensorData) {
    nsCOMPtr<nsIScreenManager> screenMgr =
        do_GetService("@mozilla.org/gfx/screenmanager;1");
    nsCOMPtr<nsIScreen> screen;
    screenMgr->GetPrimaryScreen(getter_AddRefs(screen));

    MOZ_ASSERT(aSensorData.sensor() == SensorType::SENSOR_ORIENTATION);
    InfallibleTArray<float> values = aSensorData.values();
    // float azimuth = values[0]; // unused
    float pitch = values[1];
    float roll = values[2];
    PRUint32 rotation;
    if (roll > 45)
      rotation = nsIScreen::ROTATION_90_DEG;
    else if (roll < -45)
      rotation = nsIScreen::ROTATION_270_DEG;
    else if (pitch < -45)
      rotation = nsIScreen::ROTATION_0_DEG;
    else if (pitch > 45)
      rotation = nsIScreen::ROTATION_180_DEG;
    else
      return;  // don't rotate if undecidable

    PRUint32 currRotation;
    nsresult res;
    res = screen->GetRotation(&currRotation);
    if (NS_FAILED(res) || rotation == currRotation)
      return;

    PRTime now = PR_Now();
    MOZ_ASSERT(now > mLastUpdate);
    if (now - mLastUpdate < sMinUpdateInterval)
      return;

    mLastUpdate = now;
    NS_DispatchToMainThread(new ScreenRotateEvent(screen, rotation));

  }
private:
  PRTime mLastUpdate;
  static const PRTime sMinUpdateInterval = 500 * 1000; // 500 ms
};

nsAppShell::nsAppShell()
    : mNativeCallbackRequest(false)
    , mHandlers()
    , mObserver(new OrientationSensorObserver())
{
    gAppShell = this;
    RegisterSensorObserver(SENSOR_ORIENTATION, mObserver);
}

nsAppShell::~nsAppShell()
{
    UnregisterSensorObserver(SENSOR_ORIENTATION, mObserver);
    status_t result = mReaderThread->requestExitAndWait();
    if (result)
        LOG("Could not stop reader thread - %d", result);
    gAppShell = NULL;
}

nsresult
nsAppShell::Init()
{
    nsresult rv = nsBaseAppShell::Init();
    NS_ENSURE_SUCCESS(rv, rv);

    epollfd = epoll_create(16);
    NS_ENSURE_TRUE(epollfd >= 0, NS_ERROR_UNEXPECTED);

    int ret = pipe2(signalfds, O_NONBLOCK);
    NS_ENSURE_FALSE(ret, NS_ERROR_UNEXPECTED);

    rv = AddFdHandler(signalfds[0], pipeHandler, "");
    NS_ENSURE_SUCCESS(rv, rv);

    // Delay initializing input devices until the screen has been
    // initialized (and we know the resolution).
    return rv;
}

void
nsAppShell::InitInputDevices()
{
    mEventHub = new EventHub();
    mReaderPolicy = new GeckoInputReaderPolicy();
    mReaderPolicy->setDisplayInfo();
    mDispatcher = new GeckoInputDispatcher();

    mReader = new InputReader(mEventHub, mReaderPolicy, mDispatcher);
    mReaderThread = new InputReaderThread(mReader);

    status_t result = mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    if (result) {
        LOG("Failed to initialize InputReader thread, bad things are going to happen...");
    }
}

nsresult
nsAppShell::AddFdHandler(int fd, FdHandlerCallback handlerFunc,
                         const char* deviceName)
{
    epoll_event event = {
        EPOLLIN,
        { 0 }
    };

    FdHandler *handler = mHandlers.AppendElement();
    handler->fd = fd;
    strncpy(handler->name, deviceName, sizeof(handler->name) - 1);
    handler->func = handlerFunc;
    event.data.u32 = mHandlers.Length() - 1;
    return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) ?
           NS_ERROR_UNEXPECTED : NS_OK;
}

void
nsAppShell::ScheduleNativeEventCallback()
{
    mNativeCallbackRequest = true;
    NotifyEvent();
}

bool
nsAppShell::ProcessNextNativeEvent(bool mayWait)
{
    epoll_event events[16] = {{ 0 }};

    int event_count;
    if ((event_count = epoll_wait(epollfd, events, 16,  mayWait ? -1 : 0)) <= 0)
        return true;

    for (int i = 0; i < event_count; i++)
        mHandlers[events[i].data.u32].run();

    if (mDispatcher.get())
        mDispatcher->dispatchOnce();

    // NativeEventCallback always schedules more if it needs it
    // so we can coalesce these.
    // See the implementation in nsBaseAppShell.cpp for more info
    if (mNativeCallbackRequest) {
        mNativeCallbackRequest = false;
        NativeEventCallback();
    }

    if (gDrawRequest) {
        gDrawRequest = false;
        nsWindow::DoDraw();
    }

    return true;
}

void
nsAppShell::NotifyNativeEvent()
{
    write(signalfds[1], "w", 1);
}

/* static */ void
nsAppShell::NotifyScreenInitialized()
{
    gAppShell->InitInputDevices();
}

/* static */ void
nsAppShell::NotifyScreenRotation()
{
    gAppShell->mReaderPolicy->setDisplayInfo();
    gAppShell->mReader->requestRefreshConfiguration(InputReaderConfiguration::CHANGE_DISPLAY_INFO);
}
