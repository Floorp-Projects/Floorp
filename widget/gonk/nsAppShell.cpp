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
#include "ui/EventHub.h"
#include "ui/InputReader.h"
#include "ui/InputDispatcher.h"

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
    event.isShift = false;
    event.isControl = false;
    event.isMeta = false;
    event.isAlt = false;
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
                       nsIntPoint(touch.coords.x, touch.coords.y),
                       nsIntPoint(touch.coords.size, touch.coords.size),
                       0,
                       touch.coords.pressure));
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
    event.isShift = false;
    event.isControl = false;
    event.isMeta = false;
    event.isAlt = false;

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
sendSpecialKeyEvent(nsIAtom *command, uint64_t timeMs)
{
    nsCommandEvent event(true, nsGkAtoms::onAppCommand, command, NULL);
    event.time = timeMs;
    nsWindow::DispatchInputEvent(event);
}

static void
maybeSendKeyEvent(int keyCode, bool pressed, uint64_t timeMs)
{
    switch (keyCode) {
    case KEY_BACK:
        sendKeyEvent(NS_VK_ESCAPE, pressed, timeMs);
        break;
    case KEY_MENU:
        if (!pressed)
            sendSpecialKeyEvent(nsGkAtoms::Menu, timeMs);
        break;
    case KEY_SEARCH:
        if (pressed)
            sendSpecialKeyEvent(nsGkAtoms::Search, timeMs);
        break;
    case KEY_HOME:
        sendKeyEvent(NS_VK_HOME, pressed, timeMs);
        break;
    case KEY_POWER:
        sendKeyEvent(NS_VK_SLEEP, pressed, timeMs);
        break;
    case KEY_VOLUMEUP:
        if (pressed)
            sendSpecialKeyEvent(nsGkAtoms::VolumeUp, timeMs);
        break;
    case KEY_VOLUMEDOWN:
        if (pressed)
            sendSpecialKeyEvent(nsGkAtoms::VolumeDown, timeMs);
        break;
    default:
        VERBOSE_LOG("Got unknown key event code. type 0x%04x code 0x%04x value %d",
                    keyCode, pressed);
    }
}

class GeckoInputReaderPolicy : public InputReaderPolicyInterface {
public:
    GeckoInputReaderPolicy() {}

    virtual bool getDisplayInfo(int32_t displayId,
            int32_t* width, int32_t* height, int32_t* orientation);
    virtual bool filterTouchEvents();
    virtual bool filterJumpyTouchEvents();
    virtual nsecs_t getVirtualKeyQuietTime();
    virtual void getVirtualKeyDefinitions(const String8& deviceName,
            Vector<VirtualKeyDefinition>& outVirtualKeyDefinitions);
    virtual void getInputDeviceCalibration(const String8& deviceName,
            InputDeviceCalibration& outCalibration);
    virtual void getExcludedDeviceNames(Vector<String8>& outExcludedDeviceNames);

protected:
    virtual ~GeckoInputReaderPolicy() {}
};

class GeckoInputDispatcher : public InputDispatcherInterface {
public:
    GeckoInputDispatcher()
        : mQueueLock("GeckoInputDispatcher::mQueueMutex")
    {}

    virtual void dump(String8& dump);

    // Called on the main thread
    virtual void dispatchOnce();

    // notify* methods are called on the InputReaderThread
    virtual void notifyConfigurationChanged(nsecs_t eventTime);
    virtual void notifyKey(nsecs_t eventTime, int32_t deviceId, int32_t source,
            uint32_t policyFlags, int32_t action, int32_t flags, int32_t keyCode,
            int32_t scanCode, int32_t metaState, nsecs_t downTime);
    virtual void notifyMotion(nsecs_t eventTime, int32_t deviceId, int32_t source,
            uint32_t policyFlags, int32_t action, int32_t flags,
            int32_t metaState, int32_t edgeFlags,
            uint32_t pointerCount, const int32_t* pointerIds, const PointerCoords* pointerCoords,
            float xPrecision, float yPrecision, nsecs_t downTime);
    virtual void notifySwitch(nsecs_t when,
            int32_t switchCode, int32_t switchValue, uint32_t policyFlags);

