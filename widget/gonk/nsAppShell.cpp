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
#include "GonkPermission.h"
#include "nscore.h"
#ifdef MOZ_OMX_DECODER
#include "MediaResourceManagerService.h"
#endif
#include "mozilla/TouchEvents.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Hal.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"
#include "mozilla/TextEvents.h"
#if ANDROID_VERSION >= 18
#include "nativewindow/FakeSurfaceComposer.h"
#endif
#include "nsAppShell.h"
#include "mozilla/dom/Touch.h"
#include "nsGkAtoms.h"
#include "nsIObserverService.h"
#include "nsIScreen.h"
#include "nsScreenManagerGonk.h"
#include "nsThreadUtils.h"
#include "nsWindow.h"
#include "OrientationObserver.h"
#include "GonkMemoryPressureMonitoring.h"

#include "android/log.h"
#include "libui/EventHub.h"
#include "libui/InputReader.h"
#include "libui/InputDispatcher.h"
#include "cutils/properties.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

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
static nsAppShell *gAppShell = nullptr;
static int epollfd = 0;
static int signalfds[2] = {0};
static bool sDevInputAudioJack;
static int32_t sHeadphoneState;
static int32_t sMicrophoneState;

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
    int32_t deviceId;
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
    WidgetMouseEvent event(true, msg, nullptr,
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

    event.refPoint.x = x;
    event.refPoint.y = y;
    event.time = timeMs;
    event.button = WidgetMouseEvent::eLeftButton;
    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
    if (msg != NS_MOUSE_MOVE)
        event.clickCount = 1;

    event.mFlags.mNoCrossProcessBoundaryForwarding = !forwardToChildren;

    nsWindow::DispatchInputEvent(event);
}

static void
addDOMTouch(UserInputData& data, WidgetTouchEvent& event, int i)
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

    WidgetTouchEvent event(true, msg, nullptr);

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

class MOZ_STACK_CLASS KeyEventDispatcher
{
public:
    KeyEventDispatcher(const UserInputData& aData,
                       KeyCharacterMap* aKeyCharMap);
    void Dispatch();

private:
    const UserInputData& mData;
    sp<KeyCharacterMap> mKeyCharMap;

    uint32_t mDOMKeyCode;
    KeyNameIndex mDOMKeyNameIndex;
    PRUnichar mDOMPrintableKeyValue;

    bool IsKeyPress() const
    {
        return mData.action == AKEY_EVENT_ACTION_DOWN;
    }
    bool IsRepeat() const
    {
        return IsKeyPress() && (mData.flags & AKEY_EVENT_FLAG_LONG_PRESS);
    }

    uint32_t CharCode() const;
    PRUnichar PrintableKeyValue() const;

    void DispatchKeyDownEvent();
    void DispatchKeyUpEvent();
    nsEventStatus DispatchKeyEventInternal(uint32_t aEventMessage);
};

KeyEventDispatcher::KeyEventDispatcher(const UserInputData& aData,
                                       KeyCharacterMap* aKeyCharMap) :
    mData(aData), mKeyCharMap(aKeyCharMap)
{
    // XXX Printable key's keyCode value should be computed with actual
    //     input character.
    mDOMKeyCode = (mData.key.keyCode < ArrayLength(kKeyMapping)) ?
        kKeyMapping[mData.key.keyCode] : 0;
    mDOMKeyNameIndex = GetKeyNameIndex(mData.key.keyCode);
    mDOMPrintableKeyValue = PrintableKeyValue();
}

uint32_t
KeyEventDispatcher::CharCode() const
{
    if (!mKeyCharMap.get()) {
        return 0;
    }
    // XXX If the charCode is not a printable character, the charCode should be
    //     computed without Ctrl/Alt/Meta modifiers.
    char16_t ch = mKeyCharMap->getCharacter(mData.key.keyCode, mData.metaState);
    return (ch >= ' ') ? static_cast<uint32_t>(ch) : 0;
}

PRUnichar
KeyEventDispatcher::PrintableKeyValue() const
{
    if (mDOMKeyNameIndex != KEY_NAME_INDEX_USE_STRING || !mKeyCharMap.get()) {
        return 0;
    }
    char16_t ch = mKeyCharMap->getCharacter(mData.key.keyCode, mData.metaState);
    if (ch >= ' ') {
        return static_cast<PRUnichar>(ch);
    }
    int32_t unmodifiedMetaState = mData.metaState &
        ~(AMETA_ALT_ON | AMETA_ALT_LEFT_ON | AMETA_ALT_RIGHT_ON |
          AMETA_CTRL_ON | AMETA_CTRL_LEFT_ON | AMETA_CTRL_RIGHT_ON |
          AMETA_META_ON | AMETA_META_LEFT_ON | AMETA_META_RIGHT_ON);
    if (unmodifiedMetaState == mData.metaState) {
        return 0;
    }
    ch = mKeyCharMap->getCharacter(mData.key.keyCode, unmodifiedMetaState);
    return (ch >= ' ') ? static_cast<PRUnichar>(ch) : 0;
}

