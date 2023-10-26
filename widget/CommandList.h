/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Define NS_DEFINE_COMMAND(aName, aCommandStr) before including this.
 * @param aName          The name useful in C++ of the command.
 * @param aCommandStr    The command string in JS.
 *
 * Define NS_DEFINE_COMMAND_WITH_PARAM(aName, aCommandStr, aParam) before
 * including this.
 * @param aName          The name useful in C++ of the command.
 * @param aCommandStr    The command string in JS, but this may be shared with
 *                       other aName values.  I.e., cannot map aName and
 *                       aCommandStr 1:1.
 * @param aParam         Additional param value.  When aCommandStr is executed,
 *                       this value is also specified.  I.e., aName becomes
 *                       unique when you look for with both aCommandStr and
 *                       aParam.
 *
 * Define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName) before including this.
 * @param aName          The name useful in C++ of the command.
 */

// Mapped from commands of some platforms
NS_DEFINE_COMMAND(BeginLine, cmd_beginLine)
NS_DEFINE_COMMAND(CharNext, cmd_charNext)
NS_DEFINE_COMMAND(CharPrevious, cmd_charPrevious)
NS_DEFINE_COMMAND(Copy, cmd_copy)
NS_DEFINE_COMMAND(Cut, cmd_cut)
NS_DEFINE_COMMAND(Delete, cmd_delete)
NS_DEFINE_COMMAND(DeleteCharBackward, cmd_deleteCharBackward)
NS_DEFINE_COMMAND(DeleteCharForward, cmd_deleteCharForward)
NS_DEFINE_COMMAND(DeleteToBeginningOfLine, cmd_deleteToBeginningOfLine)
NS_DEFINE_COMMAND(DeleteToEndOfLine, cmd_deleteToEndOfLine)
NS_DEFINE_COMMAND(DeleteWordBackward, cmd_deleteWordBackward)
NS_DEFINE_COMMAND(DeleteWordForward, cmd_deleteWordForward)
NS_DEFINE_COMMAND(EndLine, cmd_endLine)
NS_DEFINE_COMMAND(InsertParagraph, cmd_insertParagraph)
NS_DEFINE_COMMAND(InsertLineBreak, cmd_insertLineBreak)
NS_DEFINE_COMMAND(LineNext, cmd_lineNext)
NS_DEFINE_COMMAND(LinePrevious, cmd_linePrevious)
NS_DEFINE_COMMAND(MoveBottom, cmd_moveBottom)
NS_DEFINE_COMMAND(MovePageDown, cmd_movePageDown)
NS_DEFINE_COMMAND(MovePageUp, cmd_movePageUp)
NS_DEFINE_COMMAND(MoveTop, cmd_moveTop)
NS_DEFINE_COMMAND(Paste, cmd_paste)
NS_DEFINE_COMMAND(ScrollBottom, cmd_scrollBottom)
NS_DEFINE_COMMAND(ScrollLeft, cmd_scrollLeft)
NS_DEFINE_COMMAND(ScrollLineDown, cmd_scrollLineDown)
NS_DEFINE_COMMAND(ScrollLineUp, cmd_scrollLineUp)
NS_DEFINE_COMMAND(ScrollPageDown, cmd_scrollPageDown)
NS_DEFINE_COMMAND(ScrollPageUp, cmd_scrollPageUp)
NS_DEFINE_COMMAND(ScrollRight, cmd_scrollRight)
NS_DEFINE_COMMAND(ScrollTop, cmd_scrollTop)
NS_DEFINE_COMMAND(SelectAll, cmd_selectAll)
NS_DEFINE_COMMAND(SelectBeginLine, cmd_selectBeginLine)
NS_DEFINE_COMMAND(SelectBottom, cmd_selectBottom)
NS_DEFINE_COMMAND(SelectCharNext, cmd_selectCharNext)
NS_DEFINE_COMMAND(SelectCharPrevious, cmd_selectCharPrevious)
NS_DEFINE_COMMAND(SelectEndLine, cmd_selectEndLine)
NS_DEFINE_COMMAND(SelectLineNext, cmd_selectLineNext)
NS_DEFINE_COMMAND(SelectLinePrevious, cmd_selectLinePrevious)
NS_DEFINE_COMMAND(SelectPageDown, cmd_selectPageDown)
NS_DEFINE_COMMAND(SelectPageUp, cmd_selectPageUp)
NS_DEFINE_COMMAND(SelectTop, cmd_selectTop)
NS_DEFINE_COMMAND(SelectWordNext, cmd_selectWordNext)
NS_DEFINE_COMMAND(SelectWordPrevious, cmd_selectWordPrevious)
NS_DEFINE_COMMAND(WordNext, cmd_wordNext)
NS_DEFINE_COMMAND(WordPrevious, cmd_wordPrevious)

