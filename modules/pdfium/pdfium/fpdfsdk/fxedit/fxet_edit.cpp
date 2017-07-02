// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fpdfsdk/fxedit/fxet_edit.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_pageobject.h"
#include "core/fpdfapi/page/cpdf_pageobjectholder.h"
#include "core/fpdfapi/page/cpdf_pathobject.h"
#include "core/fpdfapi/page/cpdf_textobject.h"
#include "core/fpdfapi/parser/fpdf_parser_decode.h"
#include "core/fpdfapi/render/cpdf_renderoptions.h"
#include "core/fpdfapi/render/cpdf_textrenderer.h"
#include "core/fpdfdoc/cpvt_section.h"
#include "core/fpdfdoc/cpvt_word.h"
#include "core/fpdfdoc/ipvt_fontmap.h"
#include "core/fxge/cfx_graphstatedata.h"
#include "core/fxge/cfx_pathdata.h"
#include "core/fxge/cfx_renderdevice.h"
#include "fpdfsdk/cfx_systemhandler.h"
#include "fpdfsdk/fxedit/fx_edit.h"
#include "fpdfsdk/pdfwindow/PWL_Edit.h"
#include "fpdfsdk/pdfwindow/PWL_EditCtrl.h"
#include "third_party/base/ptr_util.h"
#include "third_party/base/stl_util.h"

namespace {

const int kEditUndoMaxItems = 10000;

CFX_ByteString GetWordRenderString(const CFX_ByteString& strWords) {
  if (strWords.GetLength() > 0)
    return PDF_EncodeString(strWords) + " Tj\n";
  return CFX_ByteString();
}

CFX_ByteString GetFontSetString(IPVT_FontMap* pFontMap,
                                int32_t nFontIndex,
                                FX_FLOAT fFontSize) {
  if (!pFontMap)
    return CFX_ByteString();

  CFX_ByteString sFontAlias = pFontMap->GetPDFFontAlias(nFontIndex);
  if (sFontAlias.GetLength() <= 0 || fFontSize <= 0)
    return CFX_ByteString();

  CFX_ByteTextBuf sRet;
  sRet << "/" << sFontAlias << " " << fFontSize << " Tf\n";
  return sRet.MakeString();
}

void DrawTextString(CFX_RenderDevice* pDevice,
                    const CFX_PointF& pt,
                    CPDF_Font* pFont,
                    FX_FLOAT fFontSize,
                    CFX_Matrix* pUser2Device,
                    const CFX_ByteString& str,
                    FX_ARGB crTextFill,
                    int32_t nHorzScale) {
  CFX_PointF pos = pUser2Device->Transform(pt);

  if (pFont) {
    if (nHorzScale != 100) {
      CFX_Matrix mt(nHorzScale / 100.0f, 0, 0, 1, 0, 0);
      mt.Concat(*pUser2Device);

      CPDF_RenderOptions ro;
      ro.m_Flags = RENDER_CLEARTYPE;
      ro.m_ColorMode = RENDER_COLOR_NORMAL;

      CPDF_TextRenderer::DrawTextString(pDevice, pos.x, pos.y, pFont, fFontSize,
                                        &mt, str, crTextFill, nullptr, &ro);
    } else {
      CPDF_RenderOptions ro;
      ro.m_Flags = RENDER_CLEARTYPE;
      ro.m_ColorMode = RENDER_COLOR_NORMAL;

      CPDF_TextRenderer::DrawTextString(pDevice, pos.x, pos.y, pFont, fFontSize,
                                        pUser2Device, str, crTextFill, nullptr,
                                        &ro);
    }
  }
}

}  // namespace

CFX_Edit_Iterator::CFX_Edit_Iterator(CFX_Edit* pEdit,
                                     CPDF_VariableText::Iterator* pVTIterator)
    : m_pEdit(pEdit), m_pVTIterator(pVTIterator) {}

CFX_Edit_Iterator::~CFX_Edit_Iterator() {}

bool CFX_Edit_Iterator::NextWord() {
  return m_pVTIterator->NextWord();
}

bool CFX_Edit_Iterator::PrevWord() {
  return m_pVTIterator->PrevWord();
}

bool CFX_Edit_Iterator::GetWord(CPVT_Word& word) const {
  ASSERT(m_pEdit);

  if (m_pVTIterator->GetWord(word)) {
    word.ptWord = m_pEdit->VTToEdit(word.ptWord);
    return true;
  }
  return false;
}

bool CFX_Edit_Iterator::GetLine(CPVT_Line& line) const {
  ASSERT(m_pEdit);

  if (m_pVTIterator->GetLine(line)) {
    line.ptLine = m_pEdit->VTToEdit(line.ptLine);
    return true;
  }
  return false;
}

bool CFX_Edit_Iterator::GetSection(CPVT_Section& section) const {
  ASSERT(m_pEdit);

  if (m_pVTIterator->GetSection(section)) {
    section.rcSection = m_pEdit->VTToEdit(section.rcSection);
    return true;
  }
  return false;
}

void CFX_Edit_Iterator::SetAt(int32_t nWordIndex) {
  m_pVTIterator->SetAt(nWordIndex);
}

void CFX_Edit_Iterator::SetAt(const CPVT_WordPlace& place) {
  m_pVTIterator->SetAt(place);
}

const CPVT_WordPlace& CFX_Edit_Iterator::GetAt() const {
  return m_pVTIterator->GetAt();
}

CFX_Edit_Provider::CFX_Edit_Provider(IPVT_FontMap* pFontMap)
    : CPDF_VariableText::Provider(pFontMap), m_pFontMap(pFontMap) {
  ASSERT(m_pFontMap);
}

CFX_Edit_Provider::~CFX_Edit_Provider() {}

IPVT_FontMap* CFX_Edit_Provider::GetFontMap() {
  return m_pFontMap;
}

int32_t CFX_Edit_Provider::GetCharWidth(int32_t nFontIndex, uint16_t word) {
  if (CPDF_Font* pPDFFont = m_pFontMap->GetPDFFont(nFontIndex)) {
    uint32_t charcode = word;

    if (pPDFFont->IsUnicodeCompatible())
      charcode = pPDFFont->CharCodeFromUnicode(word);
    else
      charcode = m_pFontMap->CharCodeFromUnicode(nFontIndex, word);

    if (charcode != CPDF_Font::kInvalidCharCode)
      return pPDFFont->GetCharWidthF(charcode);
  }

  return 0;
}

int32_t CFX_Edit_Provider::GetTypeAscent(int32_t nFontIndex) {
  if (CPDF_Font* pPDFFont = m_pFontMap->GetPDFFont(nFontIndex))
    return pPDFFont->GetTypeAscent();

  return 0;
}

int32_t CFX_Edit_Provider::GetTypeDescent(int32_t nFontIndex) {
  if (CPDF_Font* pPDFFont = m_pFontMap->GetPDFFont(nFontIndex))
    return pPDFFont->GetTypeDescent();

  return 0;
}

int32_t CFX_Edit_Provider::GetWordFontIndex(uint16_t word,
                                            int32_t charset,
                                            int32_t nFontIndex) {
  return m_pFontMap->GetWordFontIndex(word, charset, nFontIndex);
}

int32_t CFX_Edit_Provider::GetDefaultFontIndex() {
  return 0;
}

bool CFX_Edit_Provider::IsLatinWord(uint16_t word) {
  return FX_EDIT_ISLATINWORD(word);
}

CFX_Edit_Refresh::CFX_Edit_Refresh() {}

CFX_Edit_Refresh::~CFX_Edit_Refresh() {}

void CFX_Edit_Refresh::BeginRefresh() {
  m_RefreshRects.Clear();
  m_OldLineRects = std::move(m_NewLineRects);
}

void CFX_Edit_Refresh::Push(const CPVT_WordRange& linerange,
                            const CFX_FloatRect& rect) {
  m_NewLineRects.Add(linerange, rect);
}

void CFX_Edit_Refresh::NoAnalyse() {
  {
    for (int32_t i = 0, sz = m_OldLineRects.GetSize(); i < sz; i++)
      if (CFX_Edit_LineRect* pOldRect = m_OldLineRects.GetAt(i))
        m_RefreshRects.Add(pOldRect->m_rcLine);
  }

  {
    for (int32_t i = 0, sz = m_NewLineRects.GetSize(); i < sz; i++)
      if (CFX_Edit_LineRect* pNewRect = m_NewLineRects.GetAt(i))
        m_RefreshRects.Add(pNewRect->m_rcLine);
  }
}

void CFX_Edit_Refresh::AddRefresh(const CFX_FloatRect& rect) {
  m_RefreshRects.Add(rect);
}

const CFX_Edit_RectArray* CFX_Edit_Refresh::GetRefreshRects() const {
  return &m_RefreshRects;
}

void CFX_Edit_Refresh::EndRefresh() {
  m_RefreshRects.Clear();
}

CFX_Edit_Undo::CFX_Edit_Undo(int32_t nBufsize)
    : m_nCurUndoPos(0),
      m_nBufSize(nBufsize),
      m_bModified(false),
      m_bVirgin(true),
      m_bWorking(false) {}

CFX_Edit_Undo::~CFX_Edit_Undo() {
  Reset();
}

bool CFX_Edit_Undo::CanUndo() const {
  return m_nCurUndoPos > 0;
}

void CFX_Edit_Undo::Undo() {
  m_bWorking = true;
  if (m_nCurUndoPos > 0) {
    m_UndoItemStack[m_nCurUndoPos - 1]->Undo();
    m_nCurUndoPos--;
    m_bModified = (m_nCurUndoPos != 0);
  }
  m_bWorking = false;
}

bool CFX_Edit_Undo::CanRedo() const {
  return m_nCurUndoPos < m_UndoItemStack.size();
}

