/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_GeckoEditableSupport_h
#define mozilla_widget_GeckoEditableSupport_h

#include "GeneratedJNIWrappers.h"
#include "nsAppShell.h"
#include "nsIWidget.h"
#include "nsTArray.h"

#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/UniquePtr.h"

class nsWindow;

namespace mozilla {

class TextComposition;

namespace widget {

class GeckoEditableSupport final
    : public TextEventDispatcherListener
    , public java::GeckoEditableChild::Natives<GeckoEditableSupport>
{
    /*
        Rules for managing IME between Gecko and Java:

        * Gecko controls the text content, and Java shadows the Gecko text
           through text updates
        * Gecko and Java maintain separate selections, and synchronize when
           needed through selection updates and set-selection events
        * Java controls the composition, and Gecko shadows the Java
           composition through update composition events
    */

    using EditableBase =
            java::GeckoEditableChild::Natives<GeckoEditableSupport>;

    // RAII helper class that automatically sends an event reply through
    // OnImeSynchronize, as required by events like OnImeReplaceText.
    class AutoIMESynchronize
    {
        GeckoEditableSupport* const mGES;
    public:
        AutoIMESynchronize(GeckoEditableSupport* ges) : mGES(ges) {}
        ~AutoIMESynchronize() { mGES->OnImeSynchronize(); }
    };

    struct IMETextChange final {
        int32_t mStart, mOldEnd, mNewEnd;

        IMETextChange() :
            mStart(-1), mOldEnd(-1), mNewEnd(-1) {}

        IMETextChange(const IMENotification& aIMENotification)
            : mStart(aIMENotification.mTextChangeData.mStartOffset)
            , mOldEnd(aIMENotification.mTextChangeData.mRemovedEndOffset)
            , mNewEnd(aIMENotification.mTextChangeData.mAddedEndOffset)
        {
            MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE,
                       "IMETextChange initialized with wrong notification");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsValid(),
                       "The text change notification isn't initialized");
            MOZ_ASSERT(aIMENotification.mTextChangeData.IsInInt32Range(),
                       "The text change notification is out of range");
        }

        bool IsEmpty() const { return mStart < 0; }
    };

    enum FlushChangesFlag
    {
        // Not retrying.
        FLUSH_FLAG_NONE,
        // Retrying due to IME text changes during flush.
        FLUSH_FLAG_RETRY,
        // Retrying due to IME sync exceptions during flush.
        FLUSH_FLAG_RECOVER
    };

    enum RemoveCompositionFlag
    {
        CANCEL_IME_COMPOSITION,
        COMMIT_IME_COMPOSITION
    };

    const bool mIsRemote;
    nsWindow::WindowPtr<GeckoEditableSupport> mWindow; // Parent only
    RefPtr<TextEventDispatcher> mDispatcher;
    java::GeckoEditableChild::GlobalRef mEditable;
    bool mEditableAttached;
    InputContext mInputContext;
    AutoTArray<UniquePtr<mozilla::WidgetEvent>, 4> mIMEKeyEvents;
    AutoTArray<IMETextChange, 4> mIMETextChanges;
    RefPtr<TextRangeArray> mIMERanges;
    int32_t mIMEMaskEventsCount; // Mask events when > 0.
    bool mIMEUpdatingContext;
    bool mIMESelectionChanged;
    bool mIMETextChangedDuringFlush;
    bool mIMEMonitorCursor;

    nsIWidget* GetWidget() const
    {
        return mDispatcher ? mDispatcher->GetWidget() : mWindow;
    }

    nsresult BeginInputTransaction(TextEventDispatcher* aDispatcher)
    {
        if (mIsRemote) {
            return aDispatcher->BeginInputTransaction(this);
        } else {
            return aDispatcher->BeginNativeInputTransaction();
        }
    }

    virtual ~GeckoEditableSupport() {}

