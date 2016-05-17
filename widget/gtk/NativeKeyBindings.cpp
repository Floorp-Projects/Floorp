/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/TextEvents.h"

#include "NativeKeyBindings.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsGtkKeyUtils.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>

namespace mozilla {
namespace widget {

static nsIWidget::DoCommandCallback gCurrentCallback;
static void *gCurrentCallbackData;
static bool gHandled;

// Common GtkEntry and GtkTextView signals
static void
copy_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback(CommandCopy, gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "copy_clipboard");
  gHandled = true;
}

static void
cut_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback(CommandCut, gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "cut_clipboard");
  gHandled = true;
}

// GTK distinguishes between display lines (wrapped, as they appear on the
// screen) and paragraphs, which are runs of text terminated by a newline.
// We don't have this distinction, so we always use editor's notion of
// lines, which are newline-terminated.

static const Command sDeleteCommands[][2] = {
  // backward, forward
  { CommandDeleteCharBackward, CommandDeleteCharForward },    // CHARS
  { CommandDeleteWordBackward, CommandDeleteWordForward },    // WORD_ENDS
  { CommandDeleteWordBackward, CommandDeleteWordForward },    // WORDS
  { CommandDeleteToBeginningOfLine, CommandDeleteToEndOfLine }, // LINES
  { CommandDeleteToBeginningOfLine, CommandDeleteToEndOfLine }, // LINE_ENDS
  { CommandDeleteToBeginningOfLine, CommandDeleteToEndOfLine }, // PARAGRAPH_ENDS
  { CommandDeleteToBeginningOfLine, CommandDeleteToEndOfLine }, // PARAGRAPHS
  // This deletes from the end of the previous word to the beginning of the
  // next word, but only if the caret is not in a word.
  // XXX need to implement in editor
  { CommandDoNothing, CommandDoNothing } // WHITESPACE
};

static void
delete_from_cursor_cb(GtkWidget *w, GtkDeleteType del_type,
                      gint count, gpointer user_data)
{
  g_signal_stop_emission_by_name(w, "delete_from_cursor");
  bool forward = count > 0;

#if (MOZ_WIDGET_GTK == 3)
  // Ignore GTK's Ctrl-K keybinding introduced in GTK 3.14 and removed in
  // 3.18 if the user has custom bindings set. See bug 1176929.
  if (del_type == GTK_DELETE_PARAGRAPH_ENDS && forward && GTK_IS_ENTRY(w) &&
      !gtk_check_version(3, 14, 1) && gtk_check_version(3, 17, 9)) {
    GtkStyleContext* context = gtk_widget_get_style_context(w);
    GtkStateFlags flags = gtk_widget_get_state_flags(w);

    GPtrArray* array;
    gtk_style_context_get(context, flags, "gtk-key-bindings", &array, nullptr);
    if (!array)
      return;
    g_ptr_array_unref(array);
  }
#endif

  gHandled = true;
  if (uint32_t(del_type) >= ArrayLength(sDeleteCommands)) {
    // unsupported deletion type
    return;
  }

  if (del_type == GTK_DELETE_WORDS) {
    // This works like word_ends, except we first move the caret to the
    // beginning/end of the current word.
    if (forward) {
      gCurrentCallback(CommandWordNext, gCurrentCallbackData);
      gCurrentCallback(CommandWordPrevious, gCurrentCallbackData);
    } else {
      gCurrentCallback(CommandWordPrevious, gCurrentCallbackData);
      gCurrentCallback(CommandWordNext, gCurrentCallbackData);
    }
  } else if (del_type == GTK_DELETE_DISPLAY_LINES ||
             del_type == GTK_DELETE_PARAGRAPHS) {

    // This works like display_line_ends, except we first move the caret to the
    // beginning/end of the current line.
    if (forward) {
      gCurrentCallback(CommandBeginLine, gCurrentCallbackData);
    } else {
      gCurrentCallback(CommandEndLine, gCurrentCallbackData);
    }
  }

  Command command = sDeleteCommands[del_type][forward];
  if (!command) {
    return; // unsupported command
  }

  unsigned int absCount = Abs(count);
  for (unsigned int i = 0; i < absCount; ++i) {
    gCurrentCallback(command, gCurrentCallbackData);
  }
}

