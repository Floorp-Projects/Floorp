/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
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

namespace dom {
class BrowserChild;
}

namespace widget {

class GeckoEditableSupport final
    : public TextEventDispatcherListener,
      public java::GeckoEditableChild::Natives<GeckoEditableSupport> {
  /*
      Rules for managing IME between Gecko and Java:

      * Gecko controls the text content, and Java shadows the Gecko text
         through text updates
      * Gecko and Java maintain separate selections, and synchronize when
         needed through selection updates and set-selection events
      * Java controls the composition, and Gecko shadows the Java
         composition through update composition events
  */

  using EditableBase = java::GeckoEditableChild::Natives<GeckoEditableSupport>;
  using EditableClient = java::SessionTextInput::EditableClient;
  using EditableListener = java::SessionTextInput::EditableListener;

  struct IMETextChange final {
    int32_t mStart, mOldEnd, mNewEnd;

    IMETextChange() : mStart(-1), mOldEnd(-1), mNewEnd(-1) {}

    explicit IMETextChange(const IMENotification& aIMENotification)
        : mStart(aIMENotification.mTextChangeData.mStartOffset),
          mOldEnd(aIMENotification.mTextChangeData.mRemovedEndOffset),
          mNewEnd(aIMENotification.mTextChangeData.mAddedEndOffset) {
      MOZ_ASSERT(aIMENotification.mMessage == NOTIFY_IME_OF_TEXT_CHANGE,
                 "IMETextChange initialized with wrong notification");
      MOZ_ASSERT(aIMENotification.mTextChangeData.IsValid(),
                 "The text change notification isn't initialized");
      MOZ_ASSERT(aIMENotification.mTextChangeData.IsInInt32Range(),
                 "The text change notification is out of range");
    }

    bool IsEmpty() const { return mStart < 0; }
  };

  enum FlushChangesFlag {
    // Not retrying.
    FLUSH_FLAG_NONE,
    // Retrying due to IME text changes during flush.
    FLUSH_FLAG_RETRY,
    // Retrying due to IME sync exceptions during flush.
    FLUSH_FLAG_RECOVER
  };

  enum RemoveCompositionFlag { CANCEL_IME_COMPOSITION, COMMIT_IME_COMPOSITION };

  const bool mIsRemote;
  nsWindow::WindowPtr<GeckoEditableSupport> mWindow;  // Parent only
  RefPtr<TextEventDispatcher> mDispatcher;
  java::GeckoEditableChild::GlobalRef mEditable;
  bool mEditableAttached;
  InputContext mInputContext;
  AutoTArray<UniquePtr<mozilla::WidgetEvent>, 4> mIMEKeyEvents;
  AutoTArray<IMETextChange, 4> mIMETextChanges;
  RefPtr<TextRangeArray> mIMERanges;
  int32_t mIMEMaskEventsCount;         // Mask events when > 0.
  int32_t mIMEFocusCount;              // We are focused when > 0.
  bool mIMEDelaySynchronizeReply;      // We reply asynchronously when true.
  int32_t mIMEActiveSynchronizeCount;  // The number of replies being delayed.
  int32_t mIMEActiveCompositionCount;  // The number of compositions expected.
  bool mIMESelectionChanged;
  bool mIMETextChangedDuringFlush;
  bool mIMEMonitorCursor;

  nsIWidget* GetWidget() const {
    return mDispatcher ? mDispatcher->GetWidget() : mWindow;
  }

  nsresult BeginInputTransaction(TextEventDispatcher* aDispatcher) {
    if (mIsRemote) {
      return aDispatcher->BeginInputTransaction(this);
    } else {
      return aDispatcher->BeginNativeInputTransaction();
    }
  }

  virtual ~GeckoEditableSupport() {}

  RefPtr<TextComposition> GetComposition() const;
  bool RemoveComposition(RemoveCompositionFlag aFlag = COMMIT_IME_COMPOSITION);
  void SendIMEDummyKeyEvent(nsIWidget* aWidget, EventMessage msg);
  void AddIMETextChange(const IMETextChange& aChange);
  void PostFlushIMEChanges();
  void FlushIMEChanges(FlushChangesFlag aFlags = FLUSH_FLAG_NONE);
  void FlushIMEText(FlushChangesFlag aFlags = FLUSH_FLAG_NONE);
  void AsyncNotifyIME(int32_t aNotification);
  void UpdateCompositionRects();
  bool DoReplaceText(int32_t aStart, int32_t aEnd, jni::String::Param aText);
  bool DoUpdateComposition(int32_t aStart, int32_t aEnd, int32_t aFlags);
  void OnNotifyIMEOfCompositionEventHandled();
  void NotifyIMEContext(const InputContext& aContext,
                        const InputContextAction& aAction);

 public:
  template <typename Functor>
  static void OnNativeCall(Functor&& aCall) {
    struct IMEEvent : nsAppShell::LambdaEvent<Functor> {
      explicit IMEEvent(Functor&& l)
          : nsAppShell::LambdaEvent<Functor>(std::move(l)) {}

      bool IsUIEvent() const override {
        using GES = GeckoEditableSupport;
        if (this->lambda.IsTarget(&GES::OnKeyEvent) ||
            this->lambda.IsTarget(&GES::OnImeReplaceText) ||
            this->lambda.IsTarget(&GES::OnImeUpdateComposition)) {
          return true;
        }
        return false;
      }

      void Run() override {
        if (!this->lambda.GetNativeObject()) {
          // Ignore stale calls after disposal.
          jni::GetGeckoThreadEnv()->ExceptionClear();
          return;
        }
        nsAppShell::LambdaEvent<Functor>::Run();
      }
    };
    nsAppShell::PostEvent(mozilla::MakeUnique<IMEEvent>(std::move(aCall)));
  }

  static void SetOnBrowserChild(dom::BrowserChild* aBrowserChild);

  // Constructor for main process GeckoEditableChild.
  GeckoEditableSupport(nsWindow::NativePtr<GeckoEditableSupport>* aPtr,
                       nsWindow* aWindow,
                       java::GeckoEditableChild::Param aEditableChild)
      : mIsRemote(!aWindow),
        mWindow(aPtr, aWindow),
        mEditable(aEditableChild),
        mEditableAttached(!mIsRemote),
        mIMERanges(new TextRangeArray()),
        mIMEMaskEventsCount(1)  // Mask IME events since there's no focus yet
        ,
        mIMEFocusCount(0),
        mIMEDelaySynchronizeReply(false),
        mIMEActiveSynchronizeCount(0),
        mIMESelectionChanged(false),
        mIMETextChangedDuringFlush(false),
        mIMEMonitorCursor(false) {}

  // Constructor for content process GeckoEditableChild.
  explicit GeckoEditableSupport(java::GeckoEditableChild::Param aEditableChild)
      : GeckoEditableSupport(nullptr, nullptr, aEditableChild) {}

  NS_DECL_ISUPPORTS

  // TextEventDispatcherListener methods
  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) override;

  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;

