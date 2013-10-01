/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"
#include "mozilla/TextEvents.h"
#include "mozilla/Util.h"

#include "nsNativeKeyBindings.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsGtkKeyUtils.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>

using namespace mozilla;
using namespace mozilla::widget;

static nsINativeKeyBindings::DoCommandCallback gCurrentCallback;
static void *gCurrentCallbackData;
static bool gHandled;

// Common GtkEntry and GtkTextView signals
static void
copy_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback("cmd_copy", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "copy_clipboard");
  gHandled = true;
}

static void
cut_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback("cmd_cut", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "cut_clipboard");
  gHandled = true;
}

// GTK distinguishes between display lines (wrapped, as they appear on the
// screen) and paragraphs, which are runs of text terminated by a newline.
// We don't have this distinction, so we always use editor's notion of
// lines, which are newline-terminated.

static const char *const sDeleteCommands[][2] = {
  // backward, forward
  { "cmd_deleteCharBackward", "cmd_deleteCharForward" },    // CHARS
  { "cmd_deleteWordBackward", "cmd_deleteWordForward" },    // WORD_ENDS
  { "cmd_deleteWordBackward", "cmd_deleteWordForward" },    // WORDS
  { "cmd_deleteToBeginningOfLine", "cmd_deleteToEndOfLine" }, // LINES
  { "cmd_deleteToBeginningOfLine", "cmd_deleteToEndOfLine" }, // LINE_ENDS
  { "cmd_deleteToBeginningOfLine", "cmd_deleteToEndOfLine" }, // PARAGRAPH_ENDS
  { "cmd_deleteToBeginningOfLine", "cmd_deleteToEndOfLine" }, // PARAGRAPHS
  // This deletes from the end of the previous word to the beginning of the
  // next word, but only if the caret is not in a word.
  // XXX need to implement in editor
  { nullptr, nullptr } // WHITESPACE
};

static void
delete_from_cursor_cb(GtkWidget *w, GtkDeleteType del_type,
                      gint count, gpointer user_data)
{
  g_signal_stop_emission_by_name(w, "delete_from_cursor");
  gHandled = true;

  bool forward = count > 0;
  if (uint32_t(del_type) >= ArrayLength(sDeleteCommands)) {
    // unsupported deletion type
    return;
  }

  if (del_type == GTK_DELETE_WORDS) {
    // This works like word_ends, except we first move the caret to the
    // beginning/end of the current word.
    if (forward) {
      gCurrentCallback("cmd_wordNext", gCurrentCallbackData);
      gCurrentCallback("cmd_wordPrevious", gCurrentCallbackData);
    } else {
      gCurrentCallback("cmd_wordPrevious", gCurrentCallbackData);
      gCurrentCallback("cmd_wordNext", gCurrentCallbackData);
    }
  } else if (del_type == GTK_DELETE_DISPLAY_LINES ||
             del_type == GTK_DELETE_PARAGRAPHS) {

    // This works like display_line_ends, except we first move the caret to the
    // beginning/end of the current line.
    if (forward) {
      gCurrentCallback("cmd_beginLine", gCurrentCallbackData);
    } else {
      gCurrentCallback("cmd_endLine", gCurrentCallbackData);
    }
  }

  const char *cmd = sDeleteCommands[del_type][forward];
  if (!cmd)
    return; // unsupported command

  unsigned int absCount = Abs(count);
  for (unsigned int i = 0; i < absCount; ++i) {
    gCurrentCallback(cmd, gCurrentCallbackData);
  }
}

