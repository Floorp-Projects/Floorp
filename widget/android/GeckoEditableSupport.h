/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_GeckoEditableSupport_h
#define mozilla_widget_GeckoEditableSupport_h

#include "nsAppShell.h"
#include "nsIWidget.h"
#include "nsTArray.h"

#include "mozilla/java/GeckoEditableChildNatives.h"
#include "mozilla/java/SessionTextInputWrappers.h"
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
  jni::NativeWeakPtr<GeckoViewSupport> mWindow;  // Parent only
  RefPtr<TextEventDispatcher> mDispatcher;
  java::GeckoEditableChild::GlobalRef mEditable;
  bool mEditableAttached;
  InputContext mInputContext;
  AutoTArray<UniquePtr<mozilla::WidgetEvent>, 4> mIMEKeyEvents;
  IMENotification::TextChangeData mIMEPendingTextChange;
  RefPtr<TextRangeArray> mIMERanges;
  RefPtr<Runnable> mDisposeRunnable;
  int32_t mIMEMaskEventsCount;         // Mask events when > 0.
  int32_t mIMEFocusCount;              // We are focused when > 0.
  bool mIMEDelaySynchronizeReply;      // We reply asynchronously when true.
  int32_t mIMEActiveSynchronizeCount;  // The number of replies being delayed.
  int32_t mIMEActiveCompositionCount;  // The number of compositions expected.
  uint32_t mDisposeBlockCount;
  bool mIMESelectionChanged;
  bool mIMETextChangedDuringFlush;
  bool mIMEMonitorCursor;

  nsIWidget* GetWidget() const;
  nsWindow* GetNsWindow() const;

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
  void AddIMETextChange(const IMENotification::TextChangeDataBase& aChange);
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
        if (NS_WARN_IF(!this->lambda.GetNativeObject())) {
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
  GeckoEditableSupport(jni::NativeWeakPtr<GeckoViewSupport> aWindow,
                       java::GeckoEditableChild::Param aEditableChild)
      : mIsRemote(!aWindow.IsAttached()),
        mWindow(aWindow),
        mEditable(aEditableChild),
        mEditableAttached(!mIsRemote),
        mIMERanges(new TextRangeArray()),
        mIMEMaskEventsCount(1),  // Mask IME events since there's no focus yet
        mIMEFocusCount(0),
        mIMEDelaySynchronizeReply(false),
        mIMEActiveSynchronizeCount(0),
        mDisposeBlockCount(0),
        mIMESelectionChanged(false),
        mIMETextChangedDuringFlush(false),
        mIMEMonitorCursor(false) {}

  // Constructor for content process GeckoEditableChild.
  explicit GeckoEditableSupport(java::GeckoEditableChild::Param aEditableChild)
      : GeckoEditableSupport(nullptr, aEditableChild) {}

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

  bool HasIMEFocus() const { return mIMEFocusCount != 0; }

  void AddBlocker() { mDisposeBlockCount++; }

  void ReleaseBlocker() {
    mDisposeBlockCount--;

    if (!mDisposeBlockCount && mDisposeRunnable) {
      if (HasIMEFocus()) {
        // If we have IME focus, GeckoEditableChild is already attached again.
        // So disposer is unnecessary.
        mDisposeRunnable = nullptr;
        return;
      }

      RefPtr<GeckoEditableSupport> self(this);
      RefPtr<Runnable> disposer = std::move(mDisposeRunnable);

      nsAppShell::PostEvent(
          [self = std::move(self), disposer = std::move(disposer)] {
            self->mEditableAttached = false;
            disposer->Run();
          });
    }
  }

  bool IsGeckoEditableUsed() const { return mDisposeBlockCount != 0; }

  // GeckoEditableChild methods
  using EditableBase::AttachNative;
  using EditableBase::DisposeNative;

  const java::GeckoEditableChild::Ref& GetJavaEditable() { return mEditable; }

  void OnWeakNonIntrusiveDetach(already_AddRefed<Runnable> aDisposer) {
    RefPtr<GeckoEditableSupport> self(this);
    nsAppShell::PostEvent(
        [self = std::move(self), disposer = RefPtr<Runnable>(aDisposer)] {
          if (self->IsGeckoEditableUsed()) {
            // Current calling stack uses GeckoEditableChild, so we should
            // not dispose it now.
            self->mDisposeRunnable = disposer;
            return;
          }
          self->mEditableAttached = false;
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

  // Commit current composition to sync Gecko text state with Java.
  void OnImeRequestCommit();
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_GeckoEditableSupport_h
