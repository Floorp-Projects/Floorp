/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNativeKeyBindings.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsGtkKeyUtils.h"
#include "nsGUIEvent.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>

static nsINativeKeyBindings::DoCommandCallback gCurrentCallback;
static void *gCurrentCallbackData;
static PRBool gHandled;

// Common GtkEntry and GtkTextView signals
static void
copy_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback("cmd_copy", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "copy_clipboard");
  gHandled = PR_TRUE;
}

static void
cut_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback("cmd_cut", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "cut_clipboard");
  gHandled = PR_TRUE;
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
  gHandled = PR_TRUE;

  PRBool forward = count > 0;
  if (PRUint32(del_type) >= NS_ARRAY_LENGTH(sDeleteCommands)) {
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

  count = PR_ABS(count);
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
  gHandled = PR_TRUE;
  PRBool forward = count > 0;
  if (PRUint32(step) >= NS_ARRAY_LENGTH(sMoveCommands)) {
    // unsupported movement type
    return;
  }

  const char *cmd = sMoveCommands[step][extend_selection][forward];
  if (!cmd)
    return; // unsupported command

  
  count = PR_ABS(count);
  for (int i = 0; i < count; ++i) {
    gCurrentCallback(cmd, gCurrentCallbackData);
  }
}

static void
paste_clipboard_cb(GtkWidget *w, gpointer user_data)
{
  gCurrentCallback("cmd_paste", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "paste_clipboard");
  gHandled = PR_TRUE;
}

// GtkTextView-only signals
static void
select_all_cb(GtkWidget *w, gboolean select, gpointer user_data)
{
  gCurrentCallback("cmd_selectAll", gCurrentCallbackData);
  g_signal_stop_emission_by_name(w, "select_all");
  gHandled = PR_TRUE;
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
      g_signal_connect(G_OBJECT(mNativeTarget), "select_all",
                       G_CALLBACK(select_all_cb), this);
    }
    break;
  }

  g_object_ref_sink(mNativeTarget);

  g_signal_connect(G_OBJECT(mNativeTarget), "copy_clipboard",
                   G_CALLBACK(copy_clipboard_cb), this);
  g_signal_connect(G_OBJECT(mNativeTarget), "cut_clipboard",
                   G_CALLBACK(cut_clipboard_cb), this);
  g_signal_connect(G_OBJECT(mNativeTarget), "delete_from_cursor",
                   G_CALLBACK(delete_from_cursor_cb), this);
  g_signal_connect(G_OBJECT(mNativeTarget), "move_cursor",
                   G_CALLBACK(move_cursor_cb), this);
  g_signal_connect(G_OBJECT(mNativeTarget), "paste_clipboard",
                   G_CALLBACK(paste_clipboard_cb), this);
}

nsNativeKeyBindings::~nsNativeKeyBindings()
{
  gtk_widget_destroy(mNativeTarget);
  g_object_unref(mNativeTarget);
}

NS_IMPL_ISUPPORTS1(nsNativeKeyBindings, nsINativeKeyBindings)

PRBool
nsNativeKeyBindings::KeyDown(const nsNativeKeyEvent& aEvent,
                             DoCommandCallback aCallback, void *aCallbackData)
{
  return PR_FALSE;
}

PRBool
nsNativeKeyBindings::KeyPress(const nsNativeKeyEvent& aEvent,
                              DoCommandCallback aCallback, void *aCallbackData)
{
  PRUint32 keyCode;

  if (aEvent.charCode != 0)
    keyCode = gdk_unicode_to_keyval(aEvent.charCode);
  else
    keyCode = DOMKeyCodeToGdkKeyCode(aEvent.keyCode);

  if (KeyPressInternal(aEvent, aCallback, aCallbackData, keyCode))
    return PR_TRUE;

  nsKeyEvent *nativeKeyEvent = static_cast<nsKeyEvent*>(aEvent.nativeEvent);
  if (!nativeKeyEvent || nativeKeyEvent->eventStructType != NS_KEY_EVENT &&
      nativeKeyEvent->message != NS_KEY_PRESS)
    return PR_FALSE;

  for (PRUint32 i = 0; i < nativeKeyEvent->alternativeCharCodes.Length(); ++i) {
    PRUint32 ch = nativeKeyEvent->isShift ?
        nativeKeyEvent->alternativeCharCodes[i].mShiftedCharCode :
        nativeKeyEvent->alternativeCharCodes[i].mUnshiftedCharCode;
    if (ch && ch != aEvent.charCode) {
      keyCode = gdk_unicode_to_keyval(ch);
      if (KeyPressInternal(aEvent, aCallback, aCallbackData, keyCode))
        return PR_TRUE;
    }
  }

/* gtk_bindings_activate_event is preferable, but it has unresolved bug: http://bugzilla.gnome.org/show_bug.cgi?id=162726
Also gtk_bindings_activate may work with some non-shortcuts operations (todo: check it)
See bugs 411005 406407

  Code, which should be used after fixing http://bugzilla.gnome.org/show_bug.cgi?id=162726:
  const nsGUIEvent *guiEvent = static_cast<nsGUIEvent*>(aEvent.nativeEvent);
  if (guiEvent &&
     (guiEvent->message == NS_KEY_PRESS || guiEvent->message == NS_KEY_UP || guiEvent->message == NS_KEY_DOWN) &&
      guiEvent->nativeMsg)
        gtk_bindings_activate_event(GTK_OBJECT(mNativeTarget),
                                    static_cast<GdkEventKey*>(guiEvent->nativeMsg));
*/

  return PR_FALSE;
}

PRBool
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

  gHandled = PR_FALSE;

  gtk_bindings_activate(GTK_OBJECT(mNativeTarget),
                        aKeyCode, GdkModifierType(modifiers));

  gCurrentCallback = nsnull;
  gCurrentCallbackData = nsnull;

  return gHandled;
}

PRBool
nsNativeKeyBindings::KeyUp(const nsNativeKeyEvent& aEvent,
                           DoCommandCallback aCallback, void *aCallbackData)
{
  return PR_FALSE;
}