static const Command sMoveCommands[][2][2] = {
  // non-extend { backward, forward }, extend { backward, forward }
  // GTK differentiates between logical position, which is prev/next,
  // and visual position, which is always left/right.
  // We should fix this to work the same way for RTL text input.
  { // LOGICAL_POSITIONS
    { CommandCharPrevious, CommandCharNext },
    { CommandSelectCharPrevious, CommandSelectCharNext }
  },
  { // VISUAL_POSITIONS
    { CommandCharPrevious, CommandCharNext },
    { CommandSelectCharPrevious, CommandSelectCharNext }
  },
  { // WORDS
    { CommandWordPrevious, CommandWordNext },
    { CommandSelectWordPrevious, CommandSelectWordNext }
  },
  { // DISPLAY_LINES
    { CommandLinePrevious, CommandLineNext },
    { CommandSelectLinePrevious, CommandSelectLineNext }
  },
  { // DISPLAY_LINE_ENDS
    { CommandBeginLine, CommandEndLine },
    { CommandSelectBeginLine, CommandSelectEndLine }
  },
  { // PARAGRAPHS
    { CommandLinePrevious, CommandLineNext },
    { CommandSelectLinePrevious, CommandSelectLineNext }
  },
  { // PARAGRAPH_ENDS
    { CommandBeginLine, CommandEndLine },
    { CommandSelectBeginLine, CommandSelectEndLine }
  },
  { // PAGES
    { CommandMovePageUp, CommandMovePageDown },
    { CommandSelectPageUp, CommandSelectPageDown }
  },
  { // BUFFER_ENDS
    { CommandMoveTop, CommandMoveBottom },
    { CommandSelectTop, CommandSelectBottom }
  },
  { // HORIZONTAL_PAGES (unsupported)
    { CommandDoNothing, CommandDoNothing },
    { CommandDoNothing, CommandDoNothing }
  }
};

static void
move_cursor_cb(GtkWidget *w, GtkMovementStep step, gint count,
               gboolean extend_selection, gpointer user_data)
{
  g_signal_stop_emission_by_name(w, "move_cursor");
  gHandled = true;
  bool forward = count > 0;
  if (uint32_t(step) >= ArrayLength(sMoveCommands)) {
    // unsupported movement type
    return;
  }

  Command command = sMoveCommands[step][extend_selection][forward];
  if (!command) {
    return; // unsupported command
  }

  unsigned int absCount = Abs(count);
  for (unsigned int i = 0; i < absCount; ++i) {
    gCurrentCallback(command, gCurrentCallbackData);
  }
}

static void
paste_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback(CommandPaste, gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "paste_clipboard");
  gHandled = true;
}

// GtkTextView-only signals
static void
select_all_cb(GtkWidget *w, gboolean select, gpointer user_data)
{
  gCurrentCallback(CommandSelectAll, gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "select_all");
  gHandled = true;
}

NativeKeyBindings* NativeKeyBindings::sInstanceForSingleLineEditor = nullptr;
NativeKeyBindings* NativeKeyBindings::sInstanceForMultiLineEditor = nullptr;

// static
NativeKeyBindings*
NativeKeyBindings::GetInstance(NativeKeyBindingsType aType)
{
  switch (aType) {
    case nsIWidget::NativeKeyBindingsForSingleLineEditor:
      if (!sInstanceForSingleLineEditor) {
        sInstanceForSingleLineEditor = new NativeKeyBindings();
        sInstanceForSingleLineEditor->Init(aType);
      }
      return sInstanceForSingleLineEditor;

    default:
      // fallback to multiline editor case in release build
      MOZ_FALLTHROUGH_ASSERT("aType is invalid or not yet implemented");
    case nsIWidget::NativeKeyBindingsForMultiLineEditor:
    case nsIWidget::NativeKeyBindingsForRichTextEditor:
      if (!sInstanceForMultiLineEditor) {
        sInstanceForMultiLineEditor = new NativeKeyBindings();
        sInstanceForMultiLineEditor->Init(aType);
      }
      return sInstanceForMultiLineEditor;
  }
}