// We don't have corresponding commands for them, but some platforms have them.
NS_DEFINE_COMMAND_NO_EXEC_COMMAND(CancelOperation)
NS_DEFINE_COMMAND_NO_EXEC_COMMAND(Complete)
NS_DEFINE_COMMAND_NO_EXEC_COMMAND(InsertBacktab)
NS_DEFINE_COMMAND_NO_EXEC_COMMAND(InsertTab)

// Commands mapped from HTMLDocument.execCommand()
NS_DEFINE_COMMAND(FormatBold, cmd_bold)
NS_DEFINE_COMMAND(FormatItalic, cmd_italic)
NS_DEFINE_COMMAND(FormatUnderline, cmd_underline)
NS_DEFINE_COMMAND(FormatStrikeThrough, cmd_strikethrough)
NS_DEFINE_COMMAND(FormatSubscript, cmd_subscript)
NS_DEFINE_COMMAND(FormatSuperscript, cmd_superscript)
NS_DEFINE_COMMAND(HistoryUndo, cmd_undo)
NS_DEFINE_COMMAND(HistoryRedo, cmd_redo)
NS_DEFINE_COMMAND(FormatBlock, cmd_formatBlock)
NS_DEFINE_COMMAND(FormatIndent, cmd_indent)
NS_DEFINE_COMMAND(FormatOutdent, cmd_outdent)
NS_DEFINE_COMMAND_WITH_PARAM(FormatJustifyLeft, cmd_align, left)
NS_DEFINE_COMMAND_WITH_PARAM(FormatJustifyRight, cmd_align, right)
NS_DEFINE_COMMAND_WITH_PARAM(FormatJustifyCenter, cmd_align, center)
NS_DEFINE_COMMAND_WITH_PARAM(FormatJustifyFull, cmd_align, justify)
NS_DEFINE_COMMAND(FormatBackColor, cmd_highlight)
NS_DEFINE_COMMAND(FormatFontColor, cmd_fontColor)
NS_DEFINE_COMMAND(FormatFontName, cmd_fontFace)
NS_DEFINE_COMMAND(FormatFontSize, cmd_fontSize)
NS_DEFINE_COMMAND(FormatIncreaseFontSize, cmd_increaseFont)
NS_DEFINE_COMMAND(FormatDecreaseFontSize, cmd_decreaseFont)
NS_DEFINE_COMMAND(InsertHorizontalRule, cmd_insertHR)
NS_DEFINE_COMMAND(InsertLink, cmd_insertLinkNoUI)
NS_DEFINE_COMMAND(InsertImage, cmd_insertImageNoUI)
NS_DEFINE_COMMAND(InsertHTML, cmd_insertHTML)
NS_DEFINE_COMMAND(InsertText, cmd_insertText)
NS_DEFINE_COMMAND(InsertOrderedList, cmd_ol)
NS_DEFINE_COMMAND(InsertUnorderedList, cmd_ul)
NS_DEFINE_COMMAND(FormatRemove, cmd_removeStyles)
NS_DEFINE_COMMAND(FormatRemoveLink, cmd_removeLinks)
NS_DEFINE_COMMAND(SetDocumentUseCSS, cmd_setDocumentUseCSS)
NS_DEFINE_COMMAND(SetDocumentReadOnly, cmd_setDocumentReadOnly)
NS_DEFINE_COMMAND(SetDocumentInsertBROnEnterKeyPress, cmd_insertBrOnReturn)
NS_DEFINE_COMMAND(SetDocumentDefaultParagraphSeparator,
                  cmd_defaultParagraphSeparator)
NS_DEFINE_COMMAND(ToggleObjectResizers, cmd_enableObjectResizing)
NS_DEFINE_COMMAND(ToggleInlineTableEditor, cmd_enableInlineTableEditing)
NS_DEFINE_COMMAND(ToggleAbsolutePositionEditor,
                  cmd_enableAbsolutePositionEditing)