void CFX_Edit_Undo::Redo() {
  m_bWorking = true;
  if (m_nCurUndoPos < m_UndoItemStack.size()) {
    m_UndoItemStack[m_nCurUndoPos]->Redo();
    m_nCurUndoPos++;
    m_bModified = true;
  }
  m_bWorking = false;
}

void CFX_Edit_Undo::AddItem(std::unique_ptr<IFX_Edit_UndoItem> pItem) {
  ASSERT(!m_bWorking);
  ASSERT(pItem);
  ASSERT(m_nBufSize > 1);
  if (m_nCurUndoPos < m_UndoItemStack.size())
    RemoveTails();

  if (m_UndoItemStack.size() >= m_nBufSize) {
    RemoveHeads();
    m_bVirgin = false;
  }

  m_UndoItemStack.push_back(std::move(pItem));
  m_nCurUndoPos = m_UndoItemStack.size();
  m_bModified = true;
}

bool CFX_Edit_Undo::IsModified() const {
  return m_bVirgin ? m_bModified : true;
}

void CFX_Edit_Undo::RemoveHeads() {
  ASSERT(m_UndoItemStack.size() > 1);
  m_UndoItemStack.pop_front();
}

void CFX_Edit_Undo::RemoveTails() {
  while (m_UndoItemStack.size() > m_nCurUndoPos)
    m_UndoItemStack.pop_back();
}

void CFX_Edit_Undo::Reset() {
  m_UndoItemStack.clear();
  m_nCurUndoPos = 0;
}

CFX_Edit_UndoItem::CFX_Edit_UndoItem() : m_bFirst(true), m_bLast(true) {}

CFX_Edit_UndoItem::~CFX_Edit_UndoItem() {}

CFX_WideString CFX_Edit_UndoItem::GetUndoTitle() const {
  return CFX_WideString();
}

void CFX_Edit_UndoItem::SetFirst(bool bFirst) {
  m_bFirst = bFirst;
}

void CFX_Edit_UndoItem::SetLast(bool bLast) {
  m_bLast = bLast;
}

bool CFX_Edit_UndoItem::IsLast() {
  return m_bLast;
}

CFX_Edit_GroupUndoItem::CFX_Edit_GroupUndoItem(const CFX_WideString& sTitle)
    : m_sTitle(sTitle) {}

CFX_Edit_GroupUndoItem::~CFX_Edit_GroupUndoItem() {}

void CFX_Edit_GroupUndoItem::AddUndoItem(
    std::unique_ptr<CFX_Edit_UndoItem> pUndoItem) {
  pUndoItem->SetFirst(false);
  pUndoItem->SetLast(false);
  if (m_sTitle.IsEmpty())
    m_sTitle = pUndoItem->GetUndoTitle();

  m_Items.push_back(std::move(pUndoItem));
}

void CFX_Edit_GroupUndoItem::UpdateItems() {
  if (!m_Items.empty()) {
    m_Items.front()->SetFirst(true);
    m_Items.back()->SetLast(true);
  }
}

void CFX_Edit_GroupUndoItem::Undo() {
  for (auto iter = m_Items.rbegin(); iter != m_Items.rend(); ++iter)
    (*iter)->Undo();
}

void CFX_Edit_GroupUndoItem::Redo() {
  for (auto iter = m_Items.begin(); iter != m_Items.end(); ++iter)
    (*iter)->Redo();
}

CFX_WideString CFX_Edit_GroupUndoItem::GetUndoTitle() const {
  return m_sTitle;
}

CFXEU_InsertWord::CFXEU_InsertWord(CFX_Edit* pEdit,
                                   const CPVT_WordPlace& wpOldPlace,
                                   const CPVT_WordPlace& wpNewPlace,
                                   uint16_t word,
                                   int32_t charset,
                                   const CPVT_WordProps* pWordProps)
    : m_pEdit(pEdit),
      m_wpOld(wpOldPlace),
      m_wpNew(wpNewPlace),
      m_Word(word),
      m_nCharset(charset),
      m_WordProps() {
  if (pWordProps)
    m_WordProps = *pWordProps;
}

CFXEU_InsertWord::~CFXEU_InsertWord() {}

void CFXEU_InsertWord::Redo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpOld);
    m_pEdit->InsertWord(m_Word, m_nCharset, &m_WordProps, false, true);
  }
}

void CFXEU_InsertWord::Undo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpNew);
    m_pEdit->Backspace(false, true);
  }
}

CFXEU_InsertReturn::CFXEU_InsertReturn(CFX_Edit* pEdit,
                                       const CPVT_WordPlace& wpOldPlace,
                                       const CPVT_WordPlace& wpNewPlace,
                                       const CPVT_SecProps* pSecProps,
                                       const CPVT_WordProps* pWordProps)
    : m_pEdit(pEdit),
      m_wpOld(wpOldPlace),
      m_wpNew(wpNewPlace),
      m_SecProps(),
      m_WordProps() {
  if (pSecProps)
    m_SecProps = *pSecProps;
  if (pWordProps)
    m_WordProps = *pWordProps;
}

CFXEU_InsertReturn::~CFXEU_InsertReturn() {}

void CFXEU_InsertReturn::Redo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpOld);
    m_pEdit->InsertReturn(&m_SecProps, &m_WordProps, false, true);
  }
}

void CFXEU_InsertReturn::Undo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpNew);
    m_pEdit->Backspace(false, true);
  }
}

CFXEU_Backspace::CFXEU_Backspace(CFX_Edit* pEdit,
                                 const CPVT_WordPlace& wpOldPlace,
                                 const CPVT_WordPlace& wpNewPlace,
                                 uint16_t word,
                                 int32_t charset,
                                 const CPVT_SecProps& SecProps,
                                 const CPVT_WordProps& WordProps)
    : m_pEdit(pEdit),
      m_wpOld(wpOldPlace),
      m_wpNew(wpNewPlace),
      m_Word(word),
      m_nCharset(charset),
      m_SecProps(SecProps),
      m_WordProps(WordProps) {}

CFXEU_Backspace::~CFXEU_Backspace() {}

void CFXEU_Backspace::Redo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpOld);
    m_pEdit->Backspace(false, true);
  }
}

void CFXEU_Backspace::Undo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpNew);
    if (m_wpNew.SecCmp(m_wpOld) != 0) {
      m_pEdit->InsertReturn(&m_SecProps, &m_WordProps, false, true);
    } else {
      m_pEdit->InsertWord(m_Word, m_nCharset, &m_WordProps, false, true);
    }
  }
}

CFXEU_Delete::CFXEU_Delete(CFX_Edit* pEdit,
                           const CPVT_WordPlace& wpOldPlace,
                           const CPVT_WordPlace& wpNewPlace,
                           uint16_t word,
                           int32_t charset,
                           const CPVT_SecProps& SecProps,
                           const CPVT_WordProps& WordProps,
                           bool bSecEnd)
    : m_pEdit(pEdit),
      m_wpOld(wpOldPlace),
      m_wpNew(wpNewPlace),
      m_Word(word),
      m_nCharset(charset),
      m_SecProps(SecProps),
      m_WordProps(WordProps),
      m_bSecEnd(bSecEnd) {}

CFXEU_Delete::~CFXEU_Delete() {}

void CFXEU_Delete::Redo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpOld);
    m_pEdit->Delete(false, true);
  }
}

void CFXEU_Delete::Undo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpNew);
    if (m_bSecEnd) {
      m_pEdit->InsertReturn(&m_SecProps, &m_WordProps, false, true);
    } else {
      m_pEdit->InsertWord(m_Word, m_nCharset, &m_WordProps, false, true);
    }
  }
}

CFXEU_Clear::CFXEU_Clear(CFX_Edit* pEdit,
                         const CPVT_WordRange& wrSel,
                         const CFX_WideString& swText)
    : m_pEdit(pEdit), m_wrSel(wrSel), m_swText(swText) {}

CFXEU_Clear::~CFXEU_Clear() {}

void CFXEU_Clear::Redo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetSel(m_wrSel.BeginPos, m_wrSel.EndPos);
    m_pEdit->Clear(false, true);
  }
}

void CFXEU_Clear::Undo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wrSel.BeginPos);
    m_pEdit->InsertText(m_swText, FXFONT_DEFAULT_CHARSET, false, true);
    m_pEdit->SetSel(m_wrSel.BeginPos, m_wrSel.EndPos);
  }
}

CFXEU_InsertText::CFXEU_InsertText(CFX_Edit* pEdit,
                                   const CPVT_WordPlace& wpOldPlace,
                                   const CPVT_WordPlace& wpNewPlace,
                                   const CFX_WideString& swText,
                                   int32_t charset)
    : m_pEdit(pEdit),
      m_wpOld(wpOldPlace),
      m_wpNew(wpNewPlace),
      m_swText(swText),
      m_nCharset(charset) {}

CFXEU_InsertText::~CFXEU_InsertText() {}

void CFXEU_InsertText::Redo() {
  if (m_pEdit && IsLast()) {
    m_pEdit->SelectNone();
    m_pEdit->SetCaret(m_wpOld);
    m_pEdit->InsertText(m_swText, m_nCharset, false, true);
  }
}

void CFXEU_InsertText::Undo() {
  if (m_pEdit) {
    m_pEdit->SelectNone();
    m_pEdit->SetSel(m_wpOld, m_wpNew);
    m_pEdit->Clear(false, true);
  }
}