nsEventStatus
KeyEventDispatcher::DispatchKeyEventInternal(uint32_t aEventMessage)
{
    WidgetKeyboardEvent event(true, aEventMessage, nullptr);
    if (aEventMessage == NS_KEY_PRESS) {
        event.charCode = CharCode();
    }
    if (!event.charCode) {
        event.keyCode = mDOMKeyCode;
    }
    event.isChar = !!event.charCode;
    event.mIsRepeat = IsRepeat();
    event.mKeyNameIndex = mDOMKeyNameIndex;
    if (mDOMPrintableKeyValue) {
        event.mKeyValue = mDOMPrintableKeyValue;
    }
    event.location = nsIDOMKeyEvent::DOM_KEY_LOCATION_MOBILE;
    event.time = mData.timeMs;
    return nsWindow::DispatchInputEvent(event);
}

void
KeyEventDispatcher::Dispatch()
{
    // XXX Even if unknown key is pressed, DOM key event should be
    //     dispatched since Gecko for the other platforms are implemented
    //     as so.
    if (!mDOMKeyCode && mDOMKeyNameIndex == KEY_NAME_INDEX_Unidentified) {
        VERBOSE_LOG("Got unknown key event code. "
                    "type 0x%04x code 0x%04x value %d",
                    mData.action, mData.key.keyCode, IsKeyPress());
        return;
    }

    if (IsKeyPress()) {
        DispatchKeyDownEvent();
    } else {
        DispatchKeyUpEvent();
    }
}

void
KeyEventDispatcher::DispatchKeyDownEvent()
{
    nsEventStatus status = DispatchKeyEventInternal(NS_KEY_DOWN);
    if (status != nsEventStatus_eConsumeNoDefault) {
        DispatchKeyEventInternal(NS_KEY_PRESS);
    }
}

void
KeyEventDispatcher::DispatchKeyUpEvent()
{
    DispatchKeyEventInternal(NS_KEY_UP);
}

class SwitchEventRunnable : public nsRunnable {
public:
    SwitchEventRunnable(hal::SwitchEvent& aEvent) : mEvent(aEvent)
    {}

    NS_IMETHOD Run()
    {
        hal::NotifySwitchChange(mEvent);
        return NS_OK;
    }
private:
    hal::SwitchEvent mEvent;
};

static void
updateHeadphoneSwitch()
{
    hal::SwitchEvent event;

    switch (sHeadphoneState) {
    case AKEY_STATE_UP:
        event.status() = hal::SWITCH_STATE_OFF;
        break;
    case AKEY_STATE_DOWN:
        event.status() = sMicrophoneState == AKEY_STATE_DOWN ?
            hal::SWITCH_STATE_HEADPHONE : hal::SWITCH_STATE_HEADSET;
        break;
    default:
        return;
    }

    event.device() = hal::SWITCH_HEADPHONES;
    NS_DispatchToMainThread(new SwitchEventRunnable(event));
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
    DisplayViewport viewport;

    mConfig->getDisplayInfo(false, &viewport);

    *outMinX = *outMinY = 0;
    *outMaxX = viewport.logicalRight;
    *outMaxY = viewport.logicalBottom;
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
    virtual void notifyInputDevicesChanged(const android::Vector<InputDeviceInfo>& inputDevices) {};
    virtual sp<KeyCharacterMap> getKeyboardLayoutOverlay(const String8& inputDeviceDescriptor)
    {
        return nullptr;
    };
    virtual String8 getDeviceAlias(const InputDeviceIdentifier& identifier)
    {
        return String8::empty();
    };

    void setDisplayInfo();

protected:
    virtual ~GeckoInputReaderPolicy() {}
};

class GeckoInputDispatcher : public InputDispatcherInterface {
public:
    GeckoInputDispatcher(sp<EventHub> &aEventHub)
        : mQueueLock("GeckoInputDispatcher::mQueueMutex")
        , mEventHub(aEventHub)
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

    virtual void setInputWindows(const android::Vector<sp<InputWindowHandle> >& inputWindowHandles);
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
    sp<EventHub> mEventHub;
};

