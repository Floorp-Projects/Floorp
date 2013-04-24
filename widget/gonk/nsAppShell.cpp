/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 sts=4 tw=80 et: */
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

#include "base/basictypes.h"
#include "nscore.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Hal.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "nsAppShell.h"
#include "mozilla/dom/Touch.h"
#include "nsGkAtoms.h"
#include "nsGUIEvent.h"
#include "nsIObserverService.h"
#include "nsIScreen.h"
#include "nsScreenManagerGonk.h"
#include "nsWindow.h"
#include "OrientationObserver.h"
#include "GonkMemoryPressureMonitoring.h"

#include "android/log.h"
#include "libui/EventHub.h"
#include "libui/InputReader.h"
#include "libui/InputDispatcher.h"

#include "GeckoProfiler.h"

// Defines kKeyMapping and GetKeyNameIndex()
#include "GonkKeyMapping.h"

#define LOG(args...)                                            \
    __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#ifdef VERBOSE_LOG_ENABLED
# define VERBOSE_LOG(args...)                           \
    __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#else
# define VERBOSE_LOG(args...)                   \
    (void)0
#endif

using namespace android;
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::services;
using namespace mozilla::widget;

bool gDrawRequest = false;
static nsAppShell *gAppShell = NULL;
static int epollfd = 0;
static int signalfds[2] = {0};

NS_IMPL_ISUPPORTS_INHERITED1(nsAppShell, nsBaseAppShell, nsIObserver)

namespace mozilla {

bool ProcessNextEvent()
{
    return gAppShell->ProcessNextNativeEvent(true);
}

void NotifyEvent()
{
    gAppShell->NotifyNativeEvent();
}

} // namespace mozilla

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
            ::Touch touches[MAX_POINTERS];
        } motion;
    };
};

static void
sendMouseEvent(uint32_t msg, uint64_t timeMs, int x, int y, bool forwardToChildren)
{
    nsMouseEvent event(true, msg, NULL,
                       nsMouseEvent::eReal, nsMouseEvent::eNormal);

    event.refPoint.x = x;
    event.refPoint.y = y;
    event.time = timeMs;
    event.button = nsMouseEvent::eLeftButton;
    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    if (msg != NS_MOUSE_MOVE)
        event.clickCount = 1;

    event.mFlags.mNoCrossProcessBoundaryForwarding = !forwardToChildren;

    nsWindow::DispatchInputEvent(event);
}

static void
addDOMTouch(UserInputData& data, nsTouchEvent& event, int i)
{
    const ::Touch& touch = data.motion.touches[i];
    event.touches.AppendElement(
        new dom::Touch(touch.id,
                       nsIntPoint(touch.coords.getX(), touch.coords.getY()),
                       nsIntPoint(touch.coords.getAxisValue(AMOTION_EVENT_AXIS_SIZE),
                                  touch.coords.getAxisValue(AMOTION_EVENT_AXIS_SIZE)),
                       0,
                       touch.coords.getAxisValue(AMOTION_EVENT_AXIS_PRESSURE))
    );
}

static nsEventStatus
sendTouchEvent(UserInputData& data, bool* captured)
{
    uint32_t msg;
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

    return nsWindow::DispatchInputEvent(event, captured);
}

static nsEventStatus
sendKeyEventWithMsg(uint32_t keyCode,
                    KeyNameIndex keyNameIndex,
                    uint32_t msg,
                    uint64_t timeMs,
                    const EventFlags& flags)
{
    nsKeyEvent event(true, msg, NULL);
    event.keyCode = keyCode;
    event.mKeyNameIndex = keyNameIndex;
    event.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_MOBILE;
    event.time = timeMs;
    event.mFlags.Union(flags);
    return nsWindow::DispatchInputEvent(event);
}