// static
void
NativeKeyBindings::Shutdown()
{
  delete sInstanceForSingleLineEditor;
  sInstanceForSingleLineEditor = nullptr;
  delete sInstanceForMultiLineEditor;
  sInstanceForMultiLineEditor = nullptr;
}

void
NativeKeyBindings::Init(NativeKeyBindingsType  aType)
{
  switch (aType) {
  case nsIWidget::NativeKeyBindingsForSingleLineEditor:
    mNativeTarget = gtk_entry_new();
    break;
  default:
    mNativeTarget = gtk_text_view_new();
    if (gtk_major_version > 2 ||
        (gtk_major_version == 2 && (gtk_minor_version > 2 ||
                                    (gtk_minor_version == 2 &&
                                     gtk_micro_version >= 2)))) {
      // select_all only exists in gtk >= 2.2.2.  Prior to that,
      // ctrl+a is bound to (move to beginning, select to end).
      g_signal_connect(mNativeTarget, "select_all",
                       G_CALLBACK(select_all_cb), this);
    }
    break;
  }

  g_object_ref_sink(mNativeTarget);

  g_signal_connect(mNativeTarget, "copy_clipboard",
                   G_CALLBACK(copy_clipboard_cb), this);
  g_signal_connect(mNativeTarget, "cut_clipboard",
                   G_CALLBACK(cut_clipboard_cb), this);
  g_signal_connect(mNativeTarget, "delete_from_cursor",
                   G_CALLBACK(delete_from_cursor_cb), this);
  g_signal_connect(mNativeTarget, "move_cursor",
                   G_CALLBACK(move_cursor_cb), this);
  g_signal_connect(mNativeTarget, "paste_clipboard",
                   G_CALLBACK(paste_clipboard_cb), this);
}

NativeKeyBindings::~NativeKeyBindings()
{
  gtk_widget_destroy(mNativeTarget);
  g_object_unref(mNativeTarget);
}

bool
NativeKeyBindings::Execute(const WidgetKeyboardEvent& aEvent,
                           DoCommandCallback aCallback,
                           void* aCallbackData)
{
  // If the native key event is set, it must be synthesized for tests.
  // We just ignore such events because this behavior depends on system
  // settings.
  if (!aEvent.mNativeKeyEvent) {
    // It must be synthesized event or dispatched DOM event from chrome.
    return false;
  }

  guint keyval;

  if (aEvent.mCharCode) {
    keyval = gdk_unicode_to_keyval(aEvent.mCharCode);
  } else {
    keyval =
      static_cast<GdkEventKey*>(aEvent.mNativeKeyEvent)->keyval;
  }

  if (ExecuteInternal(aEvent, aCallback, aCallbackData, keyval)) {
    return true;
  }

  for (uint32_t i = 0; i < aEvent.mAlternativeCharCodes.Length(); ++i) {
    uint32_t ch = aEvent.IsShift() ?
      aEvent.mAlternativeCharCodes[i].mShiftedCharCode :
      aEvent.mAlternativeCharCodes[i].mUnshiftedCharCode;
    if (ch && ch != aEvent.mCharCode) {
      keyval = gdk_unicode_to_keyval(ch);
      if (ExecuteInternal(aEvent, aCallback, aCallbackData, keyval)) {
        return true;
      }
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

  return false;
}

bool
NativeKeyBindings::ExecuteInternal(const WidgetKeyboardEvent& aEvent,
                                   DoCommandCallback aCallback,
                                   void* aCallbackData,
                                   guint aKeyval)
{
  guint modifiers =
    static_cast<GdkEventKey*>(aEvent.mNativeKeyEvent)->state;

  gCurrentCallback = aCallback;
  gCurrentCallbackData = aCallbackData;

  gHandled = false;
#if (MOZ_WIDGET_GTK == 2)
  gtk_bindings_activate(GTK_OBJECT(mNativeTarget),
                        aKeyval, GdkModifierType(modifiers));
#else
  gtk_bindings_activate(G_OBJECT(mNativeTarget),
                        aKeyval, GdkModifierType(modifiers));
#endif

  gCurrentCallback = nullptr;
  gCurrentCallbackData = nullptr;

  return gHandled;
}

} // namespace widget
} // namespace mozilla
