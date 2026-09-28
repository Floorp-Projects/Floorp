/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_MacWebAppInput_h
#define mozilla_widget_MacWebAppInput_h

#include <functional>
#include "mozilla/Maybe.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "nsString.h"
#include "nsTArray.h"

@class NSDictionary;
@class NSEvent;
class nsIWidget;

namespace mozilla::widget {

/** Main-thread adapter for authenticated events from one native Shim window. */
class MacWebAppInput final : public TextEventDispatcherListener {
 public:
  NS_DECL_ISUPPORTS
  MacWebAppInput(nsIWidget* aWidget,
                 std::function<void(NSDictionary*)> aSendEditorState);
  void Handle(NSDictionary* aPayload);
  void UpdateEditorState();
  void OnShimDisconnected();
  void OnDestroy();
  bool GetEditCommands(NativeKeyBindingsType aType,
                       const WidgetKeyboardEvent& aEvent,
                       nsTArray<CommandInt>& aCommands);

  NS_IMETHOD NotifyIME(TextEventDispatcher* aDispatcher,
                       const IMENotification& aNotification) override;
  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;
  NS_IMETHOD_(void) OnRemovedFrom(TextEventDispatcher* aDispatcher) override;
  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher* aDispatcher,
                            WidgetKeyboardEvent& aEvent,
                            uint32_t aIndexOfKeypress, void* aData) override;

 private:
  ~MacWebAppInput();
  bool ApplyReplacement(NSDictionary* aPayload);
  void HandleText(NSDictionary* aPayload);
  void HandleKey(NSDictionary* aPayload);
  void FinishComposition(bool aCancel);
  void SendEditorState(bool aDiscardMarkedText);
  bool IsAlive() const;

  // The widget retains this listener; OnDestroy clears the raw pointer before
  // the widget tears down its TextEventDispatcher and callback owner.
  nsIWidget* mWidget;
  std::function<void(NSDictionary*)> mSendEditorState;
  NSEvent* mLastKeyEvent =
      nullptr;  // Retained; this file uses Gecko's MRC input helpers.
  Maybe<Command> mCommand;
  nsString mComposition;
  uint32_t mCompositionStart = 0;
  uint32_t mRevision = 0;
  uint32_t mFocusRevision = 0;
  int16_t mButtons = 0;
  bool mUpdatingEditorState = false;
  bool mKeyDownConsumed = false;
  bool mComposing = false;
  bool mIMEFocused = false;
};

}  // namespace mozilla::widget
#endif