static void
sendKeyEvent(uint32_t keyCode, KeyNameIndex keyNameIndex, bool down,
             uint64_t timeMs)
{
    EventFlags extraFlags;
    nsEventStatus status =
        sendKeyEventWithMsg(keyCode, keyNameIndex,
                            down ? NS_KEY_DOWN : NS_KEY_UP, timeMs, extraFlags);
    if (down) {
        extraFlags.mDefaultPrevented =
            (status == nsEventStatus_eConsumeNoDefault);
        sendKeyEventWithMsg(keyCode, keyNameIndex, NS_KEY_PRESS, timeMs,
                            extraFlags);
    }
}

static void
maybeSendKeyEvent(int keyCode, bool pressed, uint64_t timeMs)
{
    uint32_t DOMKeyCode =
        (keyCode < ArrayLength(kKeyMapping)) ? kKeyMapping[keyCode] : 0;
    KeyNameIndex DOMKeyNameIndex = GetKeyNameIndex(keyCode);
    if (DOMKeyCode || DOMKeyNameIndex != KEY_NAME_INDEX_Unidentified) {
        sendKeyEvent(DOMKeyCode, DOMKeyNameIndex, pressed, timeMs);
    } else {
        VERBOSE_LOG("Got unknown key event code. type 0x%04x code 0x%04x value %d",
                    keyCode, pressed);
    }
}

class GeckoPointerController : public PointerControllerInterface {
    float mX;
    float mY;
    int32_t mButtonState;
    InputReaderConfiguration* mConfig;
public:
    GeckoPointerController(InputReaderConfiguration* config)
        : mX(0)
        , mY(0)
        , mButtonState(0)
        , mConfig(config)
    {}

    virtual bool getBounds(float* outMinX, float* outMinY,
            float* outMaxX, float* outMaxY) const;
    virtual void move(float deltaX, float deltaY);
    virtual void setButtonState(int32_t buttonState);
    virtual int32_t getButtonState() const;
    virtual void setPosition(float x, float y);
    virtual void getPosition(float* outX, float* outY) const;
    virtual void fade(Transition transition) {}
    virtual void unfade(Transition transition) {}
    virtual void setPresentation(Presentation presentation) {}
    virtual void setSpots(const PointerCoords* spotCoords, const uint32_t* spotIdToIndex,
            BitSet32 spotIdBits) {}
    virtual void clearSpots() {}
};

bool
GeckoPointerController::getBounds(float* outMinX,
                                  float* outMinY,
                                  float* outMaxX,
                                  float* outMaxY) const
{
    int32_t width, height, orientation;

    mConfig->getDisplayInfo(0, false, &width, &height, &orientation);

    *outMinX = *outMinY = 0;
    if (orientation == DISPLAY_ORIENTATION_90 ||
        orientation == DISPLAY_ORIENTATION_270) {
        *outMaxX = height;
        *outMaxY = width;
    } else {
        *outMaxX = width;
        *outMaxY = height;
    }
    return true;
}

void
GeckoPointerController::move(float deltaX, float deltaY)
{
    float minX, minY, maxX, maxY;
    getBounds(&minX, &minY, &maxX, &maxY);

    mX = clamped(mX + deltaX, minX, maxX);
    mY = clamped(mY + deltaY, minY, maxY);
}

void
GeckoPointerController::setButtonState(int32_t buttonState)
{
    mButtonState = buttonState;
}

int32_t
GeckoPointerController::getButtonState() const
{
    return mButtonState;
}

void
GeckoPointerController::setPosition(float x, float y)
{
    mX = x;
    mY = y;
}

void
GeckoPointerController::getPosition(float* outX, float* outY) const
{
    *outX = mX;
    *outY = mY;
}

class GeckoInputReaderPolicy : public InputReaderPolicyInterface {
    InputReaderConfiguration mConfig;
public:
    GeckoInputReaderPolicy() {}

    virtual void getReaderConfiguration(InputReaderConfiguration* outConfig);
    virtual sp<PointerControllerInterface> obtainPointerController(int32_t
deviceId)
    {
        return new GeckoPointerController(&mConfig);
    };
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
        nsEventStatus status = nsEventStatus_eIgnore;
        if ((data.action & AMOTION_EVENT_ACTION_MASK) !=
            AMOTION_EVENT_ACTION_HOVER_MOVE) {
            bool captured;
            status = sendTouchEvent(data, &captured);
            if (captured) {
                return;
            }
        }

