/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "AutoRestore.h"

namespace host {
using mozilla::AutoRestore;
template <class T>
using Maybe = std::optional<T>;
template <class T>
using DebugOnly = T;
template <class T>
using nsCOMPtr = T*;
template <class T>
class RefPtr {
 public:
  explicit RefPtr(T*) {}
  ~RefPtr() {}
};
template <class T>
struct nsTArray : std::vector<T> {
  void Clear() { this->clear(); }
  void AppendElement(T aValue) { this->push_back(aValue); }
};

void Check(bool aValue, const char* aMessage) {
  if (!aValue) throw std::runtime_error(aMessage);
}
#define MOZ_ASSERT(value, ...) Check(value, #value)
#define MOZ_DIAGNOSTIC_ASSERT(value) Check(value, #value)
#define NS_WARN_IF(value) (value)
#define NS_WARNING_ASSERTION(value, message) ((void)(value))

enum class NativeKeyBindingsType {
  SingleLineEditor,
  MultiLineEditor,
  RichTextEditor
};
enum class Command { Delete = 7 };
using CommandInt = int;
enum EventMessage { eKeyDown, eKeyUp, eKeyPress };
enum nsEventStatus { nsEventStatus_eIgnore, nsEventStatus_eConsumeNoDefault };
struct WritingMode {};
struct NSEvent {};
struct nsAString {
  using size_type = size_t;
};
constexpr int KEY_NAME_INDEX_USE_STRING = 0;
struct String : std::u16string {
  using std::u16string::u16string;
  size_t Length() const { return size(); }
  void Assign(const std::u16string& aValue) { assign(aValue); }
};
bool XRE_IsParentProcess() { return true; }
Maybe<Command> SelectorCommand(Maybe<Command> aCommand) { return aCommand; }

class MacWebAppInput;
class TextEventDispatcher;
class WidgetKeyboardEvent;
class MacWebAppWidget {
 public:
  MacWebAppInput* mInput = nullptr;
  TextEventDispatcher* mDispatcher = nullptr;
  bool mDestroyed = false;
  bool Destroyed() const { return mDestroyed; }
  TextEventDispatcher* GetTextEventDispatcher() { return mDispatcher; }
  bool GetEditCommands(NativeKeyBindingsType, const WidgetKeyboardEvent&,
                       nsTArray<CommandInt>&);
};
using nsIWidget = MacWebAppWidget;

class WidgetKeyboardEvent {
 public:
  WidgetKeyboardEvent(bool, EventMessage aMessage, nsIWidget* aWidget)
      : mMessage(aMessage), mWidget(aWidget) {}
  EventMessage mMessage;
  nsIWidget* mWidget;
  struct {
    bool mIsSynthesizedForTests = false;
  } mFlags;
  int mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
  String mKeyValue;
  nsTArray<CommandInt> mEditCommandsForSingleLineEditor;
  nsTArray<CommandInt> mEditCommandsForMultiLineEditor;
  nsTArray<CommandInt> mEditCommandsForRichTextEditor;
  bool mEditCommandsForSingleLineEditorInitialized = false;
  bool mEditCommandsForMultiLineEditorInitialized = false;
  bool mEditCommandsForRichTextEditorInitialized = false;
  bool IsTrusted() const { return true; }
  bool ShouldCauseKeypressEvents() const { return true; }
  bool& IsEditCommandsInitializedRef(NativeKeyBindingsType aType) {
    switch (aType) {
      case NativeKeyBindingsType::SingleLineEditor:
        return mEditCommandsForSingleLineEditorInitialized;
      case NativeKeyBindingsType::MultiLineEditor:
        return mEditCommandsForMultiLineEditorInitialized;
      default:
        return mEditCommandsForRichTextEditorInitialized;
    }
  }
  nsTArray<CommandInt>& EditCommandsRef(NativeKeyBindingsType aType) {
    switch (aType) {
      case NativeKeyBindingsType::SingleLineEditor:
        return mEditCommandsForSingleLineEditor;
      case NativeKeyBindingsType::MultiLineEditor:
        return mEditCommandsForMultiLineEditor;
      default:
        return mEditCommandsForRichTextEditor;
    }
  }
  // @@PREVENT_BINDINGS@@
  // @@ALL_INITIALIZED@@
  void AssignKeyEventData(const WidgetKeyboardEvent& aEvent, bool) {
    mKeyValue = aEvent.mKeyValue;
    // @@ASSIGN_INITIALIZED_FLAGS@@
  }
  void InitAllEditCommands(const Maybe<WritingMode>&);
  bool InitEditCommandsFor(NativeKeyBindingsType, const Maybe<WritingMode>&);
};

class NativeKeyBindings {
 public:
  static NativeKeyBindings* GetInstance(NativeKeyBindingsType aType) {
    static std::array<NativeKeyBindings, 3> instances;
    auto& instance = instances[static_cast<size_t>(aType)];
    instance.mType = aType;
    return &instance;
  }
  void GetEditCommands(const WidgetKeyboardEvent&, const Maybe<WritingMode>&,
                       nsTArray<CommandInt>& aCommands) {
    aCommands.AppendElement(100 + static_cast<int>(mType));
  }
  static void GetEditCommandsForTests(NativeKeyBindingsType aType,
                                      const WidgetKeyboardEvent& aEvent,
                                      const Maybe<WritingMode>& aWritingMode,
                                      nsTArray<CommandInt>& aCommands) {
    GetInstance(aType)->GetEditCommands(aEvent, aWritingMode, aCommands);
  }