  NS_IMETHOD_(void)
  OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) override;

  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher* aTextEventDispatcher,
                            WidgetKeyboardEvent& aKeyboardEvent,
                            uint32_t aIndexOfKeypress, void* aData) override;

  void SetInputContext(const InputContext& aContext,
                       const InputContextAction& aAction);

  InputContext GetInputContext();

  // GeckoEditableChild methods
  using EditableBase::AttachNative;
  using EditableBase::DisposeNative;

  const java::GeckoEditableChild::Ref& GetJavaEditable() { return mEditable; }

  void OnDetach(already_AddRefed<Runnable> aDisposer) {
    RefPtr<GeckoEditableSupport> self(this);
    nsAppShell::PostEvent([this, self, disposer = RefPtr<Runnable>(aDisposer)] {
      mEditableAttached = false;
      disposer->Run();
    });
  }

  // Transfer to a new parent.
  void TransferParent(jni::Object::Param aEditableParent);

  // Handle an Android KeyEvent.
  void OnKeyEvent(int32_t aAction, int32_t aKeyCode, int32_t aScanCode,
                  int32_t aMetaState, int32_t aKeyPressMetaState, int64_t aTime,
                  int32_t aDomPrintableKeyValue, int32_t aRepeatCount,
                  int32_t aFlags, bool aIsSynthesizedImeKey,
                  jni::Object::Param originalEvent);

  // Synchronize Gecko thread with the InputConnection thread.
  void OnImeSynchronize();

  // Replace a range of text with new text.
  void OnImeReplaceText(int32_t aStart, int32_t aEnd, jni::String::Param aText);

  // Add styling for a range within the active composition.
  void OnImeAddCompositionRange(int32_t aStart, int32_t aEnd,
                                int32_t aRangeType, int32_t aRangeStyle,
                                int32_t aRangeLineStyle, bool aRangeBoldLine,
                                int32_t aRangeForeColor,
                                int32_t aRangeBackColor,
                                int32_t aRangeLineColor);

  // Update styling for the active composition using previous-added ranges.
  void OnImeUpdateComposition(int32_t aStart, int32_t aEnd, int32_t aFlags);

  // Set cursor mode whether IME requests
  void OnImeRequestCursorUpdates(int aRequestMode);
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_GeckoEditableSupport_h