static const char *const sMoveCommands[][2][2] = {
  // non-extend { backward, forward }, extend { backward, forward }
  // GTK differentiates between logical position, which is prev/next,
  // and visual position, which is always left/right.
  // We should fix this to work the same way for RTL text input.
  { // LOGICAL_POSITIONS
    { "cmd_charPrevious", "cmd_charNext" },
    { "cmd_selectCharPrevious", "cmd_selectCharNext" }
  },
  { // VISUAL_POSITIONS
    { "cmd_charPrevious", "cmd_charNext" },
    { "cmd_selectCharPrevious", "cmd_selectCharNext" }
  },
  { // WORDS
    { "cmd_wordPrevious", "cmd_wordNext" },
    { "cmd_selectWordPrevious", "cmd_selectWordNext" }
  },
  { // DISPLAY_LINES
    { "cmd_linePrevious", "cmd_lineNext" },
    { "cmd_selectLinePrevious", "cmd_selectLineNext" }
  },
  { // DISPLAY_LINE_ENDS
    { "cmd_beginLine", "cmd_endLine" },
    { "cmd_selectBeginLine", "cmd_selectEndLine" }
  },
  { // PARAGRAPHS
    { "cmd_linePrevious", "cmd_lineNext" },
    { "cmd_selectLinePrevious", "cmd_selectLineNext" }
  },
  { // PARAGRAPH_ENDS
    { "cmd_beginLine", "cmd_endLine" },
    { "cmd_selectBeginLine", "cmd_selectEndLine" }
  },
  { // PAGES
    { "cmd_movePageUp", "cmd_movePageDown" },
    { "cmd_selectPageUp", "cmd_selectPageDown" }
  },
  { // BUFFER_ENDS
    { "cmd_moveTop", "cmd_moveBottom" },
    { "cmd_selectTop", "cmd_selectBottom" }
  },
  { // HORIZONTAL_PAGES (unsupported)
    { nullptr, nullptr },
    { nullptr, nullptr }
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

  const char *cmd = sMoveCommands[step][extend_selection][forward];
  if (!cmd)
    return; // unsupported command

  
  unsigned int absCount = Abs(count);
  for (unsigned int i = 0; i < absCount; ++i) {
    gCurrentCallback(cmd, gCurrentCallbackData);
  }
}

static void
paste_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback("cmd_paste", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "paste_clipboard");
  gHandled = true;
}

// GtkTextView-only signals
static void
select_all_cb(GtkWidget *w, gboolean select, gpointer user_data)
{
  gCurrentCallback("cmd_selectAll", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "select_all");
  gHandled = true;
}

void
nsNativeKeyBindings::Init(NativeKeyBindingsType  aType)
{
  switch (aType) {
  case eKeyBindings_Input:
    mNativeTarget = gtk_entry_new();
    break;
  case eKeyBindings_TextArea:
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

nsNativeKeyBindings::~nsNativeKeyBindings()
{
  gtk_widget_destroy(mNativeTarget);
  g_object_unref(mNativeTarget);
}

NS_IMPL_ISUPPORTS1(nsNativeKeyBindings, nsINativeKeyBindings)

bool
nsNativeKeyBindings::KeyDown(const WidgetKeyboardEvent& aEvent,
                             DoCommandCallback aCallback, void *aCallbackData)
{
  return false;
}

bool
nsNativeKeyBindings::KeyPress(const WidgetKeyboardEvent& aEvent,
                              DoCommandCallback aCallback, void *aCallbackData)
{
  // If the native key event is set, it must be synthesized for tests.
  // We just ignore such events because this behavior depends on system
  // settings.
  if (!aEvent.mNativeKeyEvent) {
    // It must be synthesized event or dispatched DOM event from chrome.
    return false;
  }

  guint keyval;

  if (aEvent.charCode) {
    keyval = gdk_unicode_to_keyval(aEvent.charCode);
  } else {
    keyval =
      static_cast<GdkEventKey*>(aEvent.mNativeKeyEvent)->keyval;
  }

  if (KeyPressInternal(aEvent, aCallback, aCallbackData, keyval)) {
    return true;
  }

  for (uint32_t i = 0; i < aEvent.alternativeCharCodes.Length(); ++i) {
    uint32_t ch = aEvent.IsShift() ?
      aEvent.alternativeCharCodes[i].mShiftedCharCode :
      aEvent.alternativeCharCodes[i].mUnshiftedCharCode;
    if (ch && ch != aEvent.charCode) {
      keyval = gdk_unicode_to_keyval(ch);
      if (KeyPressInternal(aEvent, aCallback, aCallbackData, keyval)) {
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
nsNativeKeyBindings::KeyPressInternal(const WidgetKeyboardEvent& aEvent,
                                      DoCommandCallback aCallback,
                                      void *aCallbackData,
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

bool
nsNativeKeyBindings::KeyUp(const WidgetKeyboardEvent& aEvent,
                           DoCommandCallback aCallback, void *aCallbackData)
{
  return false;
}
