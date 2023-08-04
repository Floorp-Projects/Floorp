/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Maybe.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/TextEvents.h"
#include "mozilla/WritingModes.h"

#include "NativeKeyBindings.h"
#include "nsString.h"
#include "nsGtkKeyUtils.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkkeysyms-compat.h>
#include <gdk/gdk.h>

namespace mozilla {
namespace widget {

static nsTArray<CommandInt>* gCurrentCommands = nullptr;
static bool gHandled = false;

inline void AddCommand(Command aCommand) {
  MOZ_ASSERT(gCurrentCommands);
  gCurrentCommands->AppendElement(static_cast<CommandInt>(aCommand));
}

// Common GtkEntry and GtkTextView signals
static void copy_clipboard_cb(GtkWidget* w, gpointer user_data) {
  AddCommand(Command::Copy);
  g_signal_stop_emission_by_name(w, "copy_clipboard");
  gHandled = true;
}

static void cut_clipboard_cb(GtkWidget* w, gpointer user_data) {
  AddCommand(Command::Cut);
  g_signal_stop_emission_by_name(w, "cut_clipboard");
  gHandled = true;
}

// GTK distinguishes between display lines (wrapped, as they appear on the
// screen) and paragraphs, which are runs of text terminated by a newline.
// We don't have this distinction, so we always use editor's notion of
// lines, which are newline-terminated.

static const Command sDeleteCommands[][2] = {
    // backward, forward
    // CHARS
    {Command::DeleteCharBackward, Command::DeleteCharForward},
    // WORD_ENDS
    {Command::DeleteWordBackward, Command::DeleteWordForward},
    // WORDS
    {Command::DeleteWordBackward, Command::DeleteWordForward},
    // LINES
    {Command::DeleteToBeginningOfLine, Command::DeleteToEndOfLine},
    // LINE_ENDS
    {Command::DeleteToBeginningOfLine, Command::DeleteToEndOfLine},
    // PARAGRAPH_ENDS
    {Command::DeleteToBeginningOfLine, Command::DeleteToEndOfLine},
    // PARAGRAPHS
    {Command::DeleteToBeginningOfLine, Command::DeleteToEndOfLine},
    // This deletes from the end of the previous word to the beginning of the
    // next word, but only if the caret is not in a word.
    // XXX need to implement in editor
    {Command::DoNothing, Command::DoNothing}  // WHITESPACE
};

static void delete_from_cursor_cb(GtkWidget* w, GtkDeleteType del_type,
                                  gint count, gpointer user_data) {
  g_signal_stop_emission_by_name(w, "delete_from_cursor");
  if (count == 0) {
    // Nothing to do.
    return;
  }

  bool forward = count > 0;

  // Ignore GTK's Ctrl-K keybinding introduced in GTK 3.14 and removed in
  // 3.18 if the user has custom bindings set. See bug 1176929.
  if (del_type == GTK_DELETE_PARAGRAPH_ENDS && forward && GTK_IS_ENTRY(w) &&
      !gtk_check_version(3, 14, 1) && gtk_check_version(3, 17, 9)) {
    GtkStyleContext* context = gtk_widget_get_style_context(w);
    GtkStateFlags flags = gtk_widget_get_state_flags(w);

    GPtrArray* array;
    gtk_style_context_get(context, flags, "gtk-key-bindings", &array, nullptr);
    if (!array) return;
    g_ptr_array_unref(array);
  }

  gHandled = true;
  if (uint32_t(del_type) >= ArrayLength(sDeleteCommands)) {
    // unsupported deletion type
    return;
  }

  if (del_type == GTK_DELETE_WORDS) {
    // This works like word_ends, except we first move the caret to the
    // beginning/end of the current word.
    if (forward) {
      AddCommand(Command::WordNext);
      AddCommand(Command::WordPrevious);
    } else {
      AddCommand(Command::WordPrevious);
      AddCommand(Command::WordNext);
    }
  } else if (del_type == GTK_DELETE_DISPLAY_LINES ||
             del_type == GTK_DELETE_PARAGRAPHS) {
    // This works like display_line_ends, except we first move the caret to the
    // beginning/end of the current line.
    if (forward) {
      AddCommand(Command::BeginLine);
    } else {
      AddCommand(Command::EndLine);
    }
  }

  Command command = sDeleteCommands[del_type][forward];
  if (command == Command::DoNothing) {
    return;
  }

  unsigned int absCount = Abs(count);
  for (unsigned int i = 0; i < absCount; ++i) {
    AddCommand(command);
  }
}

static const Command sMoveCommands[][2][2] = {
    // non-extend { backward, forward }, extend { backward, forward }
    // GTK differentiates between logical position, which is prev/next,
    // and visual position, which is always left/right.
    // We should fix this to work the same way for RTL text input.
    {// LOGICAL_POSITIONS
     {Command::CharPrevious, Command::CharNext},
     {Command::SelectCharPrevious, Command::SelectCharNext}},
    {// VISUAL_POSITIONS
     {Command::CharPrevious, Command::CharNext},
     {Command::SelectCharPrevious, Command::SelectCharNext}},
    {// WORDS
     {Command::WordPrevious, Command::WordNext},
     {Command::SelectWordPrevious, Command::SelectWordNext}},
    {// DISPLAY_LINES
     {Command::LinePrevious, Command::LineNext},
     {Command::SelectLinePrevious, Command::SelectLineNext}},
    {// DISPLAY_LINE_ENDS
     {Command::BeginLine, Command::EndLine},
     {Command::SelectBeginLine, Command::SelectEndLine}},
    {// PARAGRAPHS
     {Command::LinePrevious, Command::LineNext},
     {Command::SelectLinePrevious, Command::SelectLineNext}},
    {// PARAGRAPH_ENDS
     {Command::BeginLine, Command::EndLine},
     {Command::SelectBeginLine, Command::SelectEndLine}},
    {// PAGES
     {Command::MovePageUp, Command::MovePageDown},
     {Command::SelectPageUp, Command::SelectPageDown}},
    {// BUFFER_ENDS
     {Command::MoveTop, Command::MoveBottom},
     {Command::SelectTop, Command::SelectBottom}},
    {// HORIZONTAL_PAGES (unsupported)
     {Command::DoNothing, Command::DoNothing},
     {Command::DoNothing, Command::DoNothing}}};

static void move_cursor_cb(GtkWidget* w, GtkMovementStep step, gint count,
                           gboolean extend_selection, gpointer user_data) {
  g_signal_stop_emission_by_name(w, "move_cursor");
  if (count == 0) {
    // Nothing to do.
    return;
  }

  gHandled = true;
  bool forward = count > 0;
  if (uint32_t(step) >= ArrayLength(sMoveCommands)) {
    // unsupported movement type
    return;
  }

  Command command = sMoveCommands[step][extend_selection][forward];
  if (command == Command::DoNothing) {
    return;
  }

  unsigned int absCount = Abs(count);
  for (unsigned int i = 0; i < absCount; ++i) {
    AddCommand(command);
  }
}

static void paste_clipboard_cb(GtkWidget* w, gpointer user_data) {
  AddCommand(Command::Paste);
  g_signal_stop_emission_by_name(w, "paste_clipboard");
  gHandled = true;
}

// GtkTextView-only signals
static void select_all_cb(GtkWidget* aWidget, gboolean aSelect,
                          gpointer aUserData) {
  // We don't support "Unselect All" command.
  // Note that if we'd support it, `Ctrl-Shift-a` will be mapped to it and
  // overrides open `about:addons` shortcut.
  if (aSelect) {
    AddCommand(Command::SelectAll);
  }
  g_signal_stop_emission_by_name(aWidget, "select_all");
  // Although we prevent the default of `GtkTExtView` with
  // `g_signal_stop_emission_by_name`, but `gHandled` is used for asserting
  // if it does not match with the emptiness of the command array.
  // Therefore, we should not set it to `true` if we don't add a command.
  gHandled |= aSelect;
}

NativeKeyBindings* NativeKeyBindings::sInstanceForSingleLineEditor = nullptr;
NativeKeyBindings* NativeKeyBindings::sInstanceForMultiLineEditor = nullptr;

// static
NativeKeyBindings* NativeKeyBindings::GetInstance(NativeKeyBindingsType aType) {
  switch (aType) {
    case NativeKeyBindingsType::SingleLineEditor:
      if (!sInstanceForSingleLineEditor) {
        sInstanceForSingleLineEditor = new NativeKeyBindings();
        sInstanceForSingleLineEditor->Init(aType);
      }
      return sInstanceForSingleLineEditor;

    default:
      // fallback to multiline editor case in release build
      MOZ_FALLTHROUGH_ASSERT("aType is invalid or not yet implemented");
    case NativeKeyBindingsType::MultiLineEditor:
    case NativeKeyBindingsType::RichTextEditor:
      if (!sInstanceForMultiLineEditor) {
        sInstanceForMultiLineEditor = new NativeKeyBindings();
        sInstanceForMultiLineEditor->Init(aType);
      }
      return sInstanceForMultiLineEditor;
  }
}

// static
void NativeKeyBindings::Shutdown() {
  delete sInstanceForSingleLineEditor;
  sInstanceForSingleLineEditor = nullptr;
  delete sInstanceForMultiLineEditor;
  sInstanceForMultiLineEditor = nullptr;
}

void NativeKeyBindings::Init(NativeKeyBindingsType aType) {
  switch (aType) {
    case NativeKeyBindingsType::SingleLineEditor:
      mNativeTarget = gtk_entry_new();
      break;
    default:
      mNativeTarget = gtk_text_view_new();
      g_signal_connect(mNativeTarget, "select_all", G_CALLBACK(select_all_cb),
                       this);
      break;
  }

  g_object_ref_sink(mNativeTarget);

  g_signal_connect(mNativeTarget, "copy_clipboard",
                   G_CALLBACK(copy_clipboard_cb), this);
  g_signal_connect(mNativeTarget, "cut_clipboard", G_CALLBACK(cut_clipboard_cb),
                   this);
  g_signal_connect(mNativeTarget, "delete_from_cursor",
                   G_CALLBACK(delete_from_cursor_cb), this);
  g_signal_connect(mNativeTarget, "move_cursor", G_CALLBACK(move_cursor_cb),
                   this);
  g_signal_connect(mNativeTarget, "paste_clipboard",
                   G_CALLBACK(paste_clipboard_cb), this);
}

NativeKeyBindings::~NativeKeyBindings() {
  gtk_widget_destroy(mNativeTarget);
  g_object_unref(mNativeTarget);
}

void NativeKeyBindings::GetEditCommands(const WidgetKeyboardEvent& aEvent,
                                        const Maybe<WritingMode>& aWritingMode,
                                        nsTArray<CommandInt>& aCommands) {
  MOZ_ASSERT(!aEvent.mFlags.mIsSynthesizedForTests);
  MOZ_ASSERT(aCommands.IsEmpty());

  // It must be a DOM event dispached by chrome script.
  if (!aEvent.mNativeKeyEvent) {
    return;
  }

  guint keyval;
  if (aEvent.mCharCode) {
    keyval = gdk_unicode_to_keyval(aEvent.mCharCode);
  } else if (aWritingMode.isSome() && aEvent.NeedsToRemapNavigationKey() &&
             aWritingMode.ref().IsVertical()) {
    // TODO: Use KeyNameIndex rather than legacy keyCode.
    uint32_t remappedGeckoKeyCode =
        aEvent.GetRemappedKeyCode(aWritingMode.ref());
    switch (remappedGeckoKeyCode) {
      case NS_VK_UP:
        keyval = GDK_Up;
        break;
      case NS_VK_DOWN:
        keyval = GDK_Down;
        break;
      case NS_VK_LEFT:
        keyval = GDK_Left;
        break;
      case NS_VK_RIGHT:
        keyval = GDK_Right;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Add a case for the new remapped key");
        return;
    }
  } else {
    keyval = static_cast<GdkEventKey*>(aEvent.mNativeKeyEvent)->keyval;
  }

  if (GetEditCommandsInternal(aEvent, aCommands, keyval)) {
    return;
  }

  for (uint32_t i = 0; i < aEvent.mAlternativeCharCodes.Length(); ++i) {
    uint32_t ch = aEvent.IsShift()
                      ? aEvent.mAlternativeCharCodes[i].mShiftedCharCode
                      : aEvent.mAlternativeCharCodes[i].mUnshiftedCharCode;
    if (ch && ch != aEvent.mCharCode) {
      keyval = gdk_unicode_to_keyval(ch);
      if (GetEditCommandsInternal(aEvent, aCommands, keyval)) {
        return;
      }
    }
  }

  // If the key event does not cause any commands, and we're for single line
  // editor, let's check whether the key combination is for "select-all" in
  // GtkTextView because the signal is not supported by GtkEntry.
  if (aCommands.IsEmpty() && this == sInstanceForSingleLineEditor &&
      StaticPrefs::ui_key_use_select_all_in_single_line_editor()) {
    if (NativeKeyBindings* bindingsForMultilineEditor =
            GetInstance(NativeKeyBindingsType::MultiLineEditor)) {
      bindingsForMultilineEditor->GetEditCommands(aEvent, aWritingMode,
                                                  aCommands);
      if (aCommands.Length() == 1u &&
          aCommands[0u] == static_cast<CommandInt>(Command::SelectAll)) {
        return;
      }
      aCommands.Clear();
    }
  }

  /*
  gtk_bindings_activate_event is preferable, but it has unresolved bug:
  http://bugzilla.gnome.org/show_bug.cgi?id=162726
  The bug was already marked as FIXED.  However, somebody reports that the
  bug still exists.
  Also gtk_bindings_activate may work with some non-shortcuts operations
  (todo: check it). See bug 411005 and bug 406407.

  Code, which should be used after fixing GNOME bug 162726:

    gtk_bindings_activate_event(GTK_OBJECT(mNativeTarget),
      static_cast<GdkEventKey*>(aEvent.mNativeKeyEvent));
  */
}

bool NativeKeyBindings::GetEditCommandsInternal(
    const WidgetKeyboardEvent& aEvent, nsTArray<CommandInt>& aCommands,
    guint aKeyval) {
  guint modifiers = static_cast<GdkEventKey*>(aEvent.mNativeKeyEvent)->state;

  gCurrentCommands = &aCommands;

  gHandled = false;
  gtk_bindings_activate(G_OBJECT(mNativeTarget), aKeyval,
                        GdkModifierType(modifiers));

  gCurrentCommands = nullptr;

  return gHandled;
}

// static
void NativeKeyBindings::GetEditCommandsForTests(
    NativeKeyBindingsType aType, const WidgetKeyboardEvent& aEvent,
    const Maybe<WritingMode>& aWritingMode, nsTArray<CommandInt>& aCommands) {
  MOZ_DIAGNOSTIC_ASSERT(aEvent.IsTrusted());

  if (aEvent.IsAlt() || aEvent.IsMeta()) {
    return;
  }

  static const size_t kBackward = 0;
  static const size_t kForward = 1;
  const size_t extentSelection = aEvent.IsShift() ? 1 : 0;
  // https://github.com/GNOME/gtk/blob/1f141c19533f4b3f397c3959ade673ce243b6138/gtk/gtktext.c#L1289
  // https://github.com/GNOME/gtk/blob/c5dd34344f0c660ceffffb3bf9da43c263db16e1/gtk/gtktextview.c#L1534
  Command command = Command::DoNothing;
  const KeyNameIndex remappedKeyNameIndex =
      aWritingMode.isSome() ? aEvent.GetRemappedKeyNameIndex(aWritingMode.ref())
                            : aEvent.mKeyNameIndex;
  switch (remappedKeyNameIndex) {
    case KEY_NAME_INDEX_USE_STRING:
      switch (aEvent.PseudoCharCode()) {
        case 'a':
        case 'A':
          if (aEvent.IsControl()) {
            command = Command::SelectAll;
          }
          break;
        case 'c':
        case 'C':
          if (aEvent.IsControl() && !aEvent.IsShift()) {
            command = Command::Copy;
          }
          break;
        case 'u':
        case 'U':
          if (aType == NativeKeyBindingsType::SingleLineEditor &&
              aEvent.IsControl() && !aEvent.IsShift()) {
            command = sDeleteCommands[GTK_DELETE_PARAGRAPH_ENDS][kBackward];
          }
          break;
        case 'v':
        case 'V':
          if (aEvent.IsControl() && !aEvent.IsShift()) {
            command = Command::Paste;
          }
          break;
        case 'x':
        case 'X':
          if (aEvent.IsControl() && !aEvent.IsShift()) {
            command = Command::Cut;
          }
          break;
        case '/':
          if (aEvent.IsControl() && !aEvent.IsShift()) {
            command = Command::SelectAll;
          }
          break;
        default:
          break;
      }
      break;
    case KEY_NAME_INDEX_Insert:
      if (aEvent.IsControl() && !aEvent.IsShift()) {
        command = Command::Copy;
      } else if (aEvent.IsShift() && !aEvent.IsControl()) {
        command = Command::Paste;
      }
      break;
    case KEY_NAME_INDEX_Delete:
      if (aEvent.IsShift()) {
        command = Command::Cut;
        break;
      }
      [[fallthrough]];
    case KEY_NAME_INDEX_Backspace: {
      const size_t direction =
          remappedKeyNameIndex == KEY_NAME_INDEX_Delete ? kForward : kBackward;
      const GtkDeleteType amount =
          aEvent.IsControl() && aEvent.IsShift()
              ? GTK_DELETE_PARAGRAPH_ENDS
              // FYI: Shift key for Backspace is ignored to help mis-typing.
              : (aEvent.IsControl() ? GTK_DELETE_WORD_ENDS : GTK_DELETE_CHARS);
      command = sDeleteCommands[amount][direction];
      break;
    }
    case KEY_NAME_INDEX_ArrowLeft:
    case KEY_NAME_INDEX_ArrowRight: {
      const size_t direction = remappedKeyNameIndex == KEY_NAME_INDEX_ArrowRight
                                   ? kForward
                                   : kBackward;
      const GtkMovementStep amount = aEvent.IsControl()
                                         ? GTK_MOVEMENT_WORDS
                                         : GTK_MOVEMENT_VISUAL_POSITIONS;
      command = sMoveCommands[amount][extentSelection][direction];
      break;
    }
    case KEY_NAME_INDEX_ArrowUp:
    case KEY_NAME_INDEX_ArrowDown: {
      const size_t direction = remappedKeyNameIndex == KEY_NAME_INDEX_ArrowDown
                                   ? kForward
                                   : kBackward;
      const GtkMovementStep amount = aEvent.IsControl()
                                         ? GTK_MOVEMENT_PARAGRAPHS
                                         : GTK_MOVEMENT_DISPLAY_LINES;
      command = sMoveCommands[amount][extentSelection][direction];
      break;
    }
    case KEY_NAME_INDEX_Home:
    case KEY_NAME_INDEX_End: {
      const size_t direction =
          remappedKeyNameIndex == KEY_NAME_INDEX_End ? kForward : kBackward;
      const GtkMovementStep amount = aEvent.IsControl()
                                         ? GTK_MOVEMENT_BUFFER_ENDS
                                         : GTK_MOVEMENT_DISPLAY_LINE_ENDS;
      command = sMoveCommands[amount][extentSelection][direction];
      break;
    }
    case KEY_NAME_INDEX_PageUp:
    case KEY_NAME_INDEX_PageDown: {
      const size_t direction = remappedKeyNameIndex == KEY_NAME_INDEX_PageDown
                                   ? kForward
                                   : kBackward;
      const GtkMovementStep amount = aEvent.IsControl()
                                         ? GTK_MOVEMENT_HORIZONTAL_PAGES
                                         : GTK_MOVEMENT_PAGES;
      command = sMoveCommands[amount][extentSelection][direction];
      break;
    }
    default:
      break;
  }
  if (command != Command::DoNothing) {
    aCommands.AppendElement(static_cast<CommandInt>(command));
  }
}

}  // namespace widget
}  // namespace mozilla