 private:
  NativeKeyBindingsType mType = NativeKeyBindingsType::SingleLineEditor;
};

namespace widget {
using ::host::NativeKeyBindings;
}

class TextEventDispatcher {
 public:
  explicit TextEventDispatcher(MacWebAppWidget* aWidget) : mWidget(aWidget) {}
  MacWebAppWidget* mWidget;
  bool mHasFocus = true;
  Maybe<WritingMode> mWritingMode;
  std::function<void()> mAfterInitialize;
  std::vector<std::array<std::vector<CommandInt>, 3>> mObserved;
  const Maybe<WritingMode>& MaybeWritingModeRefAtSelection() {
    return mWritingMode;
  }
  void InitEvent(WidgetKeyboardEvent&) {}
  bool MaybeDispatchKeypressEvents(const WidgetKeyboardEvent&, nsEventStatus&,
                                   void* = nullptr, bool = false);
  bool DispatchKeyboardEventInternal(EventMessage aMessage,
                                     const WidgetKeyboardEvent& aKeyboardEvent,
                                     nsEventStatus& aStatus, void*, size_t,
                                     bool) {
    // @@DISPATCH_COPY@@
    // @@DISPATCH_INITIALIZE@@
    Check(keyEvent.AreAllEditCommandsInitialized(), "uninitialized commands");
    mObserved.push_back({keyEvent.mEditCommandsForSingleLineEditor,
                         keyEvent.mEditCommandsForMultiLineEditor,
                         keyEvent.mEditCommandsForRichTextEditor});
    auto callback = std::exchange(mAfterInitialize, {});
    if (callback) callback();
    aStatus = nsEventStatus_eIgnore;
    return true;
  }
};

class TISInputSourceWrapper {
 public:
  static TISInputSourceWrapper& CurrentInputSource() {
    static TISInputSourceWrapper wrapper;
    return wrapper;
  }
  void InitKeyEvent(NSEvent*, WidgetKeyboardEvent& aEvent, bool,
                    const std::u16string* aText = nullptr) {
    aEvent.mKeyValue.Assign(aText ? *aText : u"x");
  }
};

class MacWebAppInput {
 public:
  explicit MacWebAppInput(MacWebAppWidget* aWidget) : mWidget(aWidget) {}
  MacWebAppWidget* mWidget;
  NSEvent mKey;
  NSEvent* mLastKeyEvent = &mKey;
  // @@COMMAND_FIELD@@
  // @@SUPPRESSION_FIELD@@
  int mButtons = 0;
  bool mKeyDownConsumed = false;
  std::function<void()> mCompositionCallback;
  bool IsAlive() const;
  bool GetEditCommands(NativeKeyBindingsType, const WidgetKeyboardEvent&,
                       nsTArray<CommandInt>&);
  void Inbound(const std::function<void()>& aBody) {
    if (!IsAlive()) return;
    // @@HANDLE_SCOPE@@
    aBody();
  }
  void TextCommand(Maybe<Command> selector) {
    auto* dispatcher = mWidget->GetTextEventDispatcher();
    nsEventStatus status = nsEventStatus_eIgnore;
    // @@TEXT_COMMAND@@
  }
  void InsertText(const std::u16string& value) {
    auto* dispatcher = mWidget->GetTextEventDispatcher();
    nsEventStatus status = nsEventStatus_eIgnore;
    // @@INSERT_TEXT@@
  }
  void RawKey() {
    WidgetKeyboardEvent event(true, eKeyDown, mWidget);
    TISInputSourceWrapper::CurrentInputSource().InitKeyEvent(&mKey, event,
                                                             false);
    nsEventStatus status = nsEventStatus_eIgnore;
    mWidget->GetTextEventDispatcher()->MaybeDispatchKeypressEvents(event,
                                                                   status);
  }
  void FinishComposition(bool) {
    if (mCompositionCallback) mCompositionCallback();
  }
  void Disconnect() {
    // @@DISCONNECT_RESET@@
    FinishComposition(true);
  }
  void Destroy() {
    // @@DESTROY_RESET@@
  }
};

// @@WIDGET_FORWARD@@
// @@IS_ALIVE@@
// @@GET_EDIT_COMMANDS@@
// @@INIT_ALL_COMMANDS@@
// @@INIT_COMMANDS_FOR@@
// @@MAYBE_KEYPRESS@@

struct Fixture {
  MacWebAppWidget widget;
  MacWebAppInput input{&widget};
  TextEventDispatcher dispatcher{&widget};
  Fixture() {
    widget.mInput = &input;
    widget.mDispatcher = &dispatcher;
  }
  bool Default() const {
    return !input.mCommand && !input.mSuppressNativeKeyBindings;
  }
  void Commands(const std::array<std::vector<CommandInt>, 3>& aCommands,
                bool aEmpty, bool aSelected = false) {
    for (size_t i = 0; i < 3; ++i) {
      Check(
          aCommands[i] ==
              (aEmpty ? std::vector<CommandInt>{}
                      : std::vector<CommandInt>{aSelected ? 7 : 100 + int(i)}),
          "wrong edit commands");
    }
  }
  void Current(bool aEmpty, bool aSelected = false) {
    WidgetKeyboardEvent event(true, eKeyPress, &widget);
    for (int i = 0; i < 3; ++i) {
      nsTArray<CommandInt> result;
      if (aEmpty) result.push_back(999);
      Check(input.GetEditCommands(static_cast<NativeKeyBindingsType>(i), event,
                                  result),
            "binding lookup failed");
      Check(
          result == (aEmpty ? std::vector<CommandInt>{}
                            : std::vector<CommandInt>{aSelected ? 7 : 100 + i}),
          "wrong active policy");
    }
  }
};
}  // namespace host