    RefPtr<TextComposition> GetComposition() const;
    void RemoveComposition(
            RemoveCompositionFlag aFlag = COMMIT_IME_COMPOSITION);
    void SendIMEDummyKeyEvent(nsIWidget* aWidget, EventMessage msg);
    void AddIMETextChange(const IMETextChange& aChange);
    void PostFlushIMEChanges();
    void FlushIMEChanges(FlushChangesFlag aFlags = FLUSH_FLAG_NONE);
    void FlushIMEText(FlushChangesFlag aFlags = FLUSH_FLAG_NONE);
    void AsyncNotifyIME(int32_t aNotification);
    void UpdateCompositionRects();

public:
    template<typename Functor>
    static void OnNativeCall(Functor&& aCall)
    {
        struct IMEEvent : nsAppShell::LambdaEvent<Functor>
        {
            using Base = nsAppShell::LambdaEvent<Functor>;
            using Base::LambdaEvent;

            nsAppShell::Event::Type ActivityType() const override
            {
                using GES = GeckoEditableSupport;
                if (Base::lambda.IsTarget(&GES::OnKeyEvent) ||
                        Base::lambda.IsTarget(&GES::OnImeReplaceText) ||
                        Base::lambda.IsTarget(&GES::OnImeUpdateComposition)) {
                    return nsAppShell::Event::Type::kUIActivity;
                }
                return nsAppShell::Event::Type::kGeneralActivity;
            }

            void Run() override
            {
                if (!Base::lambda.GetNativeObject()) {
                    // Ignore stale calls after disposal.
                    jni::GetGeckoThreadEnv()->ExceptionClear();
                    return;
                }
                Base::Run();
            }
        };
        nsAppShell::PostEvent(mozilla::MakeUnique<IMEEvent>(
                mozilla::Move(aCall)));
    }

    // Constructor for main process GeckoEditableChild.
    GeckoEditableSupport(nsWindow::NativePtr<GeckoEditableSupport>* aPtr,
                         nsWindow* aWindow,
                         java::GeckoEditableChild::Param aEditableChild)
        : mIsRemote(!aWindow)
        , mWindow(aPtr, aWindow)
        , mEditable(aEditableChild)
        , mEditableAttached(!mIsRemote)
        , mIMERanges(new TextRangeArray())
        , mIMEMaskEventsCount(1) // Mask IME events since there's no focus yet
        , mIMEUpdatingContext(false)
        , mIMESelectionChanged(false)
        , mIMETextChangedDuringFlush(false)
        , mIMEMonitorCursor(false)
    {}

    // Constructor for content process GeckoEditableChild.
    GeckoEditableSupport(java::GeckoEditableChild::Param aEditableChild)
        : GeckoEditableSupport(nullptr, nullptr, aEditableChild)
    {}

    NS_DECL_ISUPPORTS

    // TextEventDispatcherListener methods
    NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                         const IMENotification& aNotification) override;

    NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;

    NS_IMETHOD_(void) OnRemovedFrom(
            TextEventDispatcher* aTextEventDispatcher) override;

    NS_IMETHOD_(void) WillDispatchKeyboardEvent(
            TextEventDispatcher* aTextEventDispatcher,
            WidgetKeyboardEvent& aKeyboardEvent,
            uint32_t aIndexOfKeypress,
            void* aData) override;

    void SetInputContext(const InputContext& aContext,
                         const InputContextAction& aAction);

    InputContext GetInputContext();

    // GeckoEditableChild methods
    using EditableBase::AttachNative;

    void OnDetach() {
        RefPtr<GeckoEditableSupport> self(this);
        nsAppShell::PostEvent([this, self] {
            mEditableAttached = false;
            DisposeNative(mEditable);
        });
    }

    // Handle an Android KeyEvent.
    void OnKeyEvent(int32_t aAction, int32_t aKeyCode, int32_t aScanCode,
                    int32_t aMetaState, int32_t aKeyPressMetaState,
                    int64_t aTime, int32_t aDomPrintableKeyValue,
                    int32_t aRepeatCount, int32_t aFlags,
                    bool aIsSynthesizedImeKey,
                    jni::Object::Param originalEvent);

    // Synchronize Gecko thread with the InputConnection thread.
    void OnImeSynchronize();

    // Replace a range of text with new text.
    void OnImeReplaceText(int32_t aStart, int32_t aEnd,
                          jni::String::Param aText);

    // Add styling for a range within the active composition.
    void OnImeAddCompositionRange(int32_t aStart, int32_t aEnd,
            int32_t aRangeType, int32_t aRangeStyle, int32_t aRangeLineStyle,
            bool aRangeBoldLine, int32_t aRangeForeColor,
            int32_t aRangeBackColor, int32_t aRangeLineColor);

    // Update styling for the active composition using previous-added ranges.
    void OnImeUpdateComposition(int32_t aStart, int32_t aEnd, int32_t aFlags);

    // Set cursor mode whether IME requests
    void OnImeRequestCursorUpdates(int aRequestMode);
};

} // namespace widget
} // namespace mozill

#endif // mozilla_widget_GeckoEditableSupport_h
