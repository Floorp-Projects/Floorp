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
    , public java::GeckoEditable::Natives<GeckoEditableSupport>
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

    using EditableBase = java::GeckoEditable::Natives<GeckoEditableSupport>;

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

    nsWindow::WindowPtr<GeckoEditableSupport> mWindow; // Parent only
    RefPtr<TextEventDispatcher> mDispatcher;
    java::GeckoEditable::GlobalRef mEditable;
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
            using nsAppShell::LambdaEvent<Functor>::LambdaEvent;

            nsAppShell::Event::Type ActivityType() const override
            {
                return nsAppShell::Event::Type::kUIActivity;
            }
        };
        nsAppShell::PostEvent(mozilla::MakeUnique<IMEEvent>(
                mozilla::Move(aCall)));
    }

    GeckoEditableSupport(nsWindow::NativePtr<GeckoEditableSupport>* aPtr,
                         nsWindow* aWindow,
                         java::GeckoEditable::Param aEditable)
        : mWindow(aPtr, aWindow)
        , mEditable(aEditable)
        , mIMERanges(new TextRangeArray())
        , mIMEMaskEventsCount(1) // Mask IME events since there's no focus yet
        , mIMEUpdatingContext(false)
        , mIMESelectionChanged(false)
        , mIMETextChangedDuringFlush(false)
        , mIMEMonitorCursor(false)
    {}

    NS_DECL_ISUPPORTS

    // TextEventDispatcherListener methods
    NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                         const IMENotification& aNotification) override;

    NS_IMETHOD_(void) OnRemovedFrom(
            TextEventDispatcher* aTextEventDispatcher) override;

    NS_IMETHOD_(void) WillDispatchKeyboardEvent(
            TextEventDispatcher* aTextEventDispatcher,
            WidgetKeyboardEvent& aKeyboardEvent,
            uint32_t aIndexOfKeypress,
            void* aData) override;

    nsIMEUpdatePreference GetIMEUpdatePreference();

    void SetInputContext(const InputContext& aContext,
                         const InputContextAction& aAction);

    InputContext GetInputContext();

    // GeckoEditable methods
    using EditableBase::AttachNative;
    using EditableBase::DisposeNative;

    void OnDetach() {
        mEditable->OnViewChange(nullptr);
    }

    void OnViewChange(java::GeckoView::Param aView) {
        mEditable->OnViewChange(aView);
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
    void OnImeUpdateComposition(int32_t aStart, int32_t aEnd);

    // Set cursor mode whether IME requests
    void OnImeRequestCursorUpdates(int aRequestMode);
};

} // namespace widget
} // namespace mozill

#endif // mozilla_widget_GeckoEditableSupport_h