// GeckoInputReaderPolicy
void
GeckoInputReaderPolicy::setDisplayInfo()
{
    static_assert(nsIScreen::ROTATION_0_DEG ==
                  DISPLAY_ORIENTATION_0,
                  "Orientation enums not matched!");
    static_assert(nsIScreen::ROTATION_90_DEG ==
                  DISPLAY_ORIENTATION_90,
                  "Orientation enums not matched!");
    static_assert(nsIScreen::ROTATION_180_DEG ==
                  DISPLAY_ORIENTATION_180,
                  "Orientation enums not matched!");
    static_assert(nsIScreen::ROTATION_270_DEG ==
                  DISPLAY_ORIENTATION_270,
                  "Orientation enums not matched!");

    DisplayViewport viewport;
    viewport.displayId = 0;
    viewport.orientation = nsScreenGonk::GetRotation();
    viewport.physicalRight = viewport.deviceWidth = gScreenBounds.width;
    viewport.physicalBottom = viewport.deviceHeight = gScreenBounds.height;
    if (viewport.orientation == DISPLAY_ORIENTATION_90 ||
        viewport.orientation == DISPLAY_ORIENTATION_270) {
        viewport.logicalRight = gScreenBounds.height;
        viewport.logicalBottom = gScreenBounds.width;
    } else {
        viewport.logicalRight = gScreenBounds.width;
        viewport.logicalBottom = gScreenBounds.height;
    }
    mConfig.setDisplayInfo(false, viewport);
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
    case UserInputData::KEY_DATA: {
        sp<KeyCharacterMap> kcm = mEventHub->getKeyCharacterMap(data.deviceId);
        KeyEventDispatcher dispatcher(data, kcm.get());
        dispatcher.Dispatch();
        break;
    }
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
    data.deviceId = args->deviceId;
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
    data.deviceId = args->deviceId;
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
    if (!sDevInputAudioJack)
        return;

    bool needSwitchUpdate = false;

    if (args->switchMask & (1 << SW_HEADPHONE_INSERT)) {
        sHeadphoneState = (args->switchValues & (1 << SW_HEADPHONE_INSERT)) ?
                          AKEY_STATE_DOWN : AKEY_STATE_UP;
        needSwitchUpdate = true;
    }

    if (args->switchMask & (1 << SW_MICROPHONE_INSERT)) {
        sMicrophoneState = (args->switchValues & (1 << SW_MICROPHONE_INSERT)) ?
                           AKEY_STATE_DOWN : AKEY_STATE_UP;
        needSwitchUpdate = true;
    }

    if (needSwitchUpdate)
        updateHeadphoneSwitch();
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
GeckoInputDispatcher::setInputWindows(const android::Vector<sp<InputWindowHandle> >& inputWindowHandles)
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
    , mEnableDraw(false)
    , mHandlers()
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
    gAppShell = nullptr;
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

    if (XRE_GetProcessType() == GeckoProcessType_Default) {
#ifdef MOZ_OMX_DECODER
        android::MediaResourceManagerService::instantiate();
#endif
#if ANDROID_VERSION >= 18
        android::FakeSurfaceComposer::instantiate();
#endif
        GonkPermissionService::instantiate();
    }

    nsCOMPtr<nsIObserverService> obsServ = GetObserverService();
    if (obsServ) {
        obsServ->AddObserver(this, "browser-ui-startup-complete", false);
    }

#ifdef MOZ_NUWA_PROCESS
    // Make sure main thread was woken up after Nuwa fork.
    NuwaAddConstructor((void (*)(void *))&NotifyEvent, nullptr);
#endif

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

    if (sDevInputAudioJack) {
        sHeadphoneState  = mReader->getSwitchState(-1, AINPUT_SOURCE_SWITCH, SW_HEADPHONE_INSERT);
        sMicrophoneState = mReader->getSwitchState(-1, AINPUT_SOURCE_SWITCH, SW_MICROPHONE_INSERT);
        updateHeadphoneSwitch();
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
    char value[PROPERTY_VALUE_MAX];
    property_get("ro.moz.devinputjack", value, "0");
    sDevInputAudioJack = !strcmp(value, "1");
    sHeadphoneState = AKEY_STATE_UNKNOWN;
    sMicrophoneState = AKEY_STATE_UNKNOWN;

    mEventHub = new EventHub();
    mReaderPolicy = new GeckoInputReaderPolicy();
    mReaderPolicy->setDisplayInfo();
    mDispatcher = new GeckoInputDispatcher(mEventHub);

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