// static
CFX_ByteString CFX_Edit::GetEditAppearanceStream(CFX_Edit* pEdit,
                                                 const CFX_PointF& ptOffset,
                                                 const CPVT_WordRange* pRange,
                                                 bool bContinuous,
                                                 uint16_t SubWord) {
  CFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (pRange)
    pIterator->SetAt(pRange->BeginPos);
  else
    pIterator->SetAt(0);

  CFX_ByteTextBuf sEditStream;
  CFX_ByteTextBuf sWords;
  int32_t nCurFontIndex = -1;
  CFX_PointF ptOld;
  CFX_PointF ptNew;
  CPVT_WordPlace oldplace;

  while (pIterator->NextWord()) {
    CPVT_WordPlace place = pIterator->GetAt();
    if (pRange && place.WordCmp(pRange->EndPos) > 0)
      break;

    if (bContinuous) {
      if (place.LineCmp(oldplace) != 0) {
        if (sWords.GetSize() > 0) {
          sEditStream << GetWordRenderString(sWords.MakeString());
          sWords.Clear();
        }

        CPVT_Word word;
        if (pIterator->GetWord(word)) {
          ptNew = CFX_PointF(word.ptWord.x + ptOffset.x,
                             word.ptWord.y + ptOffset.y);
        } else {
          CPVT_Line line;
          pIterator->GetLine(line);
          ptNew = CFX_PointF(line.ptLine.x + ptOffset.x,
                             line.ptLine.y + ptOffset.y);
        }

        if (ptNew.x != ptOld.x || ptNew.y != ptOld.y) {
          sEditStream << ptNew.x - ptOld.x << " " << ptNew.y - ptOld.y
                      << " Td\n";

          ptOld = ptNew;
        }
      }

      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        if (word.nFontIndex != nCurFontIndex) {
          if (sWords.GetSize() > 0) {
            sEditStream << GetWordRenderString(sWords.MakeString());
            sWords.Clear();
          }
          sEditStream << GetFontSetString(pEdit->GetFontMap(), word.nFontIndex,
                                          word.fFontSize);
          nCurFontIndex = word.nFontIndex;
        }

        sWords << GetPDFWordString(pEdit->GetFontMap(), nCurFontIndex,
                                   word.Word, SubWord);
      }

      oldplace = place;
    } else {
      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        ptNew =
            CFX_PointF(word.ptWord.x + ptOffset.x, word.ptWord.y + ptOffset.y);

        if (ptNew.x != ptOld.x || ptNew.y != ptOld.y) {
          sEditStream << ptNew.x - ptOld.x << " " << ptNew.y - ptOld.y
                      << " Td\n";
          ptOld = ptNew;
        }

        if (word.nFontIndex != nCurFontIndex) {
          sEditStream << GetFontSetString(pEdit->GetFontMap(), word.nFontIndex,
                                          word.fFontSize);
          nCurFontIndex = word.nFontIndex;
        }

        sEditStream << GetWordRenderString(GetPDFWordString(
            pEdit->GetFontMap(), nCurFontIndex, word.Word, SubWord));
      }
    }
  }

  if (sWords.GetSize() > 0) {
    sEditStream << GetWordRenderString(sWords.MakeString());
    sWords.Clear();
  }

  CFX_ByteTextBuf sAppStream;
  if (sEditStream.GetSize() > 0) {
    int32_t nHorzScale = pEdit->GetHorzScale();
    if (nHorzScale != 100) {
      sAppStream << nHorzScale << " Tz\n";
    }

    FX_FLOAT fCharSpace = pEdit->GetCharSpace();
    if (!IsFloatZero(fCharSpace)) {
      sAppStream << fCharSpace << " Tc\n";
    }

    sAppStream << sEditStream;
  }

  return sAppStream.MakeString();
}

// static
CFX_ByteString CFX_Edit::GetSelectAppearanceStream(
    CFX_Edit* pEdit,
    const CFX_PointF& ptOffset,
    const CPVT_WordRange* pRange) {
  if (!pRange || !pRange->IsExist())
    return CFX_ByteString();

  CFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  pIterator->SetAt(pRange->BeginPos);

  CFX_ByteTextBuf sRet;
  while (pIterator->NextWord()) {
    CPVT_WordPlace place = pIterator->GetAt();
    if (place.WordCmp(pRange->EndPos) > 0)
      break;

    CPVT_Word word;
    CPVT_Line line;
    if (pIterator->GetWord(word) && pIterator->GetLine(line)) {
      sRet << word.ptWord.x + ptOffset.x << " "
           << line.ptLine.y + line.fLineDescent << " " << word.fWidth << " "
           << line.fLineAscent - line.fLineDescent << " re\nf\n";
    }
  }

  return sRet.MakeString();
}