NS_DEFINE_COMMAND(EnableCompatibleJoinSplitNodeDirection,
                  cmd_enableCompatibleJoinSplitNodeDirection)

// Commands not mapped from HTMLDocument.execCommand() but available with
// command dispatcher and handled in editor.
NS_DEFINE_COMMAND(CutOrDelete, cmd_cutOrDelete)
NS_DEFINE_COMMAND(CopyOrDelete, cmd_copyOrDelete)
NS_DEFINE_COMMAND(EditorObserverDocumentCreated, obs_documentCreated)
NS_DEFINE_COMMAND(EditorObserverDocumentLocationChanged,
                  obs_documentLocationChanged)
NS_DEFINE_COMMAND(EditorObserverDocumentWillBeDestroyed,
                  obs_documentWillBeDestroyed)
NS_DEFINE_COMMAND(FormatAbbreviation, cmd_abbr)
NS_DEFINE_COMMAND(FormatAbsolutePosition, cmd_absPos)
NS_DEFINE_COMMAND(FormatAcronym, cmd_acronym)
NS_DEFINE_COMMAND(FormatCitation, cmd_cite)
NS_DEFINE_COMMAND(FormatCode, cmd_code)
NS_DEFINE_COMMAND(FormatDecreaseZIndex, cmd_decreaseZIndex)
NS_DEFINE_COMMAND(FormatDocumentBackgroundColor, cmd_backgroundColor)
NS_DEFINE_COMMAND(FormatEmphasis, cmd_em)
NS_DEFINE_COMMAND(FormatIncreaseZIndex, cmd_increaseZIndex)
NS_DEFINE_COMMAND(FormatJustify, cmd_align)  // Only for getting enabled/state
NS_DEFINE_COMMAND(FormatJustifyNone, cmd_align)  // with empty string or params
NS_DEFINE_COMMAND(FormatNoBreak, cmd_nobreak)
NS_DEFINE_COMMAND(FormatRemoveList, cmd_removeList)
NS_DEFINE_COMMAND(FormatSample, cmd_samp)
NS_DEFINE_COMMAND(FormatSetBlockTextDirection, cmd_switchTextDirection)
NS_DEFINE_COMMAND(FormatStrong, cmd_strong)
NS_DEFINE_COMMAND(FormatTeletypeText, cmd_tt)
NS_DEFINE_COMMAND(FormatVariable, cmd_var)
NS_DEFINE_COMMAND(InsertDefinitionDetails, cmd_dd)
NS_DEFINE_COMMAND(InsertDefinitionTerm, cmd_dt)
NS_DEFINE_COMMAND(MoveDown, cmd_moveDown)
NS_DEFINE_COMMAND(MoveDown2, cmd_moveDown2)
NS_DEFINE_COMMAND(MoveLeft, cmd_moveLeft)
NS_DEFINE_COMMAND(MoveLeft2, cmd_moveLeft2)
NS_DEFINE_COMMAND(MoveRight, cmd_moveRight)
NS_DEFINE_COMMAND(MoveRight2, cmd_moveRight2)
NS_DEFINE_COMMAND(MoveUp, cmd_moveUp)
NS_DEFINE_COMMAND(MoveUp2, cmd_moveUp2)
NS_DEFINE_COMMAND(ParagraphState, cmd_paragraphState)
NS_DEFINE_COMMAND(PasteAsQuotation, cmd_pasteQuote)
NS_DEFINE_COMMAND(PasteTransferable, cmd_pasteTransferable)
NS_DEFINE_COMMAND(PasteWithoutFormat, cmd_pasteNoFormatting)
NS_DEFINE_COMMAND(SelectDown, cmd_selectDown)
NS_DEFINE_COMMAND(SelectDown2, cmd_selectDown2)
NS_DEFINE_COMMAND(SelectLeft, cmd_selectLeft)
NS_DEFINE_COMMAND(SelectLeft2, cmd_selectLeft2)
NS_DEFINE_COMMAND(SelectRight, cmd_selectRight)
NS_DEFINE_COMMAND(SelectRight2, cmd_selectRight2)
NS_DEFINE_COMMAND(SelectUp, cmd_selectUp)
NS_DEFINE_COMMAND(SelectUp2, cmd_selectUp2)
NS_DEFINE_COMMAND(SetDocumentModified, cmd_setDocumentModified)