    virtual int32_t injectInputEvent(const InputEvent* event,
            int32_t injectorPid, int32_t injectorUid, int32_t syncMode, int32_t timeoutMillis);
    virtual void setInputWindows(const Vector<InputWindow>& inputWindows);
    virtual void setFocusedApplication(const InputApplication* inputApplication);
    virtual void setInputDispatchMode(bool enabled, bool frozen);
    virtual status_t registerInputChannel(const sp<InputChannel>& inputChannel, bool monitor);
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
bool
GeckoInputReaderPolicy::getDisplayInfo(int32_t displayId,
                                       int32_t* width,
                                       int32_t* height,
                                       int32_t* orientation)
{
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_0_DEG ==
                      InputReaderPolicyInterface::ROTATION_0,
                      "Orientation enums not matched!");
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_90_DEG ==
                      InputReaderPolicyInterface::ROTATION_90,
                      "Orientation enums not matched!");
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_180_DEG ==
                      InputReaderPolicyInterface::ROTATION_180,
                      "Orientation enums not matched!");
    MOZ_STATIC_ASSERT(nsIScreen::ROTATION_270_DEG ==
                      InputReaderPolicyInterface::ROTATION_270,
                      "Orientation enums not matched!");

    // 0 is the default displayId. We only support one display
    if (displayId)
        return false;

    if (width)
        *width = gScreenBounds.width;
    if (height)
        *height = gScreenBounds.height;
    if (orientation)
        *orientation = nsScreenGonk::GetRotation();
    return true;
}

bool
GeckoInputReaderPolicy::filterTouchEvents()
{
    return false;
}

bool
GeckoInputReaderPolicy::filterJumpyTouchEvents()
{
    return false;
}

nsecs_t
GeckoInputReaderPolicy::getVirtualKeyQuietTime()
{
    return 0;
}

void
GeckoInputReaderPolicy::getVirtualKeyDefinitions(const String8& deviceName,
    Vector<VirtualKeyDefinition>& outVirtualKeyDefinitions)
{
    outVirtualKeyDefinitions.clear();

    char vbuttonsPath[PATH_MAX];
    snprintf(vbuttonsPath, sizeof(vbuttonsPath),
             "/sys/board_properties/virtualkeys.%s",
             deviceName.string());
    ScopedClose fd(open(vbuttonsPath, O_RDONLY));
    if (0 > fd.mFd) {
        LOG("No vbuttons for mt device %s", deviceName.string());
        return;
    }

    // This device has vbuttons.  Process the configuration.
    char config[1024];
    ssize_t nread;
    do {
        nread = read(fd.mFd, config, sizeof(config));
    } while (-1 == nread && EINTR == errno);

    if (0 > nread) {
        LOG("Error reading virtualkey configuration");
        return;
    }

    config[nread] = '\0';

    LOG("Device %s has vbutton config '%s'", deviceName.string(), config);

    char* first = config;
    char* magic;
    char* state;
    while ((magic = strtok_r(first, ":", &state))) {
        // XXX not clear what "0x01" is ... maybe a version
        // number?  See InputManager.java.
        if (strcmp(magic, "0x01")) {
            LOG("  magic 0x01 tag missing");
            break;
        }
        first = NULL;

        const char *scanCode, *centerX, *centerY, *width, *height;
        if (!((scanCode = strtok_r(NULL, ":", &state)) &&
              (centerX = strtok_r(NULL, ":", &state)) &&
              (centerY = strtok_r(NULL, ":", &state)) &&
              (width = strtok_r(NULL, ":", &state)) &&
              (height = strtok_r(NULL, ":", &state)))) {
            LOG("  failed to read bound params");
            break;
        }

        // NB: these coordinates are in *screen* space, not input
        // space.  That means the values in /sys/board_config make
        // assumptions about how the raw input events are mapped
        // ... le sigh.
        VirtualKeyDefinition def;
        def.scanCode = atoi(scanCode);
        def.centerX = atoi(centerX);
        def.centerY = atoi(centerY);
        def.width = atoi(width);
        def.height = atoi(height);
        outVirtualKeyDefinitions.push(def);

        LOG("  configured vbutton code=%d at <x=%d,y=%d,w=%d,h=%d>",
            def.scanCode, def.centerX, def.centerY, def.width, def.height);
    }
}