int main() {
  using namespace host;
  int failures = 0;
  auto test = [&](const char* aName, const std::function<void()>& aBody) {
    try {
      aBody();
      std::cout << "PASS " << aName << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cout << "FAIL " << aName << ": " << error.what() << '\n';
    }
  };
  test("text-command-initializes-once", [] {
    Fixture f;
    f.input.Inbound([&] { f.input.TextCommand(Command::Delete); });
    f.Commands(f.dispatcher.mObserved.back(), false, true);
    Check(f.Default(), "command policy leaked");
  });
  test("insert-text-empty-bindings-all-editors", [] {
    Fixture f;
    f.input.Inbound([&] { f.input.InsertText(u"a"); });
    f.Commands(f.dispatcher.mObserved.back(), true);
    Check(f.Default(), "suppression leaked");
  });
  test("multiple-utf16-keypresses-share-policy", [] {
    Fixture f;
    f.input.Inbound([&] { f.input.InsertText(u"A\U0001F600B"); });
    Check(f.dispatcher.mObserved.size() == 4, "keypress count changed");
    for (const auto& commands : f.dispatcher.mObserved)
      f.Commands(commands, true);
    Check(f.Default(), "suppression leaked");
  });
  test("normal-key-native-bindings", [] {
    Fixture f;
    f.input.Inbound([&] { f.input.RawKey(); });
    f.Commands(f.dispatcher.mObserved.back(), false);
  });
  test("unknown-command-native-fallback", [] {
    Fixture f;
    f.input.Inbound([&] { f.input.TextCommand({}); });
    f.Commands(f.dispatcher.mObserved.back(), false);
  });
  test("insert-reentry-raw-command-outer-restoration", [] {
    Fixture f;
    f.dispatcher.mAfterInitialize = [&] {
      f.Current(true);
      f.input.Inbound([&] {
        Check(f.Default(), "nested raw inherited policy");
        f.dispatcher.mAfterInitialize = [&] {
          f.input.Inbound([&] { f.input.TextCommand(Command::Delete); });
          Check(f.Default(), "nested command leaked");
        };
        f.input.RawKey();
      });
      f.Current(true);
    };
    f.input.Inbound([&] { f.input.InsertText(u"ab"); });
    f.Commands(f.dispatcher.mObserved[0], true);
    f.Commands(f.dispatcher.mObserved[1], false);
    f.Commands(f.dispatcher.mObserved[2], false, true);
    f.Commands(f.dispatcher.mObserved[3], true);
    Check(f.Default(), "outer suppression leaked");
  });
  test("command-reentry-insert-outer-restoration", [] {
    Fixture f;
    f.dispatcher.mAfterInitialize = [&] {
      f.Current(false, true);
      f.input.Inbound([&] { f.input.InsertText(u"a"); });
      f.Current(false, true);
    };
    f.input.Inbound([&] { f.input.TextCommand(Command::Delete); });
    f.Commands(f.dispatcher.mObserved[1], true);
    Check(f.Default(), "outer command leaked");
  });
  test("early-return-restores-outer-policy", [] {
    Fixture f;
    f.input.mCommand = Command::Delete;
    f.input.mSuppressNativeKeyBindings = true;
    f.input.Inbound([&] { Check(f.Default(), "entry not default"); });
    Check(f.input.mCommand == Command::Delete &&
              f.input.mSuppressNativeKeyBindings,
          "early return lost outer policy");
  });
  test("exception-restores-all-scopes", [] {
    Fixture f;
    bool called = false;
    f.dispatcher.mAfterInitialize = [&] {
      called = true;
      throw std::runtime_error("callback");
    };
    try {
      f.input.Inbound([&] { f.input.InsertText(u"a"); });
    } catch (const std::runtime_error&) {
    }
    Check(called, "callback not reached");
    Check(f.Default(), "exception leaked suppression");
  });
  test("suppression-clears-seeded-arrays", [] {
    Fixture f;
    f.input.mSuppressNativeKeyBindings = true;
    f.Current(true);
  });
  test("disconnect-keeps-multiple-keypress-suppression", [] {
    Fixture f;
    f.dispatcher.mAfterInitialize = [&] { f.input.Disconnect(); };
    f.input.Inbound([&] { f.input.InsertText(u"ab"); });
    Check(f.dispatcher.mObserved.size() == 2, "keypress count changed");
    for (const auto& commands : f.dispatcher.mObserved)
      f.Commands(commands, true);
    Check(f.Default(), "disconnect scope leaked");
  });
  test("disconnect-synthetic-reentry-restores-outer-policy", [] {
    Fixture f;
    f.dispatcher.mAfterInitialize = [&] {
      f.input.mCompositionCallback = [&] {
        f.Current(true);
        f.input.Inbound([&] {
          Check(f.Default(), "nested raw inherited policy");
          f.input.RawKey();
        });
        f.Current(true);
      };
      f.input.Disconnect();
      f.Current(true);
    };
    f.input.Inbound([&] { f.input.InsertText(u"ab"); });
    f.Commands(f.dispatcher.mObserved[0], true);
    f.Commands(f.dispatcher.mObserved[1], false);
    f.Commands(f.dispatcher.mObserved[2], true);
    Check(f.Default(), "disconnect scope leaked");
  });
  test("destroyed-widget-fails-before-suppression", [] {
    Fixture f;
    f.input.mSuppressNativeKeyBindings = true;
    f.widget.mDestroyed = true;
    WidgetKeyboardEvent event(true, eKeyPress, &f.widget);
    nsTArray<CommandInt> commands;
    commands.push_back(999);
    Check(!f.input.GetEditCommands(NativeKeyBindingsType::SingleLineEditor,
                                   event, commands),
          "dead widget accepted");
    Check(commands == std::vector<CommandInt>{999}, "failure changed array");
  });
  test("on-destroy-reentry-fails-and-restores", [] {
    Fixture f;
    f.dispatcher.mAfterInitialize = [&] {
      f.input.Destroy();
      bool called = false;
      f.input.Inbound([&] { called = true; });
      Check(!called, "dead widget entered handler");
      WidgetKeyboardEvent event(true, eKeyPress, &f.widget);
      nsTArray<CommandInt> commands;
      commands.push_back(999);
      Check(!f.input.GetEditCommands(NativeKeyBindingsType::SingleLineEditor,
                                     event, commands),
            "destroy accepted");
      Check(commands == std::vector<CommandInt>{999}, "destroy changed array");
    };
    f.input.Inbound([&] { f.input.InsertText(u"a"); });
    Check(f.Default(), "destroy scope leaked");
  });
  std::cout << "RESULT tests=14 failures=" << failures << '\n';
  return failures ? 1 : 0;
}