// static
void CFX_Edit::DrawEdit(CFX_RenderDevice* pDevice,
                        CFX_Matrix* pUser2Device,
                        CFX_Edit* pEdit,
                        FX_COLORREF crTextFill,
                        const CFX_FloatRect& rcClip,
                        const CFX_PointF& ptOffset,
                        const CPVT_WordRange* pRange,
                        CFX_SystemHandler* pSystemHandler,
                        CFFL_FormFiller* pFFLData) {
  const bool bContinuous =
      pEdit->GetCharArray() == 0 && pEdit->GetCharSpace() <= 0.0f;
  uint16_t SubWord = pEdit->GetPasswordChar();
  FX_FLOAT fFontSize = pEdit->GetFontSize();
  CPVT_WordRange wrSelect = pEdit->GetSelectWordRange();
  int32_t nHorzScale = pEdit->GetHorzScale();

  FX_COLORREF crCurFill = crTextFill;
  FX_COLORREF crOldFill = crCurFill;

  bool bSelect = false;
  const FX_COLORREF crWhite = ArgbEncode(255, 255, 255, 255);
  const FX_COLORREF crSelBK = ArgbEncode(255, 0, 51, 113);

  CFX_ByteTextBuf sTextBuf;
  int32_t nFontIndex = -1;
  CFX_PointF ptBT;
  pDevice->SaveState();
  if (!rcClip.IsEmpty()) {
    CFX_FloatRect rcTemp = rcClip;
    pUser2Device->TransformRect(rcTemp);
    pDevice->SetClip_Rect(rcTemp.ToFxRect());
  }

  CFX_Edit_Iterator* pIterator = pEdit->GetIterator();
  if (IPVT_FontMap* pFontMap = pEdit->GetFontMap()) {
    if (pRange)
      pIterator->SetAt(pRange->BeginPos);
    else
      pIterator->SetAt(0);

    CPVT_WordPlace oldplace;
    while (pIterator->NextWord()) {
      CPVT_WordPlace place = pIterator->GetAt();
      if (pRange && place.WordCmp(pRange->EndPos) > 0)
        break;

      if (wrSelect.IsExist()) {
        bSelect = place.WordCmp(wrSelect.BeginPos) > 0 &&
                  place.WordCmp(wrSelect.EndPos) <= 0;
        crCurFill = bSelect ? crWhite : crTextFill;
      }
      if (pSystemHandler && pSystemHandler->IsSelectionImplemented()) {
        crCurFill = crTextFill;
        crOldFill = crCurFill;
      }
      CPVT_Word word;
      if (pIterator->GetWord(word)) {
        if (bSelect) {
          CPVT_Line line;
          pIterator->GetLine(line);

          if (pSystemHandler && pSystemHandler->IsSelectionImplemented()) {
            CFX_FloatRect rc(word.ptWord.x, line.ptLine.y + line.fLineDescent,
                             word.ptWord.x + word.fWidth,
                             line.ptLine.y + line.fLineAscent);
            rc.Intersect(rcClip);
            pSystemHandler->OutputSelectedRect(pFFLData, rc);
          } else {
            CFX_PathData pathSelBK;
            pathSelBK.AppendRect(
                word.ptWord.x, line.ptLine.y + line.fLineDescent,
                word.ptWord.x + word.fWidth, line.ptLine.y + line.fLineAscent);

            pDevice->DrawPath(&pathSelBK, pUser2Device, nullptr, crSelBK, 0,
                              FXFILL_WINDING);
          }
        }

        if (bContinuous) {
          if (place.LineCmp(oldplace) != 0 || word.nFontIndex != nFontIndex ||
              crOldFill != crCurFill) {
            if (sTextBuf.GetLength() > 0) {
              DrawTextString(
                  pDevice, CFX_PointF(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
                  pFontMap->GetPDFFont(nFontIndex), fFontSize, pUser2Device,
                  sTextBuf.MakeString(), crOldFill, nHorzScale);

              sTextBuf.Clear();
            }
            nFontIndex = word.nFontIndex;
            ptBT = word.ptWord;
            crOldFill = crCurFill;
          }

          sTextBuf << GetPDFWordString(pFontMap, word.nFontIndex, word.Word,
                                       SubWord)
                          .AsStringC();
        } else {
          DrawTextString(
              pDevice, CFX_PointF(word.ptWord.x + ptOffset.x,
                                  word.ptWord.y + ptOffset.y),
              pFontMap->GetPDFFont(word.nFontIndex), fFontSize, pUser2Device,
              GetPDFWordString(pFontMap, word.nFontIndex, word.Word, SubWord),
              crCurFill, nHorzScale);
        }
        oldplace = place;
      }
    }

    if (sTextBuf.GetLength() > 0) {
      DrawTextString(pDevice,
                     CFX_PointF(ptBT.x + ptOffset.x, ptBT.y + ptOffset.y),
                     pFontMap->GetPDFFont(nFontIndex), fFontSize, pUser2Device,
                     sTextBuf.MakeString(), crOldFill, nHorzScale);
    }
  }

  pDevice->RestoreState(false);
}

CFX_Edit::CFX_Edit()
    : m_pVT(new CPDF_VariableText),
      m_pNotify(nullptr),
      m_pOprNotify(nullptr),
      m_wpCaret(-1, -1, -1),
      m_wpOldCaret(-1, -1, -1),
      m_SelState(),
      m_bEnableScroll(false),
      m_Undo(kEditUndoMaxItems),
      m_nAlignment(0),
      m_bNotifyFlag(false),
      m_bEnableOverflow(false),
      m_bEnableRefresh(true),
      m_rcOldContent(0.0f, 0.0f, 0.0f, 0.0f),
      m_bEnableUndo(true),
      m_bOprNotify(false),
      m_pGroupUndoItem(nullptr) {}

CFX_Edit::~CFX_Edit() {
  ASSERT(!m_pGroupUndoItem);
}

void CFX_Edit::Initialize() {
  m_pVT->Initialize();
  SetCaret(m_pVT->GetBeginWordPlace());
  SetCaretOrigin();
}

void CFX_Edit::SetFontMap(IPVT_FontMap* pFontMap) {
  m_pVTProvider = pdfium::MakeUnique<CFX_Edit_Provider>(pFontMap);
  m_pVT->SetProvider(m_pVTProvider.get());
}

void CFX_Edit::SetNotify(CPWL_EditCtrl* pNotify) {
  m_pNotify = pNotify;
}

void CFX_Edit::SetOprNotify(CPWL_Edit* pOprNotify) {
  m_pOprNotify = pOprNotify;
}

CFX_Edit_Iterator* CFX_Edit::GetIterator() {
  if (!m_pIterator) {
    m_pIterator =
        pdfium::MakeUnique<CFX_Edit_Iterator>(this, m_pVT->GetIterator());
  }
  return m_pIterator.get();
}

IPVT_FontMap* CFX_Edit::GetFontMap() {
  return m_pVTProvider ? m_pVTProvider->GetFontMap() : nullptr;
}

void CFX_Edit::SetPlateRect(const CFX_FloatRect& rect) {
  m_pVT->SetPlateRect(rect);
  m_ptScrollPos = CFX_PointF(rect.left, rect.top);
  Paint();
}

void CFX_Edit::SetAlignmentH(int32_t nFormat, bool bPaint) {
  m_pVT->SetAlignment(nFormat);
  if (bPaint)
    Paint();
}

void CFX_Edit::SetAlignmentV(int32_t nFormat, bool bPaint) {
  m_nAlignment = nFormat;
  if (bPaint)
    Paint();
}

void CFX_Edit::SetPasswordChar(uint16_t wSubWord, bool bPaint) {
  m_pVT->SetPasswordChar(wSubWord);
  if (bPaint)
    Paint();
}

void CFX_Edit::SetLimitChar(int32_t nLimitChar) {
  m_pVT->SetLimitChar(nLimitChar);
  Paint();
}

void CFX_Edit::SetCharArray(int32_t nCharArray) {
  m_pVT->SetCharArray(nCharArray);
  Paint();
}

void CFX_Edit::SetCharSpace(FX_FLOAT fCharSpace) {
  m_pVT->SetCharSpace(fCharSpace);
  Paint();
}

void CFX_Edit::SetMultiLine(bool bMultiLine, bool bPaint) {
  m_pVT->SetMultiLine(bMultiLine);
  if (bPaint)
    Paint();
}

void CFX_Edit::SetAutoReturn(bool bAuto, bool bPaint) {
  m_pVT->SetAutoReturn(bAuto);
  if (bPaint)
    Paint();
}

void CFX_Edit::SetAutoFontSize(bool bAuto, bool bPaint) {
  m_pVT->SetAutoFontSize(bAuto);
  if (bPaint)
    Paint();
}

void CFX_Edit::SetFontSize(FX_FLOAT fFontSize) {
  m_pVT->SetFontSize(fFontSize);
  Paint();
}

void CFX_Edit::SetAutoScroll(bool bAuto, bool bPaint) {
  m_bEnableScroll = bAuto;
  if (bPaint)
    Paint();
}

void CFX_Edit::SetTextOverflow(bool bAllowed, bool bPaint) {
  m_bEnableOverflow = bAllowed;
  if (bPaint)
    Paint();
}

void CFX_Edit::SetSel(int32_t nStartChar, int32_t nEndChar) {
  if (m_pVT->IsValid()) {
    if (nStartChar == 0 && nEndChar < 0) {
      SelectAll();
    } else if (nStartChar < 0) {
      SelectNone();
    } else {
      if (nStartChar < nEndChar) {
        SetSel(m_pVT->WordIndexToWordPlace(nStartChar),
               m_pVT->WordIndexToWordPlace(nEndChar));
      } else {
        SetSel(m_pVT->WordIndexToWordPlace(nEndChar),
               m_pVT->WordIndexToWordPlace(nStartChar));
      }
    }
  }
}

void CFX_Edit::SetSel(const CPVT_WordPlace& begin, const CPVT_WordPlace& end) {
  if (m_pVT->IsValid()) {
    SelectNone();

    m_SelState.Set(begin, end);

    SetCaret(m_SelState.EndPos);

    if (m_SelState.IsExist()) {
      ScrollToCaret();
      Refresh();
      SetCaretInfo();
    } else {
      ScrollToCaret();
      SetCaretInfo();
    }
  }
}

void CFX_Edit::GetSel(int32_t& nStartChar, int32_t& nEndChar) const {
  nStartChar = -1;
  nEndChar = -1;

  if (m_pVT->IsValid()) {
    if (m_SelState.IsExist()) {
      if (m_SelState.BeginPos.WordCmp(m_SelState.EndPos) < 0) {
        nStartChar = m_pVT->WordPlaceToWordIndex(m_SelState.BeginPos);
        nEndChar = m_pVT->WordPlaceToWordIndex(m_SelState.EndPos);
      } else {
        nStartChar = m_pVT->WordPlaceToWordIndex(m_SelState.EndPos);
        nEndChar = m_pVT->WordPlaceToWordIndex(m_SelState.BeginPos);
      }
    } else {
      nStartChar = m_pVT->WordPlaceToWordIndex(m_wpCaret);
      nEndChar = m_pVT->WordPlaceToWordIndex(m_wpCaret);
    }
  }
}

int32_t CFX_Edit::GetCaret() const {
  if (m_pVT->IsValid())
    return m_pVT->WordPlaceToWordIndex(m_wpCaret);

  return -1;
}

CPVT_WordPlace CFX_Edit::GetCaretWordPlace() const {
  return m_wpCaret;
}

CFX_WideString CFX_Edit::GetText() const {
  CFX_WideString swRet;

  if (!m_pVT->IsValid())
    return swRet;

  CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
  pIterator->SetAt(0);

  CPVT_Word wordinfo;
  CPVT_WordPlace oldplace = pIterator->GetAt();
  while (pIterator->NextWord()) {
    CPVT_WordPlace place = pIterator->GetAt();

    if (pIterator->GetWord(wordinfo))
      swRet += wordinfo.Word;

    if (oldplace.SecCmp(place) != 0)
      swRet += L"\r\n";

    oldplace = place;
  }

  return swRet;
}

CFX_WideString CFX_Edit::GetRangeText(const CPVT_WordRange& range) const {
  CFX_WideString swRet;

  if (!m_pVT->IsValid())
    return swRet;

  CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
  CPVT_WordRange wrTemp = range;
  m_pVT->UpdateWordPlace(wrTemp.BeginPos);
  m_pVT->UpdateWordPlace(wrTemp.EndPos);
  pIterator->SetAt(wrTemp.BeginPos);

  CPVT_Word wordinfo;
  CPVT_WordPlace oldplace = wrTemp.BeginPos;
  while (pIterator->NextWord()) {
    CPVT_WordPlace place = pIterator->GetAt();
    if (place.WordCmp(wrTemp.EndPos) > 0)
      break;

    if (pIterator->GetWord(wordinfo))
      swRet += wordinfo.Word;

    if (oldplace.SecCmp(place) != 0)
      swRet += L"\r\n";

    oldplace = place;
  }

  return swRet;
}

CFX_WideString CFX_Edit::GetSelText() const {
  return GetRangeText(m_SelState.ConvertToWordRange());
}

int32_t CFX_Edit::GetTotalWords() const {
  return m_pVT->GetTotalWords();
}

int32_t CFX_Edit::GetTotalLines() const {
  int32_t nLines = 1;

  CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
  pIterator->SetAt(0);
  while (pIterator->NextLine())
    ++nLines;

  return nLines;
}

CPVT_WordRange CFX_Edit::GetSelectWordRange() const {
  return m_SelState.ConvertToWordRange();
}

void CFX_Edit::SetText(const CFX_WideString& sText) {
  Empty();
  DoInsertText(CPVT_WordPlace(0, 0, -1), sText, FXFONT_DEFAULT_CHARSET);
  Paint();
}

bool CFX_Edit::InsertWord(uint16_t word, int32_t charset) {
  return InsertWord(word, charset, nullptr, true, true);
}

bool CFX_Edit::InsertReturn() {
  return InsertReturn(nullptr, nullptr, true, true);
}

bool CFX_Edit::Backspace() {
  return Backspace(true, true);
}

bool CFX_Edit::Delete() {
  return Delete(true, true);
}

bool CFX_Edit::Clear() {
  return Clear(true, true);
}

bool CFX_Edit::InsertText(const CFX_WideString& sText, int32_t charset) {
  return InsertText(sText, charset, true, true);
}

FX_FLOAT CFX_Edit::GetFontSize() const {
  return m_pVT->GetFontSize();
}

uint16_t CFX_Edit::GetPasswordChar() const {
  return m_pVT->GetPasswordChar();
}

int32_t CFX_Edit::GetCharArray() const {
  return m_pVT->GetCharArray();
}

CFX_FloatRect CFX_Edit::GetContentRect() const {
  return VTToEdit(m_pVT->GetContentRect());
}

int32_t CFX_Edit::GetHorzScale() const {
  return m_pVT->GetHorzScale();
}

FX_FLOAT CFX_Edit::GetCharSpace() const {
  return m_pVT->GetCharSpace();
}

CPVT_WordRange CFX_Edit::GetWholeWordRange() const {
  if (m_pVT->IsValid())
    return CPVT_WordRange(m_pVT->GetBeginWordPlace(), m_pVT->GetEndWordPlace());

  return CPVT_WordRange();
}

CPVT_WordRange CFX_Edit::GetVisibleWordRange() const {
  if (m_bEnableOverflow)
    return GetWholeWordRange();

  if (m_pVT->IsValid()) {
    CFX_FloatRect rcPlate = m_pVT->GetPlateRect();

    CPVT_WordPlace place1 =
        m_pVT->SearchWordPlace(EditToVT(CFX_PointF(rcPlate.left, rcPlate.top)));
    CPVT_WordPlace place2 = m_pVT->SearchWordPlace(
        EditToVT(CFX_PointF(rcPlate.right, rcPlate.bottom)));

    return CPVT_WordRange(place1, place2);
  }

  return CPVT_WordRange();
}

CPVT_WordPlace CFX_Edit::SearchWordPlace(const CFX_PointF& point) const {
  if (m_pVT->IsValid()) {
    return m_pVT->SearchWordPlace(EditToVT(point));
  }

  return CPVT_WordPlace();
}

void CFX_Edit::Paint() {
  if (m_pVT->IsValid()) {
    RearrangeAll();
    ScrollToCaret();
    Refresh();
    SetCaretOrigin();
    SetCaretInfo();
  }
}

void CFX_Edit::RearrangeAll() {
  if (m_pVT->IsValid()) {
    m_pVT->UpdateWordPlace(m_wpCaret);
    m_pVT->RearrangeAll();
    m_pVT->UpdateWordPlace(m_wpCaret);
    SetScrollInfo();
    SetContentChanged();
  }
}

void CFX_Edit::RearrangePart(const CPVT_WordRange& range) {
  if (m_pVT->IsValid()) {
    m_pVT->UpdateWordPlace(m_wpCaret);
    m_pVT->RearrangePart(range);
    m_pVT->UpdateWordPlace(m_wpCaret);
    SetScrollInfo();
    SetContentChanged();
  }
}

void CFX_Edit::SetContentChanged() {
  if (m_pNotify) {
    CFX_FloatRect rcContent = m_pVT->GetContentRect();
    if (rcContent.Width() != m_rcOldContent.Width() ||
        rcContent.Height() != m_rcOldContent.Height()) {
      if (!m_bNotifyFlag) {
        m_bNotifyFlag = true;
        m_pNotify->IOnContentChange(rcContent);
        m_bNotifyFlag = false;
      }
      m_rcOldContent = rcContent;
    }
  }
}

void CFX_Edit::SelectAll() {
  if (m_pVT->IsValid()) {
    m_SelState = CFX_Edit_Select(GetWholeWordRange());
    SetCaret(m_SelState.EndPos);

    ScrollToCaret();
    Refresh();
    SetCaretInfo();
  }
}

void CFX_Edit::SelectNone() {
  if (m_pVT->IsValid()) {
    if (m_SelState.IsExist()) {
      m_SelState.Default();
      Refresh();
    }
  }
}

bool CFX_Edit::IsSelected() const {
  return m_SelState.IsExist();
}

CFX_PointF CFX_Edit::VTToEdit(const CFX_PointF& point) const {
  CFX_FloatRect rcContent = m_pVT->GetContentRect();
  CFX_FloatRect rcPlate = m_pVT->GetPlateRect();

  FX_FLOAT fPadding = 0.0f;

  switch (m_nAlignment) {
    case 0:
      fPadding = 0.0f;
      break;
    case 1:
      fPadding = (rcPlate.Height() - rcContent.Height()) * 0.5f;
      break;
    case 2:
      fPadding = rcPlate.Height() - rcContent.Height();
      break;
  }

  return CFX_PointF(point.x - (m_ptScrollPos.x - rcPlate.left),
                    point.y - (m_ptScrollPos.y + fPadding - rcPlate.top));
}

CFX_PointF CFX_Edit::EditToVT(const CFX_PointF& point) const {
  CFX_FloatRect rcContent = m_pVT->GetContentRect();
  CFX_FloatRect rcPlate = m_pVT->GetPlateRect();

  FX_FLOAT fPadding = 0.0f;

  switch (m_nAlignment) {
    case 0:
      fPadding = 0.0f;
      break;
    case 1:
      fPadding = (rcPlate.Height() - rcContent.Height()) * 0.5f;
      break;
    case 2:
      fPadding = rcPlate.Height() - rcContent.Height();
      break;
  }

  return CFX_PointF(point.x + (m_ptScrollPos.x - rcPlate.left),
                    point.y + (m_ptScrollPos.y + fPadding - rcPlate.top));
}

CFX_FloatRect CFX_Edit::VTToEdit(const CFX_FloatRect& rect) const {
  CFX_PointF ptLeftBottom = VTToEdit(CFX_PointF(rect.left, rect.bottom));
  CFX_PointF ptRightTop = VTToEdit(CFX_PointF(rect.right, rect.top));

  return CFX_FloatRect(ptLeftBottom.x, ptLeftBottom.y, ptRightTop.x,
                       ptRightTop.y);
}

void CFX_Edit::SetScrollInfo() {
  if (m_pNotify) {
    CFX_FloatRect rcPlate = m_pVT->GetPlateRect();
    CFX_FloatRect rcContent = m_pVT->GetContentRect();

    if (!m_bNotifyFlag) {
      m_bNotifyFlag = true;
      m_pNotify->IOnSetScrollInfoY(rcPlate.bottom, rcPlate.top,
                                   rcContent.bottom, rcContent.top,
                                   rcPlate.Height() / 3, rcPlate.Height());
      m_bNotifyFlag = false;
    }
  }
}

void CFX_Edit::SetScrollPosX(FX_FLOAT fx) {
  if (!m_bEnableScroll)
    return;

  if (m_pVT->IsValid()) {
    if (!IsFloatEqual(m_ptScrollPos.x, fx)) {
      m_ptScrollPos.x = fx;
      Refresh();
    }
  }
}

void CFX_Edit::SetScrollPosY(FX_FLOAT fy) {
  if (!m_bEnableScroll)
    return;

  if (m_pVT->IsValid()) {
    if (!IsFloatEqual(m_ptScrollPos.y, fy)) {
      m_ptScrollPos.y = fy;
      Refresh();

      if (m_pNotify) {
        if (!m_bNotifyFlag) {
          m_bNotifyFlag = true;
          m_pNotify->IOnSetScrollPosY(fy);
          m_bNotifyFlag = false;
        }
      }
    }
  }
}

void CFX_Edit::SetScrollPos(const CFX_PointF& point) {
  SetScrollPosX(point.x);
  SetScrollPosY(point.y);
  SetScrollLimit();
  SetCaretInfo();
}

CFX_PointF CFX_Edit::GetScrollPos() const {
  return m_ptScrollPos;
}

void CFX_Edit::SetScrollLimit() {
  if (m_pVT->IsValid()) {
    CFX_FloatRect rcContent = m_pVT->GetContentRect();
    CFX_FloatRect rcPlate = m_pVT->GetPlateRect();

    if (rcPlate.Width() > rcContent.Width()) {
      SetScrollPosX(rcPlate.left);
    } else {
      if (IsFloatSmaller(m_ptScrollPos.x, rcContent.left)) {
        SetScrollPosX(rcContent.left);
      } else if (IsFloatBigger(m_ptScrollPos.x,
                               rcContent.right - rcPlate.Width())) {
        SetScrollPosX(rcContent.right - rcPlate.Width());
      }
    }

    if (rcPlate.Height() > rcContent.Height()) {
      SetScrollPosY(rcPlate.top);
    } else {
      if (IsFloatSmaller(m_ptScrollPos.y,
                         rcContent.bottom + rcPlate.Height())) {
        SetScrollPosY(rcContent.bottom + rcPlate.Height());
      } else if (IsFloatBigger(m_ptScrollPos.y, rcContent.top)) {
        SetScrollPosY(rcContent.top);
      }
    }
  }
}

void CFX_Edit::ScrollToCaret() {
  SetScrollLimit();

  if (!m_pVT->IsValid())
    return;

  CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
  pIterator->SetAt(m_wpCaret);

  CFX_PointF ptHead;
  CFX_PointF ptFoot;
  CPVT_Word word;
  CPVT_Line line;
  if (pIterator->GetWord(word)) {
    ptHead.x = word.ptWord.x + word.fWidth;
    ptHead.y = word.ptWord.y + word.fAscent;
    ptFoot.x = word.ptWord.x + word.fWidth;
    ptFoot.y = word.ptWord.y + word.fDescent;
  } else if (pIterator->GetLine(line)) {
    ptHead.x = line.ptLine.x;
    ptHead.y = line.ptLine.y + line.fLineAscent;
    ptFoot.x = line.ptLine.x;
    ptFoot.y = line.ptLine.y + line.fLineDescent;
  }

  CFX_PointF ptHeadEdit = VTToEdit(ptHead);
  CFX_PointF ptFootEdit = VTToEdit(ptFoot);
  CFX_FloatRect rcPlate = m_pVT->GetPlateRect();
  if (!IsFloatEqual(rcPlate.left, rcPlate.right)) {
    if (IsFloatSmaller(ptHeadEdit.x, rcPlate.left) ||
        IsFloatEqual(ptHeadEdit.x, rcPlate.left)) {
      SetScrollPosX(ptHead.x);
    } else if (IsFloatBigger(ptHeadEdit.x, rcPlate.right)) {
      SetScrollPosX(ptHead.x - rcPlate.Width());
    }
  }

  if (!IsFloatEqual(rcPlate.top, rcPlate.bottom)) {
    if (IsFloatSmaller(ptFootEdit.y, rcPlate.bottom) ||
        IsFloatEqual(ptFootEdit.y, rcPlate.bottom)) {
      if (IsFloatSmaller(ptHeadEdit.y, rcPlate.top)) {
        SetScrollPosY(ptFoot.y + rcPlate.Height());
      }
    } else if (IsFloatBigger(ptHeadEdit.y, rcPlate.top)) {
      if (IsFloatBigger(ptFootEdit.y, rcPlate.bottom)) {
        SetScrollPosY(ptHead.y);
      }
    }
  }
}

void CFX_Edit::Refresh() {
  if (m_bEnableRefresh && m_pVT->IsValid()) {
    m_Refresh.BeginRefresh();
    RefreshPushLineRects(GetVisibleWordRange());

    m_Refresh.NoAnalyse();
    m_ptRefreshScrollPos = m_ptScrollPos;

    if (m_pNotify) {
      if (!m_bNotifyFlag) {
        m_bNotifyFlag = true;
        if (const CFX_Edit_RectArray* pRects = m_Refresh.GetRefreshRects()) {
          for (int32_t i = 0, sz = pRects->GetSize(); i < sz; i++)
            m_pNotify->IOnInvalidateRect(pRects->GetAt(i));
        }
        m_bNotifyFlag = false;
      }
    }

    m_Refresh.EndRefresh();
  }
}

void CFX_Edit::RefreshPushLineRects(const CPVT_WordRange& wr) {
  if (!m_pVT->IsValid())
    return;

  CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
  CPVT_WordPlace wpBegin = wr.BeginPos;
  m_pVT->UpdateWordPlace(wpBegin);
  CPVT_WordPlace wpEnd = wr.EndPos;
  m_pVT->UpdateWordPlace(wpEnd);
  pIterator->SetAt(wpBegin);

  CPVT_Line lineinfo;
  do {
    if (!pIterator->GetLine(lineinfo))
      break;
    if (lineinfo.lineplace.LineCmp(wpEnd) > 0)
      break;

    CFX_FloatRect rcLine(lineinfo.ptLine.x,
                         lineinfo.ptLine.y + lineinfo.fLineDescent,
                         lineinfo.ptLine.x + lineinfo.fLineWidth,
                         lineinfo.ptLine.y + lineinfo.fLineAscent);

    m_Refresh.Push(CPVT_WordRange(lineinfo.lineplace, lineinfo.lineEnd),
                   VTToEdit(rcLine));
  } while (pIterator->NextLine());
}

void CFX_Edit::RefreshWordRange(const CPVT_WordRange& wr) {
  CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
  CPVT_WordRange wrTemp = wr;

  m_pVT->UpdateWordPlace(wrTemp.BeginPos);
  m_pVT->UpdateWordPlace(wrTemp.EndPos);
  pIterator->SetAt(wrTemp.BeginPos);

  CPVT_Word wordinfo;
  CPVT_Line lineinfo;
  CPVT_WordPlace place;

  while (pIterator->NextWord()) {
    place = pIterator->GetAt();
    if (place.WordCmp(wrTemp.EndPos) > 0)
      break;

    pIterator->GetWord(wordinfo);
    pIterator->GetLine(lineinfo);

    if (place.LineCmp(wrTemp.BeginPos) == 0 ||
        place.LineCmp(wrTemp.EndPos) == 0) {
      CFX_FloatRect rcWord(wordinfo.ptWord.x,
                           lineinfo.ptLine.y + lineinfo.fLineDescent,
                           wordinfo.ptWord.x + wordinfo.fWidth,
                           lineinfo.ptLine.y + lineinfo.fLineAscent);

      if (m_pNotify) {
        if (!m_bNotifyFlag) {
          m_bNotifyFlag = true;
          CFX_FloatRect rcRefresh = VTToEdit(rcWord);
          m_pNotify->IOnInvalidateRect(&rcRefresh);
          m_bNotifyFlag = false;
        }
      }
    } else {
      CFX_FloatRect rcLine(lineinfo.ptLine.x,
                           lineinfo.ptLine.y + lineinfo.fLineDescent,
                           lineinfo.ptLine.x + lineinfo.fLineWidth,
                           lineinfo.ptLine.y + lineinfo.fLineAscent);

      if (m_pNotify) {
        if (!m_bNotifyFlag) {
          m_bNotifyFlag = true;
          CFX_FloatRect rcRefresh = VTToEdit(rcLine);
          m_pNotify->IOnInvalidateRect(&rcRefresh);
          m_bNotifyFlag = false;
        }
      }

      pIterator->NextLine();
    }
  }
}

void CFX_Edit::SetCaret(const CPVT_WordPlace& place) {
  m_wpOldCaret = m_wpCaret;
  m_wpCaret = place;
}

void CFX_Edit::SetCaretInfo() {
  if (m_pNotify) {
    if (!m_bNotifyFlag) {
      CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
      pIterator->SetAt(m_wpCaret);

      CFX_PointF ptHead;
      CFX_PointF ptFoot;
      CPVT_Word word;
      CPVT_Line line;
      if (pIterator->GetWord(word)) {
        ptHead.x = word.ptWord.x + word.fWidth;
        ptHead.y = word.ptWord.y + word.fAscent;
        ptFoot.x = word.ptWord.x + word.fWidth;
        ptFoot.y = word.ptWord.y + word.fDescent;
      } else if (pIterator->GetLine(line)) {
        ptHead.x = line.ptLine.x;
        ptHead.y = line.ptLine.y + line.fLineAscent;
        ptFoot.x = line.ptLine.x;
        ptFoot.y = line.ptLine.y + line.fLineDescent;
      }

      m_bNotifyFlag = true;
      m_pNotify->IOnSetCaret(!m_SelState.IsExist(), VTToEdit(ptHead),
                             VTToEdit(ptFoot), m_wpCaret);
      m_bNotifyFlag = false;
    }
  }
}

void CFX_Edit::SetCaret(int32_t nPos) {
  if (m_pVT->IsValid()) {
    SelectNone();
    SetCaret(m_pVT->WordIndexToWordPlace(nPos));
    m_SelState.Set(m_wpCaret, m_wpCaret);

    ScrollToCaret();
    SetCaretOrigin();
    SetCaretInfo();
  }
}

void CFX_Edit::OnMouseDown(const CFX_PointF& point, bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    SelectNone();
    SetCaret(m_pVT->SearchWordPlace(EditToVT(point)));
    m_SelState.Set(m_wpCaret, m_wpCaret);

    ScrollToCaret();
    SetCaretOrigin();
    SetCaretInfo();
  }
}

