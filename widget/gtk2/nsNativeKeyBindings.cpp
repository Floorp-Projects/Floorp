/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsNativeKeyBindings.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsGtkKeyUtils.h"
#include "nsGUIEvent.h"

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
  { nsnull, nsnull } // WHITESPACE
};

static void
delete_from_cursor_cb(GtkWidget *w, GtkDeleteType del_type,
                      gint count, gpointer user_data)
{
  g_signal_stop_emission_by_name(w, "delete_from_cursor");
  gHandled = true;

  bool forward = count > 0;
  if (PRUint32(del_type) >= ArrayLength(sDeleteCommands)) {
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

  count = NS_ABS(count);
  for (int i = 0; i < count; ++i) {
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
    { nsnull, nsnull },
    { nsnull, nsnull }
  }
};

static void
move_cursor_cb(GtkWidget *w, GtkMovementStep step, gint count,
               gboolean extend_selection, gpointer user_data)
{
  g_signal_stop_emission_by_name(w, "move_cursor");
  gHandled = true;
  bool forward = count > 0;
  if (PRUint32(step) >= ArrayLength(sMoveCommands)) {
    // unsupported movement type
    return;
  }

  const char *cmd = sMoveCommands[step][extend_selection][forward];
  if (!cmd)
    return; // unsupported command

  
  count = NS_ABS(count);
  for (int i = 0; i < count; ++i) {
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
nsNativeKeyBindings::KeyDown(const nsNativeKeyEvent& aEvent,
                             DoCommandCallback aCallback, void *aCallbackData)
{
  return false;
}

bool
nsNativeKeyBindings::KeyPress(const nsNativeKeyEvent& aEvent,
                              DoCommandCallback aCallback, void *aCallbackData)
{
  PRUint32 keyCode;

  if (aEvent.charCode != 0)
    keyCode = gdk_unicode_to_keyval(aEvent.charCode);
  else
    keyCode = KeymapWrapper::GuessGDKKeyval(aEvent.keyCode);

  if (KeyPressInternal(aEvent, aCallback, aCallbackData, keyCode))
    return true;

  nsKeyEvent *nativeKeyEvent = static_cast<nsKeyEvent*>(aEvent.nativeEvent);
  if (!nativeKeyEvent ||
      (nativeKeyEvent->eventStructType != NS_KEY_EVENT &&
       nativeKeyEvent->message != NS_KEY_PRESS)) {
    return false;
  }

  for (PRUint32 i = 0; i < nativeKeyEvent->alternativeCharCodes.Length(); ++i) {
    PRUint32 ch = nativeKeyEvent->IsShift() ?
        nativeKeyEvent->alternativeCharCodes[i].mShiftedCharCode :
        nativeKeyEvent->alternativeCharCodes[i].mUnshiftedCharCode;
    if (ch && ch != aEvent.charCode) {
      keyCode = gdk_unicode_to_keyval(ch);
      if (KeyPressInternal(aEvent, aCallback, aCallbackData, keyCode))
        return true;
    }
  }

/* gtk_bindings_activate_event is preferable, but it has unresolved bug: http://bugzilla.gnome.org/show_bug.cgi?id=162726
Also gtk_bindings_activate may work with some non-shortcuts operations (todo: check it)
See bugs 411005 406407

  Code, which should be used after fixing http://bugzilla.gnome.org/show_bug.cgi?id=162726:
  const nsGUIEvent *guiEvent = static_cast<nsGUIEvent*>(aEvent.nativeEvent);
  if (guiEvent &&
     (guiEvent->message == NS_KEY_PRESS || guiEvent->message == NS_KEY_UP || guiEvent->message == NS_KEY_DOWN) &&
      guiEvent->pluginEvent)
        gtk_bindings_activate_event(GTK_OBJECT(mNativeTarget),
                                    static_cast<GdkEventKey*>(guiEvent->pluginEvent));
*/

  return false;
}

bool
nsNativeKeyBindings::KeyPressInternal(const nsNativeKeyEvent& aEvent,
                                      DoCommandCallback aCallback,
                                      void *aCallbackData,
                                      PRUint32 aKeyCode)
{
  int modifiers = 0;
  if (aEvent.altKey)
    modifiers |= GDK_MOD1_MASK;
  if (aEvent.ctrlKey)
    modifiers |= GDK_CONTROL_MASK;
  if (aEvent.shiftKey)
    modifiers |= GDK_SHIFT_MASK;
  // we don't support meta

  gCurrentCallback = aCallback;
  gCurrentCallbackData = aCallbackData;

  gHandled = false;

  gtk_bindings_activate(GTK_OBJECT(mNativeTarget),
                        aKeyCode, GdkModifierType(modifiers));

  gCurrentCallback = nsnull;
  gCurrentCallbackData = nsnull;

  return gHandled;
}

bool
nsNativeKeyBindings::KeyUp(const nsNativeKeyEvent& aEvent,
                           DoCommandCallback aCallback, void *aCallbackData)
{
  return false;
}
