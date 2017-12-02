/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Define NS_DEFINE_COMMAND(aName, aCommandStr) before including this.
 * @param aName          The name useful in C++ of the command.
 * @param aCommandStr    The command string in JS.
 *
 * Define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName) before including this.
 * @param aName          The name useful in C++ of the command.
 */

NS_DEFINE_COMMAND(BeginLine,                   cmd_beginLine)
NS_DEFINE_COMMAND(CharNext,                    cmd_charNext)
NS_DEFINE_COMMAND(CharPrevious,                cmd_charPrevious)
NS_DEFINE_COMMAND(Copy,                        cmd_copy)
NS_DEFINE_COMMAND(Cut,                         cmd_cut)
NS_DEFINE_COMMAND(Delete,                      cmd_delete)
NS_DEFINE_COMMAND(DeleteCharBackward,          cmd_deleteCharBackward)
NS_DEFINE_COMMAND(DeleteCharForward,           cmd_deleteCharForward)
NS_DEFINE_COMMAND(DeleteToBeginningOfLine,     cmd_deleteToBeginningOfLine)
NS_DEFINE_COMMAND(DeleteToEndOfLine,           cmd_deleteToEndOfLine)
NS_DEFINE_COMMAND(DeleteWordBackward,          cmd_deleteWordBackward)
NS_DEFINE_COMMAND(DeleteWordForward,           cmd_deleteWordForward)
NS_DEFINE_COMMAND(EndLine,                     cmd_endLine)
NS_DEFINE_COMMAND(InsertParagraph,             cmd_insertParagraph)
NS_DEFINE_COMMAND(InsertLineBreak,             cmd_insertLineBreak)
NS_DEFINE_COMMAND(LineNext,                    cmd_lineNext)
NS_DEFINE_COMMAND(LinePrevious,                cmd_linePrevious)
NS_DEFINE_COMMAND(MoveBottom,                  cmd_moveBottom)
NS_DEFINE_COMMAND(MovePageDown,                cmd_movePageDown)
NS_DEFINE_COMMAND(MovePageUp,                  cmd_movePageUp)
NS_DEFINE_COMMAND(MoveTop,                     cmd_moveTop)
NS_DEFINE_COMMAND(Paste,                       cmd_paste)
NS_DEFINE_COMMAND(ScrollBottom,                cmd_scrollBottom)
NS_DEFINE_COMMAND(ScrollLineDown,              cmd_scrollLineDown)
NS_DEFINE_COMMAND(ScrollLineUp,                cmd_scrollLineUp)
NS_DEFINE_COMMAND(ScrollPageDown,              cmd_scrollPageDown)
NS_DEFINE_COMMAND(ScrollPageUp,                cmd_scrollPageUp)
NS_DEFINE_COMMAND(ScrollTop,                   cmd_scrollTop)
NS_DEFINE_COMMAND(SelectAll,                   cmd_selectAll)
NS_DEFINE_COMMAND(SelectBeginLine,             cmd_selectBeginLine)
NS_DEFINE_COMMAND(SelectBottom,                cmd_selectBottom)
NS_DEFINE_COMMAND(SelectCharNext,              cmd_selectCharNext)
NS_DEFINE_COMMAND(SelectCharPrevious,          cmd_selectCharPrevious)
NS_DEFINE_COMMAND(SelectEndLine,               cmd_selectEndLine)
NS_DEFINE_COMMAND(SelectLineNext,              cmd_selectLineNext)
NS_DEFINE_COMMAND(SelectLinePrevious,          cmd_selectLinePrevious)
NS_DEFINE_COMMAND(SelectPageDown,              cmd_selectPageDown)
NS_DEFINE_COMMAND(SelectPageUp,                cmd_selectPageUp)
NS_DEFINE_COMMAND(SelectTop,                   cmd_selectTop)
NS_DEFINE_COMMAND(SelectWordNext,              cmd_selectWordNext)
NS_DEFINE_COMMAND(SelectWordPrevious,          cmd_selectWordPrevious)
NS_DEFINE_COMMAND(WordNext,                    cmd_wordNext)
NS_DEFINE_COMMAND(WordPrevious,                cmd_wordPrevious)

NS_DEFINE_COMMAND_NO_EXEC_COMMAND(CancelOperation)
NS_DEFINE_COMMAND_NO_EXEC_COMMAND(Complete)
NS_DEFINE_COMMAND_NO_EXEC_COMMAND(InsertBacktab)
NS_DEFINE_COMMAND_NO_EXEC_COMMAND(InsertTab)