void CFX_Edit::OnMouseMove(const CFX_PointF& point, bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    SetCaret(m_pVT->SearchWordPlace(EditToVT(point)));

    if (m_wpCaret != m_wpOldCaret) {
      m_SelState.SetEndPos(m_wpCaret);

      ScrollToCaret();
      Refresh();
      SetCaretOrigin();
      SetCaretInfo();
    }
  }
}

void CFX_Edit::OnVK_UP(bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    SetCaret(m_pVT->GetUpWordPlace(m_wpCaret, m_ptCaret));

    if (bShift) {
      if (m_SelState.IsExist())
        m_SelState.SetEndPos(m_wpCaret);
      else
        m_SelState.Set(m_wpOldCaret, m_wpCaret);

      if (m_wpOldCaret != m_wpCaret) {
        ScrollToCaret();
        Refresh();
        SetCaretInfo();
      }
    } else {
      SelectNone();

      ScrollToCaret();
      SetCaretInfo();
    }
  }
}

void CFX_Edit::OnVK_DOWN(bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    SetCaret(m_pVT->GetDownWordPlace(m_wpCaret, m_ptCaret));

    if (bShift) {
      if (m_SelState.IsExist())
        m_SelState.SetEndPos(m_wpCaret);
      else
        m_SelState.Set(m_wpOldCaret, m_wpCaret);

      if (m_wpOldCaret != m_wpCaret) {
        ScrollToCaret();
        Refresh();
        SetCaretInfo();
      }
    } else {
      SelectNone();

      ScrollToCaret();
      SetCaretInfo();
    }
  }
}