void
GeckoInputReaderPolicy::getInputDeviceCalibration(const String8& deviceName,            InputDeviceCalibration& outCalibration)
{
    outCalibration.clear();
}

void
GeckoInputReaderPolicy::getExcludedDeviceNames(Vector<String8>& outExcludedDeviceNames)
{
    outExcludedDeviceNames.clear();
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
                       data.motion.touches[0].coords.x,
                       data.motion.touches[0].coords.y);
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
GeckoInputDispatcher::notifyConfigurationChanged(nsecs_t eventTime)
{
}

static uint64_t
nanosecsToMillisecs(nsecs_t nsecs)
{
    return nsecs / 1000000;
}

void
GeckoInputDispatcher::notifyKey(nsecs_t eventTime,
                                int32_t deviceId,
                                int32_t source,
                                uint32_t policyFlags,
                                int32_t action,
                                int32_t flags,
                                int32_t keyCode,
                                int32_t scanCode,
                                int32_t metaState,
                                nsecs_t downTime)
{
    UserInputData data;
    data.timeMs = nanosecsToMillisecs(eventTime);
    data.type = UserInputData::KEY_DATA;
    data.action = action;
    data.flags = flags;
    data.metaState = metaState;
    data.key.keyCode = keyCode;
    data.key.scanCode = scanCode;
    {
        MutexAutoLock lock(mQueueLock);
        mEventQueue.push(data);
    }
    gAppShell->NotifyNativeEvent();
}

void
GeckoInputDispatcher::notifyMotion(nsecs_t eventTime,
                                   int32_t deviceId,
                                   int32_t source,
                                   uint32_t policyFlags,
                                   int32_t action,
                                   int32_t flags,
                                   int32_t metaState,
                                   int32_t edgeFlags,
                                   uint32_t pointerCount,
                                   const int32_t* pointerIds,
                                   const PointerCoords* pointerCoords,
                                   float xPrecision,
                                   float yPrecision,
                                   nsecs_t downTime)
{
    UserInputData data;
    data.timeMs = nanosecsToMillisecs(eventTime);
    data.type = UserInputData::MOTION_DATA;
    data.action = action;
    data.flags = flags;
    data.metaState = metaState;
    MOZ_ASSERT(pointerCount <= MAX_POINTERS);
    data.motion.touchCount = pointerCount;
    for (int32_t i = 0; i < pointerCount; ++i) {
        Touch& touch = data.motion.touches[i];
        touch.id = pointerIds[i];
        memcpy(&touch.coords, &pointerCoords[i], sizeof(*pointerCoords));
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

void
GeckoInputDispatcher::notifySwitch(nsecs_t when,
                                   int32_t switchCode,
                                   int32_t switchValue,
                                   uint32_t policyFlags)
{
}


int32_t
GeckoInputDispatcher::injectInputEvent(const InputEvent* event,
                                       int32_t injectorPid,
                                       int32_t injectorUid,
                                       int32_t syncMode,
                                       int32_t timeoutMillis)
{
    return INPUT_EVENT_INJECTION_SUCCEEDED;
}

void
GeckoInputDispatcher::setInputWindows(const Vector<InputWindow>& inputWindows)
{
}

void
GeckoInputDispatcher::setFocusedApplication(const InputApplication* inputApplication)
{
}

void
GeckoInputDispatcher::setInputDispatchMode(bool enabled, bool frozen)
{
}

status_t
GeckoInputDispatcher::registerInputChannel(const sp<InputChannel>& inputChannel,
                                           bool monitor)
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

    mEventHub = new EventHub();
    mReaderPolicy = new GeckoInputReaderPolicy();
    mDispatcher = new GeckoInputDispatcher();

    mReader = new InputReader(mEventHub, mReaderPolicy, mDispatcher);
    mReaderThread = new InputReaderThread(mReader);

    status_t result = mReaderThread->run("InputReader", PRIORITY_URGENT_DISPLAY);
    NS_ENSURE_FALSE(result, NS_ERROR_UNEXPECTED);
    return rv;
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

