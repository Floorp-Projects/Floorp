// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// update menu items that rely on focus or on the current selection
function goUpdateGlobalEditMenuItems()
{
  // Don't bother updating the edit commands if they aren't visible in any way
  // (i.e. the Edit menu isn't open, nor is the context menu open, nor have the
  // cut, copy, and paste buttons been added to the toolbars) for performance.
  // This only works in applications/on platforms that set the gEditUIVisible
  // flag, so we check to see if that flag is defined before using it.
  if (typeof gEditUIVisible != "undefined" && !gEditUIVisible)
    return;

  goUpdateCommand("cmd_undo");
  goUpdateCommand("cmd_redo");
  goUpdateCommand("cmd_cut");
  goUpdateCommand("cmd_copy");
  goUpdateCommand("cmd_paste");
  goUpdateCommand("cmd_selectAll");
  goUpdateCommand("cmd_delete");
  goUpdateCommand("cmd_switchTextDirection");
}

// update menu items that relate to undo/redo
function goUpdateUndoEditMenuItems()
{
  goUpdateCommand("cmd_undo");
  goUpdateCommand("cmd_redo");
}

// update menu items that depend on clipboard contents
function goUpdatePasteMenuItems()
{
  goUpdateCommand("cmd_paste");
}