void CFX_Edit::OnVK_LEFT(bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    if (bShift) {
      if (m_wpCaret == m_pVT->GetLineBeginPlace(m_wpCaret) &&
          m_wpCaret != m_pVT->GetSectionBeginPlace(m_wpCaret))
        SetCaret(m_pVT->GetPrevWordPlace(m_wpCaret));

      SetCaret(m_pVT->GetPrevWordPlace(m_wpCaret));

      if (m_SelState.IsExist())
        m_SelState.SetEndPos(m_wpCaret);
      else
        m_SelState.Set(m_wpOldCaret, m_wpCaret);

      if (m_wpOldCaret != m_wpCaret) {
        ScrollToCaret();
        Refresh();
        SetCaretInfo();
      }
    } else {
      if (m_SelState.IsExist()) {
        if (m_SelState.BeginPos.WordCmp(m_SelState.EndPos) < 0)
          SetCaret(m_SelState.BeginPos);
        else
          SetCaret(m_SelState.EndPos);

        SelectNone();
        ScrollToCaret();
        SetCaretInfo();
      } else {
        if (m_wpCaret == m_pVT->GetLineBeginPlace(m_wpCaret) &&
            m_wpCaret != m_pVT->GetSectionBeginPlace(m_wpCaret))
          SetCaret(m_pVT->GetPrevWordPlace(m_wpCaret));

        SetCaret(m_pVT->GetPrevWordPlace(m_wpCaret));

        ScrollToCaret();
        SetCaretOrigin();
        SetCaretInfo();
      }
    }
  }
}