        uint32_t msg;
        switch (data.action & AMOTION_EVENT_ACTION_MASK) {
        case AMOTION_EVENT_ACTION_DOWN:
            msg = NS_MOUSE_BUTTON_DOWN;
            break;
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_UP:
        case AMOTION_EVENT_ACTION_MOVE:
        case AMOTION_EVENT_ACTION_HOVER_MOVE:
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
                       data.motion.touches[0].coords.getY(),
                       status != nsEventStatus_eConsumeNoDefault);
        break;
    }
    case UserInputData::KEY_DATA:
        maybeSendKeyEvent(data.key.keyCode,
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
        ::Touch& touch = data.motion.touches[i];
        touch.id = args->pointerProperties[i].id;
        memcpy(&touch.coords, &args->pointerCoords[i], sizeof(*args->pointerCoords));
    }
    {
        MutexAutoLock lock(mQueueLock);
        if (!mEventQueue.empty() &&
             mEventQueue.back().type == UserInputData::MOTION_DATA &&
           ((mEventQueue.back().action & AMOTION_EVENT_ACTION_MASK) ==
             AMOTION_EVENT_ACTION_MOVE ||
            (mEventQueue.back().action & AMOTION_EVENT_ACTION_MASK) ==
             AMOTION_EVENT_ACTION_HOVER_MOVE))
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

nsAppShell::nsAppShell()
    : mNativeCallbackRequest(false)
    , mHandlers()
    , mEnableDraw(false)
{
    gAppShell = this;
}

nsAppShell::~nsAppShell()
{
    // mReaderThread and mEventHub will both be null if InitInputDevices
    // is not called.
    if (mReaderThread.get()) {
        // We separate requestExit() and join() here so we can wake the EventHub's
        // input loop, and stop it from polling for input events
        mReaderThread->requestExit();
        mEventHub->wake();

        status_t result = mReaderThread->requestExitAndWait();
        if (result)
            LOG("Could not stop reader thread - %d", result);
    }
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

    InitGonkMemoryPressureMonitoring();

    nsCOMPtr<nsIObserverService> obsServ = GetObserverService();
    if (obsServ) {
        obsServ->AddObserver(this, "browser-ui-startup-complete", false);
    }

    // Delay initializing input devices until the screen has been
    // initialized (and we know the resolution).
    return rv;
}

NS_IMETHODIMP
nsAppShell::Observe(nsISupports* aSubject,
                    const char* aTopic,
                    const PRUnichar* aData)
{
    if (strcmp(aTopic, "browser-ui-startup-complete")) {
        return nsBaseAppShell::Observe(aSubject, aTopic, aData);
    }

    mEnableDraw = true;
    NotifyEvent();
    return NS_OK;
}

NS_IMETHODIMP
nsAppShell::Exit()
{
    OrientationObserver::ShutDown();
    nsCOMPtr<nsIObserverService> obsServ = GetObserverService();
    if (obsServ) {
        obsServ->RemoveObserver(this, "browser-ui-startup-complete");
    }
    return nsBaseAppShell::Exit();
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
    PROFILER_LABEL("nsAppShell", "ProcessNextNativeEvent");
    epoll_event events[16] = {{ 0 }};

    int event_count;
    {
        PROFILER_LABEL("nsAppShell", "ProcessNextNativeEvent::Wait");
        if ((event_count = epoll_wait(epollfd, events, 16,  mayWait ? -1 : 0)) <= 0)
            return true;
    }

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

    if (gDrawRequest && mEnableDraw) {
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

    // Getting the instance of OrientationObserver to initialize it.
    OrientationObserver::GetInstance();
}

/* static */ void
nsAppShell::NotifyScreenRotation()
{
    gAppShell->mReaderPolicy->setDisplayInfo();
    gAppShell->mReader->requestRefreshConfiguration(InputReaderConfiguration::CHANGE_DISPLAY_INFO);

    hal::NotifyScreenConfigurationChange(nsScreenGonk::GetConfiguration());
}