void CFX_Edit::OnVK_RIGHT(bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    if (bShift) {
      SetCaret(m_pVT->GetNextWordPlace(m_wpCaret));

      if (m_wpCaret == m_pVT->GetLineEndPlace(m_wpCaret) &&
          m_wpCaret != m_pVT->GetSectionEndPlace(m_wpCaret))
        SetCaret(m_pVT->GetNextWordPlace(m_wpCaret));

      if (m_SelState.IsExist())
        m_SelState.SetEndPos(m_wpCaret);
      else
        m_SelState.Set(m_wpOldCaret, m_wpCaret);

      if (m_wpOldCaret != m_wpCaret) {
        ScrollToCaret();
        Refresh();
        SetCaretInfo();
      }
    } else {
      if (m_SelState.IsExist()) {
        if (m_SelState.BeginPos.WordCmp(m_SelState.EndPos) > 0)
          SetCaret(m_SelState.BeginPos);
        else
          SetCaret(m_SelState.EndPos);

        SelectNone();
        ScrollToCaret();
        SetCaretInfo();
      } else {
        SetCaret(m_pVT->GetNextWordPlace(m_wpCaret));

        if (m_wpCaret == m_pVT->GetLineEndPlace(m_wpCaret) &&
            m_wpCaret != m_pVT->GetSectionEndPlace(m_wpCaret))
          SetCaret(m_pVT->GetNextWordPlace(m_wpCaret));

        ScrollToCaret();
        SetCaretOrigin();
        SetCaretInfo();
      }
    }
  }
}

void CFX_Edit::OnVK_HOME(bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    if (bShift) {
      if (bCtrl)
        SetCaret(m_pVT->GetBeginWordPlace());
      else
        SetCaret(m_pVT->GetLineBeginPlace(m_wpCaret));

      if (m_SelState.IsExist())
        m_SelState.SetEndPos(m_wpCaret);
      else
        m_SelState.Set(m_wpOldCaret, m_wpCaret);

      ScrollToCaret();
      Refresh();
      SetCaretInfo();
    } else {
      if (m_SelState.IsExist()) {
        if (m_SelState.BeginPos.WordCmp(m_SelState.EndPos) < 0)
          SetCaret(m_SelState.BeginPos);
        else
          SetCaret(m_SelState.EndPos);

        SelectNone();
        ScrollToCaret();
        SetCaretInfo();
      } else {
        if (bCtrl)
          SetCaret(m_pVT->GetBeginWordPlace());
        else
          SetCaret(m_pVT->GetLineBeginPlace(m_wpCaret));

        ScrollToCaret();
        SetCaretOrigin();
        SetCaretInfo();
      }
    }
  }
}

void CFX_Edit::OnVK_END(bool bShift, bool bCtrl) {
  if (m_pVT->IsValid()) {
    if (bShift) {
      if (bCtrl)
        SetCaret(m_pVT->GetEndWordPlace());
      else
        SetCaret(m_pVT->GetLineEndPlace(m_wpCaret));

      if (m_SelState.IsExist())
        m_SelState.SetEndPos(m_wpCaret);
      else
        m_SelState.Set(m_wpOldCaret, m_wpCaret);

      ScrollToCaret();
      Refresh();
      SetCaretInfo();
    } else {
      if (m_SelState.IsExist()) {
        if (m_SelState.BeginPos.WordCmp(m_SelState.EndPos) > 0)
          SetCaret(m_SelState.BeginPos);
        else
          SetCaret(m_SelState.EndPos);

        SelectNone();
        ScrollToCaret();
        SetCaretInfo();
      } else {
        if (bCtrl)
          SetCaret(m_pVT->GetEndWordPlace());
        else
          SetCaret(m_pVT->GetLineEndPlace(m_wpCaret));

        ScrollToCaret();
        SetCaretOrigin();
        SetCaretInfo();
      }
    }
  }
}

bool CFX_Edit::InsertWord(uint16_t word,
                          int32_t charset,
                          const CPVT_WordProps* pWordProps,
                          bool bAddUndo,
                          bool bPaint) {
  if (IsTextOverflow() || !m_pVT->IsValid())
    return false;

  m_pVT->UpdateWordPlace(m_wpCaret);
  SetCaret(m_pVT->InsertWord(m_wpCaret, word,
                             GetCharSetFromUnicode(word, charset), pWordProps));
  m_SelState.Set(m_wpCaret, m_wpCaret);
  if (m_wpCaret == m_wpOldCaret)
    return false;

  if (bAddUndo && m_bEnableUndo) {
    AddEditUndoItem(pdfium::MakeUnique<CFXEU_InsertWord>(
        this, m_wpOldCaret, m_wpCaret, word, charset, pWordProps));
  }
  if (bPaint)
    PaintInsertText(m_wpOldCaret, m_wpCaret);

  if (m_bOprNotify && m_pOprNotify)
    m_pOprNotify->OnInsertWord(m_wpCaret, m_wpOldCaret);

  return true;
}

bool CFX_Edit::InsertReturn(const CPVT_SecProps* pSecProps,
                            const CPVT_WordProps* pWordProps,
                            bool bAddUndo,
                            bool bPaint) {
  if (IsTextOverflow() || !m_pVT->IsValid())
    return false;

  m_pVT->UpdateWordPlace(m_wpCaret);
  SetCaret(m_pVT->InsertSection(m_wpCaret, pSecProps, pWordProps));
  m_SelState.Set(m_wpCaret, m_wpCaret);
  if (m_wpCaret == m_wpOldCaret)
    return false;

  if (bAddUndo && m_bEnableUndo) {
    AddEditUndoItem(pdfium::MakeUnique<CFXEU_InsertReturn>(
        this, m_wpOldCaret, m_wpCaret, pSecProps, pWordProps));
  }
  if (bPaint) {
    RearrangePart(CPVT_WordRange(m_wpOldCaret, m_wpCaret));
    ScrollToCaret();
    Refresh();
    SetCaretOrigin();
    SetCaretInfo();
  }
  if (m_bOprNotify && m_pOprNotify)
    m_pOprNotify->OnInsertReturn(m_wpCaret, m_wpOldCaret);

  return true;
}

bool CFX_Edit::Backspace(bool bAddUndo, bool bPaint) {
  if (!m_pVT->IsValid() || m_wpCaret == m_pVT->GetBeginWordPlace())
    return false;

  CPVT_Section section;
  CPVT_Word word;
  if (bAddUndo) {
    CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
    pIterator->SetAt(m_wpCaret);
    pIterator->GetSection(section);
    pIterator->GetWord(word);
  }
  m_pVT->UpdateWordPlace(m_wpCaret);
  SetCaret(m_pVT->BackSpaceWord(m_wpCaret));
  m_SelState.Set(m_wpCaret, m_wpCaret);
  if (m_wpCaret == m_wpOldCaret)
    return false;

  if (bAddUndo && m_bEnableUndo) {
    if (m_wpCaret.SecCmp(m_wpOldCaret) != 0) {
      AddEditUndoItem(pdfium::MakeUnique<CFXEU_Backspace>(
          this, m_wpOldCaret, m_wpCaret, word.Word, word.nCharset,
          section.SecProps, section.WordProps));
    } else {
      AddEditUndoItem(pdfium::MakeUnique<CFXEU_Backspace>(
          this, m_wpOldCaret, m_wpCaret, word.Word, word.nCharset,
          section.SecProps, word.WordProps));
    }
  }
  if (bPaint) {
    RearrangePart(CPVT_WordRange(m_wpCaret, m_wpOldCaret));
    ScrollToCaret();
    Refresh();
    SetCaretOrigin();
    SetCaretInfo();
  }
  if (m_bOprNotify && m_pOprNotify)
    m_pOprNotify->OnBackSpace(m_wpCaret, m_wpOldCaret);

  return true;
}

bool CFX_Edit::Delete(bool bAddUndo, bool bPaint) {
  if (!m_pVT->IsValid() || m_wpCaret == m_pVT->GetEndWordPlace())
    return false;

  CPVT_Section section;
  CPVT_Word word;
  if (bAddUndo) {
    CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
    pIterator->SetAt(m_pVT->GetNextWordPlace(m_wpCaret));
    pIterator->GetSection(section);
    pIterator->GetWord(word);
  }
  m_pVT->UpdateWordPlace(m_wpCaret);
  bool bSecEnd = (m_wpCaret == m_pVT->GetSectionEndPlace(m_wpCaret));
  SetCaret(m_pVT->DeleteWord(m_wpCaret));
  m_SelState.Set(m_wpCaret, m_wpCaret);
  if (bAddUndo && m_bEnableUndo) {
    if (bSecEnd) {
      AddEditUndoItem(pdfium::MakeUnique<CFXEU_Delete>(
          this, m_wpOldCaret, m_wpCaret, word.Word, word.nCharset,
          section.SecProps, section.WordProps, bSecEnd));
    } else {
      AddEditUndoItem(pdfium::MakeUnique<CFXEU_Delete>(
          this, m_wpOldCaret, m_wpCaret, word.Word, word.nCharset,
          section.SecProps, word.WordProps, bSecEnd));
    }
  }
  if (bPaint) {
    RearrangePart(CPVT_WordRange(m_wpOldCaret, m_wpCaret));
    ScrollToCaret();
    Refresh();
    SetCaretOrigin();
    SetCaretInfo();
  }
  if (m_bOprNotify && m_pOprNotify)
    m_pOprNotify->OnDelete(m_wpCaret, m_wpOldCaret);

  return true;
}

bool CFX_Edit::Empty() {
  if (m_pVT->IsValid()) {
    m_pVT->DeleteWords(GetWholeWordRange());
    SetCaret(m_pVT->GetBeginWordPlace());

    return true;
  }

  return false;
}

bool CFX_Edit::Clear(bool bAddUndo, bool bPaint) {
  if (!m_pVT->IsValid() || !m_SelState.IsExist())
    return false;

  CPVT_WordRange range = m_SelState.ConvertToWordRange();
  if (bAddUndo && m_bEnableUndo)
    AddEditUndoItem(pdfium::MakeUnique<CFXEU_Clear>(this, range, GetSelText()));

  SelectNone();
  SetCaret(m_pVT->DeleteWords(range));
  m_SelState.Set(m_wpCaret, m_wpCaret);
  if (bPaint) {
    RearrangePart(range);
    ScrollToCaret();
    Refresh();
    SetCaretOrigin();
    SetCaretInfo();
  }
  if (m_bOprNotify && m_pOprNotify)
    m_pOprNotify->OnClear(m_wpCaret, m_wpOldCaret);

  return true;
}

bool CFX_Edit::InsertText(const CFX_WideString& sText,
                          int32_t charset,
                          bool bAddUndo,
                          bool bPaint) {
  if (IsTextOverflow())
    return false;

  m_pVT->UpdateWordPlace(m_wpCaret);
  SetCaret(DoInsertText(m_wpCaret, sText, charset));
  m_SelState.Set(m_wpCaret, m_wpCaret);
  if (m_wpCaret == m_wpOldCaret)
    return false;

  if (bAddUndo && m_bEnableUndo) {
    AddEditUndoItem(pdfium::MakeUnique<CFXEU_InsertText>(
        this, m_wpOldCaret, m_wpCaret, sText, charset));
  }
  if (bPaint)
    PaintInsertText(m_wpOldCaret, m_wpCaret);

  if (m_bOprNotify && m_pOprNotify)
    m_pOprNotify->OnInsertText(m_wpCaret, m_wpOldCaret);

  return true;
}

void CFX_Edit::PaintInsertText(const CPVT_WordPlace& wpOld,
                               const CPVT_WordPlace& wpNew) {
  if (m_pVT->IsValid()) {
    RearrangePart(CPVT_WordRange(wpOld, wpNew));
    ScrollToCaret();
    Refresh();
    SetCaretOrigin();
    SetCaretInfo();
  }
}

bool CFX_Edit::Redo() {
  if (m_bEnableUndo) {
    if (m_Undo.CanRedo()) {
      m_Undo.Redo();
      return true;
    }
  }

  return false;
}

bool CFX_Edit::Undo() {
  if (m_bEnableUndo) {
    if (m_Undo.CanUndo()) {
      m_Undo.Undo();
      return true;
    }
  }

  return false;
}

void CFX_Edit::SetCaretOrigin() {
  if (!m_pVT->IsValid())
    return;

  CPDF_VariableText::Iterator* pIterator = m_pVT->GetIterator();
  pIterator->SetAt(m_wpCaret);
  CPVT_Word word;
  CPVT_Line line;
  if (pIterator->GetWord(word)) {
    m_ptCaret.x = word.ptWord.x + word.fWidth;
    m_ptCaret.y = word.ptWord.y;
  } else if (pIterator->GetLine(line)) {
    m_ptCaret.x = line.ptLine.x;
    m_ptCaret.y = line.ptLine.y;
  }
}

int32_t CFX_Edit::WordPlaceToWordIndex(const CPVT_WordPlace& place) const {
  if (m_pVT->IsValid())
    return m_pVT->WordPlaceToWordIndex(place);

  return -1;
}

CPVT_WordPlace CFX_Edit::WordIndexToWordPlace(int32_t index) const {
  if (m_pVT->IsValid())
    return m_pVT->WordIndexToWordPlace(index);

  return CPVT_WordPlace();
}

bool CFX_Edit::IsTextFull() const {
  int32_t nTotalWords = m_pVT->GetTotalWords();
  int32_t nLimitChar = m_pVT->GetLimitChar();
  int32_t nCharArray = m_pVT->GetCharArray();

  return IsTextOverflow() || (nLimitChar > 0 && nTotalWords >= nLimitChar) ||
         (nCharArray > 0 && nTotalWords >= nCharArray);
}

bool CFX_Edit::IsTextOverflow() const {
  if (!m_bEnableScroll && !m_bEnableOverflow) {
    CFX_FloatRect rcPlate = m_pVT->GetPlateRect();
    CFX_FloatRect rcContent = m_pVT->GetContentRect();

    if (m_pVT->IsMultiLine() && GetTotalLines() > 1 &&
        IsFloatBigger(rcContent.Height(), rcPlate.Height())) {
      return true;
    }

    if (IsFloatBigger(rcContent.Width(), rcPlate.Width()))
      return true;
  }

  return false;
}

bool CFX_Edit::CanUndo() const {
  if (m_bEnableUndo) {
    return m_Undo.CanUndo();
  }

  return false;
}

bool CFX_Edit::CanRedo() const {
  if (m_bEnableUndo) {
    return m_Undo.CanRedo();
  }

  return false;
}

void CFX_Edit::EnableRefresh(bool bRefresh) {
  m_bEnableRefresh = bRefresh;
}

void CFX_Edit::EnableUndo(bool bUndo) {
  m_bEnableUndo = bUndo;
}

void CFX_Edit::EnableOprNotify(bool bNotify) {
  m_bOprNotify = bNotify;
}

CPVT_WordPlace CFX_Edit::DoInsertText(const CPVT_WordPlace& place,
                                      const CFX_WideString& sText,
                                      int32_t charset) {
  CPVT_WordPlace wp = place;

  if (m_pVT->IsValid()) {
    for (int32_t i = 0, sz = sText.GetLength(); i < sz; i++) {
      uint16_t word = sText[i];
      switch (word) {
        case 0x0D:
          wp = m_pVT->InsertSection(wp, nullptr, nullptr);
          if (sText[i + 1] == 0x0A)
            i++;
          break;
        case 0x0A:
          wp = m_pVT->InsertSection(wp, nullptr, nullptr);
          if (sText[i + 1] == 0x0D)
            i++;
          break;
        case 0x09:
          word = 0x20;
        default:
          wp = m_pVT->InsertWord(wp, word, GetCharSetFromUnicode(word, charset),
                                 nullptr);
          break;
      }
    }
  }

  return wp;
}

int32_t CFX_Edit::GetCharSetFromUnicode(uint16_t word, int32_t nOldCharset) {
  if (IPVT_FontMap* pFontMap = GetFontMap())
    return pFontMap->CharSetFromUnicode(word, nOldCharset);
  return nOldCharset;
}

void CFX_Edit::AddEditUndoItem(
    std::unique_ptr<CFX_Edit_UndoItem> pEditUndoItem) {
  if (m_pGroupUndoItem)
    m_pGroupUndoItem->AddUndoItem(std::move(pEditUndoItem));
  else
    m_Undo.AddItem(std::move(pEditUndoItem));
}

CFX_Edit_LineRectArray::CFX_Edit_LineRectArray() {}

CFX_Edit_LineRectArray::~CFX_Edit_LineRectArray() {}

void CFX_Edit_LineRectArray::operator=(CFX_Edit_LineRectArray&& that) {
  m_LineRects = std::move(that.m_LineRects);
}

void CFX_Edit_LineRectArray::Add(const CPVT_WordRange& wrLine,
                                 const CFX_FloatRect& rcLine) {
  m_LineRects.push_back(pdfium::MakeUnique<CFX_Edit_LineRect>(wrLine, rcLine));
}

int32_t CFX_Edit_LineRectArray::GetSize() const {
  return pdfium::CollectionSize<int32_t>(m_LineRects);
}

CFX_Edit_LineRect* CFX_Edit_LineRectArray::GetAt(int32_t nIndex) const {
  if (nIndex < 0 || nIndex >= GetSize())
    return nullptr;

  return m_LineRects[nIndex].get();
}

CFX_Edit_Select::CFX_Edit_Select() {}

CFX_Edit_Select::CFX_Edit_Select(const CPVT_WordPlace& begin,
                                 const CPVT_WordPlace& end) {
  Set(begin, end);
}

CFX_Edit_Select::CFX_Edit_Select(const CPVT_WordRange& range) {
  Set(range.BeginPos, range.EndPos);
}

CPVT_WordRange CFX_Edit_Select::ConvertToWordRange() const {
  return CPVT_WordRange(BeginPos, EndPos);
}

void CFX_Edit_Select::Default() {
  BeginPos.Default();
  EndPos.Default();
}

void CFX_Edit_Select::Set(const CPVT_WordPlace& begin,
                          const CPVT_WordPlace& end) {
  BeginPos = begin;
  EndPos = end;
}

void CFX_Edit_Select::SetBeginPos(const CPVT_WordPlace& begin) {
  BeginPos = begin;
}

void CFX_Edit_Select::SetEndPos(const CPVT_WordPlace& end) {
  EndPos = end;
}

bool CFX_Edit_Select::IsExist() const {
  return BeginPos != EndPos;
}

CFX_Edit_RectArray::CFX_Edit_RectArray() {}

CFX_Edit_RectArray::~CFX_Edit_RectArray() {}

void CFX_Edit_RectArray::Clear() {
  m_Rects.clear();
}

void CFX_Edit_RectArray::Add(const CFX_FloatRect& rect) {
  // check for overlapped area
  for (const auto& pRect : m_Rects) {
    if (pRect && pRect->Contains(rect))
      return;
  }
  m_Rects.push_back(pdfium::MakeUnique<CFX_FloatRect>(rect));
}

int32_t CFX_Edit_RectArray::GetSize() const {
  return pdfium::CollectionSize<int32_t>(m_Rects);
}

CFX_FloatRect* CFX_Edit_RectArray::GetAt(int32_t nIndex) const {
  if (nIndex < 0 || nIndex >= GetSize())
    return nullptr;

  return m_Rects[nIndex].get();
}
