// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_FXEDIT_FXET_EDIT_H_
#define FPDFSDK_FXEDIT_FXET_EDIT_H_

#include <deque>
#include <memory>
#include <vector>

#include "core/fpdfdoc/cpvt_secprops.h"
#include "core/fpdfdoc/cpvt_wordprops.h"
#include "fpdfsdk/fxedit/fx_edit.h"

class CFFL_FormFiller;
class CFX_Edit;
class CFX_Edit_Iterator;
class CFX_Edit_Provider;
class CFX_RenderDevice;
class CFX_SystemHandler;
class CPDF_PageObjectHolder;
class CPDF_TextObject;
class CPWL_Edit;
class CPWL_EditCtrl;

class IFX_Edit_UndoItem;

struct CFX_Edit_LineRect {
  CFX_Edit_LineRect(const CPVT_WordRange& wrLine, const CFX_FloatRect& rcLine)
      : m_wrLine(wrLine), m_rcLine(rcLine) {}

  CPVT_WordRange m_wrLine;
  CFX_FloatRect m_rcLine;
};

class CFX_Edit_LineRectArray {
 public:
  CFX_Edit_LineRectArray();
  virtual ~CFX_Edit_LineRectArray();

  void operator=(CFX_Edit_LineRectArray&& rects);
  void Add(const CPVT_WordRange& wrLine, const CFX_FloatRect& rcLine);

  int32_t GetSize() const;
  CFX_Edit_LineRect* GetAt(int32_t nIndex) const;

 private:
  std::vector<std::unique_ptr<CFX_Edit_LineRect>> m_LineRects;
};

class CFX_Edit_RectArray {
 public:
  CFX_Edit_RectArray();
  virtual ~CFX_Edit_RectArray();

  void Clear();
  void Add(const CFX_FloatRect& rect);

  int32_t GetSize() const;
  CFX_FloatRect* GetAt(int32_t nIndex) const;

 private:
  std::vector<std::unique_ptr<CFX_FloatRect>> m_Rects;
};

class CFX_Edit_Refresh {
 public:
  CFX_Edit_Refresh();
  virtual ~CFX_Edit_Refresh();

  void BeginRefresh();
  void Push(const CPVT_WordRange& linerange, const CFX_FloatRect& rect);
  void NoAnalyse();
  void AddRefresh(const CFX_FloatRect& rect);
  const CFX_Edit_RectArray* GetRefreshRects() const;
  void EndRefresh();

 private:
  CFX_Edit_LineRectArray m_NewLineRects;
  CFX_Edit_LineRectArray m_OldLineRects;
  CFX_Edit_RectArray m_RefreshRects;
};

class CFX_Edit_Select {
 public:
  CFX_Edit_Select();
  CFX_Edit_Select(const CPVT_WordPlace& begin, const CPVT_WordPlace& end);
  explicit CFX_Edit_Select(const CPVT_WordRange& range);

  void Default();
  void Set(const CPVT_WordPlace& begin, const CPVT_WordPlace& end);
  void SetBeginPos(const CPVT_WordPlace& begin);
  void SetEndPos(const CPVT_WordPlace& end);

  CPVT_WordRange ConvertToWordRange() const;
  bool IsExist() const;

  CPVT_WordPlace BeginPos;
  CPVT_WordPlace EndPos;
};

class CFX_Edit_Undo {
 public:
  explicit CFX_Edit_Undo(int32_t nBufsize);
  virtual ~CFX_Edit_Undo();

  void AddItem(std::unique_ptr<IFX_Edit_UndoItem> pItem);
  void Undo();
  void Redo();
  bool CanUndo() const;
  bool CanRedo() const;
  bool IsModified() const;
  void Reset();

 private:
  void RemoveHeads();
  void RemoveTails();

  std::deque<std::unique_ptr<IFX_Edit_UndoItem>> m_UndoItemStack;
  size_t m_nCurUndoPos;
  size_t m_nBufSize;
  bool m_bModified;
  bool m_bVirgin;
  bool m_bWorking;
};

class IFX_Edit_UndoItem {
 public:
  virtual ~IFX_Edit_UndoItem() {}

  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual CFX_WideString GetUndoTitle() const = 0;
};

class CFX_Edit_UndoItem : public IFX_Edit_UndoItem {
 public:
  CFX_Edit_UndoItem();
  ~CFX_Edit_UndoItem() override;

  CFX_WideString GetUndoTitle() const override;

  void SetFirst(bool bFirst);
  void SetLast(bool bLast);
  bool IsLast();

 private:
  bool m_bFirst;
  bool m_bLast;
};

class CFX_Edit_GroupUndoItem : public IFX_Edit_UndoItem {
 public:
  explicit CFX_Edit_GroupUndoItem(const CFX_WideString& sTitle);
  ~CFX_Edit_GroupUndoItem() override;

  // IFX_Edit_UndoItem
  void Undo() override;
  void Redo() override;
  CFX_WideString GetUndoTitle() const override;

  void AddUndoItem(std::unique_ptr<CFX_Edit_UndoItem> pUndoItem);
  void UpdateItems();

 private:
  CFX_WideString m_sTitle;
  std::vector<std::unique_ptr<CFX_Edit_UndoItem>> m_Items;
};

class CFXEU_InsertWord : public CFX_Edit_UndoItem {
 public:
  CFXEU_InsertWord(CFX_Edit* pEdit,
                   const CPVT_WordPlace& wpOldPlace,
                   const CPVT_WordPlace& wpNewPlace,
                   uint16_t word,
                   int32_t charset,
                   const CPVT_WordProps* pWordProps);
  ~CFXEU_InsertWord() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  uint16_t m_Word;
  int32_t m_nCharset;
  CPVT_WordProps m_WordProps;
};

class CFXEU_InsertReturn : public CFX_Edit_UndoItem {
 public:
  CFXEU_InsertReturn(CFX_Edit* pEdit,
                     const CPVT_WordPlace& wpOldPlace,
                     const CPVT_WordPlace& wpNewPlace,
                     const CPVT_SecProps* pSecProps,
                     const CPVT_WordProps* pWordProps);
  ~CFXEU_InsertReturn() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
};

class CFXEU_Backspace : public CFX_Edit_UndoItem {
 public:
  CFXEU_Backspace(CFX_Edit* pEdit,
                  const CPVT_WordPlace& wpOldPlace,
                  const CPVT_WordPlace& wpNewPlace,
                  uint16_t word,
                  int32_t charset,
                  const CPVT_SecProps& SecProps,
                  const CPVT_WordProps& WordProps);
  ~CFXEU_Backspace() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  uint16_t m_Word;
  int32_t m_nCharset;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
};

class CFXEU_Delete : public CFX_Edit_UndoItem {
 public:
  CFXEU_Delete(CFX_Edit* pEdit,
               const CPVT_WordPlace& wpOldPlace,
               const CPVT_WordPlace& wpNewPlace,
               uint16_t word,
               int32_t charset,
               const CPVT_SecProps& SecProps,
               const CPVT_WordProps& WordProps,
               bool bSecEnd);
  ~CFXEU_Delete() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  uint16_t m_Word;
  int32_t m_nCharset;
  CPVT_SecProps m_SecProps;
  CPVT_WordProps m_WordProps;
  bool m_bSecEnd;
};

class CFXEU_Clear : public CFX_Edit_UndoItem {
 public:
  CFXEU_Clear(CFX_Edit* pEdit,
              const CPVT_WordRange& wrSel,
              const CFX_WideString& swText);
  ~CFXEU_Clear() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordRange m_wrSel;
  CFX_WideString m_swText;
};

class CFXEU_InsertText : public CFX_Edit_UndoItem {
 public:
  CFXEU_InsertText(CFX_Edit* pEdit,
                   const CPVT_WordPlace& wpOldPlace,
                   const CPVT_WordPlace& wpNewPlace,
                   const CFX_WideString& swText,
                   int32_t charset);
  ~CFXEU_InsertText() override;

  // CFX_Edit_UndoItem
  void Redo() override;
  void Undo() override;

 private:
  CFX_Edit* m_pEdit;

  CPVT_WordPlace m_wpOld;
  CPVT_WordPlace m_wpNew;
  CFX_WideString m_swText;
  int32_t m_nCharset;
};

class CFX_Edit {
 public:
  static CFX_ByteString GetEditAppearanceStream(CFX_Edit* pEdit,
                                                const CFX_PointF& ptOffset,
                                                const CPVT_WordRange* pRange,
                                                bool bContinuous,
                                                uint16_t SubWord);
  static CFX_ByteString GetSelectAppearanceStream(CFX_Edit* pEdit,
                                                  const CFX_PointF& ptOffset,
                                                  const CPVT_WordRange* pRange);
  static void DrawEdit(CFX_RenderDevice* pDevice,
                       CFX_Matrix* pUser2Device,
                       CFX_Edit* pEdit,
                       FX_COLORREF crTextFill,
                       const CFX_FloatRect& rcClip,
                       const CFX_PointF& ptOffset,
                       const CPVT_WordRange* pRange,
                       CFX_SystemHandler* pSystemHandler,
                       CFFL_FormFiller* pFFLData);

  CFX_Edit();
  ~CFX_Edit();

  void SetFontMap(IPVT_FontMap* pFontMap);
  void SetNotify(CPWL_EditCtrl* pNotify);
  void SetOprNotify(CPWL_Edit* pOprNotify);

  // Returns an iterator for the contents. Should not be released.
  CFX_Edit_Iterator* GetIterator();
  IPVT_FontMap* GetFontMap();
  void Initialize();

  // Set the bounding box of the text area.
  void SetPlateRect(const CFX_FloatRect& rect);
  void SetScrollPos(const CFX_PointF& point);

  // Set the horizontal text alignment. (nFormat [0:left, 1:middle, 2:right])
  void SetAlignmentH(int32_t nFormat, bool bPaint);
  // Set the vertical text alignment. (nFormat [0:left, 1:middle, 2:right])
  void SetAlignmentV(int32_t nFormat, bool bPaint);

  // Set the substitution character for hidden text.
  void SetPasswordChar(uint16_t wSubWord, bool bPaint);

  // Set the maximum number of words in the text.
  void SetLimitChar(int32_t nLimitChar);
  void SetCharArray(int32_t nCharArray);
  void SetCharSpace(FX_FLOAT fCharSpace);
  void SetMultiLine(bool bMultiLine, bool bPaint);
  void SetAutoReturn(bool bAuto, bool bPaint);
  void SetAutoFontSize(bool bAuto, bool bPaint);
  void SetAutoScroll(bool bAuto, bool bPaint);
  void SetFontSize(FX_FLOAT fFontSize);
  void SetTextOverflow(bool bAllowed, bool bPaint);
  void OnMouseDown(const CFX_PointF& point, bool bShift, bool bCtrl);
  void OnMouseMove(const CFX_PointF& point, bool bShift, bool bCtrl);
  void OnVK_UP(bool bShift, bool bCtrl);
  void OnVK_DOWN(bool bShift, bool bCtrl);
  void OnVK_LEFT(bool bShift, bool bCtrl);
  void OnVK_RIGHT(bool bShift, bool bCtrl);
  void OnVK_HOME(bool bShift, bool bCtrl);
  void OnVK_END(bool bShift, bool bCtrl);
  void SetText(const CFX_WideString& sText);
  bool InsertWord(uint16_t word, int32_t charset);
  bool InsertReturn();
  bool Backspace();
  bool Delete();
  bool Clear();
  bool InsertText(const CFX_WideString& sText, int32_t charset);
  bool Redo();
  bool Undo();
  int32_t WordPlaceToWordIndex(const CPVT_WordPlace& place) const;
  CPVT_WordPlace WordIndexToWordPlace(int32_t index) const;
  CPVT_WordPlace SearchWordPlace(const CFX_PointF& point) const;
  int32_t GetCaret() const;
  CPVT_WordPlace GetCaretWordPlace() const;
  CFX_WideString GetSelText() const;
  CFX_WideString GetText() const;
  FX_FLOAT GetFontSize() const;
  uint16_t GetPasswordChar() const;
  CFX_PointF GetScrollPos() const;
  int32_t GetCharArray() const;
  CFX_FloatRect GetContentRect() const;
  CFX_WideString GetRangeText(const CPVT_WordRange& range) const;
  int32_t GetHorzScale() const;
  FX_FLOAT GetCharSpace() const;
  int32_t GetTotalWords() const;
  void SetSel(int32_t nStartChar, int32_t nEndChar);
  void GetSel(int32_t& nStartChar, int32_t& nEndChar) const;
  void SelectAll();
  void SelectNone();
  bool IsSelected() const;
  void Paint();
  void EnableRefresh(bool bRefresh);
  void RefreshWordRange(const CPVT_WordRange& wr);
  void SetCaret(int32_t nPos);
  CPVT_WordRange GetWholeWordRange() const;
  CPVT_WordRange GetSelectWordRange() const;
  void EnableUndo(bool bUndo);
  void EnableOprNotify(bool bNotify);
  bool IsTextFull() const;
  bool IsTextOverflow() const;
  bool CanUndo() const;
  bool CanRedo() const;
  CPVT_WordRange GetVisibleWordRange() const;

  bool Empty();

  CPVT_WordPlace DoInsertText(const CPVT_WordPlace& place,
                              const CFX_WideString& sText,
                              int32_t charset);
  int32_t GetCharSetFromUnicode(uint16_t word, int32_t nOldCharset);

  int32_t GetTotalLines() const;

 private:
  friend class CFX_Edit_Iterator;
  friend class CFXEU_InsertWord;
  friend class CFXEU_InsertReturn;
  friend class CFXEU_Backspace;
  friend class CFXEU_Delete;
  friend class CFXEU_Clear;
  friend class CFXEU_InsertText;

  void SetSel(const CPVT_WordPlace& begin, const CPVT_WordPlace& end);

  void RearrangeAll();
  void RearrangePart(const CPVT_WordRange& range);
  void ScrollToCaret();
  void SetScrollInfo();
  void SetScrollPosX(FX_FLOAT fx);
  void SetScrollPosY(FX_FLOAT fy);
  void SetScrollLimit();
  void SetContentChanged();

  bool InsertWord(uint16_t word,
                  int32_t charset,
                  const CPVT_WordProps* pWordProps,
                  bool bAddUndo,
                  bool bPaint);
  bool InsertReturn(const CPVT_SecProps* pSecProps,
                    const CPVT_WordProps* pWordProps,
                    bool bAddUndo,
                    bool bPaint);
  bool Backspace(bool bAddUndo, bool bPaint);
  bool Delete(bool bAddUndo, bool bPaint);
  bool Clear(bool bAddUndo, bool bPaint);
  bool InsertText(const CFX_WideString& sText,
                  int32_t charset,
                  bool bAddUndo,
                  bool bPaint);
  void PaintInsertText(const CPVT_WordPlace& wpOld,
                       const CPVT_WordPlace& wpNew);

  inline CFX_PointF VTToEdit(const CFX_PointF& point) const;
  inline CFX_PointF EditToVT(const CFX_PointF& point) const;
  inline CFX_FloatRect VTToEdit(const CFX_FloatRect& rect) const;

  void Refresh();
  void RefreshPushLineRects(const CPVT_WordRange& wr);

  void SetCaret(const CPVT_WordPlace& place);
  void SetCaretInfo();
  void SetCaretOrigin();

  void AddEditUndoItem(std::unique_ptr<CFX_Edit_UndoItem> pEditUndoItem);

 private:
  std::unique_ptr<CPDF_VariableText> m_pVT;
  CPWL_EditCtrl* m_pNotify;
  CPWL_Edit* m_pOprNotify;
  std::unique_ptr<CFX_Edit_Provider> m_pVTProvider;
  CPVT_WordPlace m_wpCaret;
  CPVT_WordPlace m_wpOldCaret;
  CFX_Edit_Select m_SelState;
  CFX_PointF m_ptScrollPos;
  CFX_PointF m_ptRefreshScrollPos;
  bool m_bEnableScroll;
  std::unique_ptr<CFX_Edit_Iterator> m_pIterator;
  CFX_Edit_Refresh m_Refresh;
  CFX_PointF m_ptCaret;
  CFX_Edit_Undo m_Undo;
  int32_t m_nAlignment;
  bool m_bNotifyFlag;
  bool m_bEnableOverflow;
  bool m_bEnableRefresh;
  CFX_FloatRect m_rcOldContent;
  bool m_bEnableUndo;
  bool m_bOprNotify;
  CFX_Edit_GroupUndoItem* m_pGroupUndoItem;
};

class CFX_Edit_Iterator {
 public:
  CFX_Edit_Iterator(CFX_Edit* pEdit, CPDF_VariableText::Iterator* pVTIterator);
  ~CFX_Edit_Iterator();

  bool NextWord();
  bool PrevWord();
  bool GetWord(CPVT_Word& word) const;
  bool GetLine(CPVT_Line& line) const;
  bool GetSection(CPVT_Section& section) const;
  void SetAt(int32_t nWordIndex);
  void SetAt(const CPVT_WordPlace& place);
  const CPVT_WordPlace& GetAt() const;

 private:
  CFX_Edit* m_pEdit;
  CPDF_VariableText::Iterator* m_pVTIterator;
};

class CFX_Edit_Provider : public CPDF_VariableText::Provider {
 public:
  explicit CFX_Edit_Provider(IPVT_FontMap* pFontMap);
  ~CFX_Edit_Provider() override;

  IPVT_FontMap* GetFontMap();

  // CPDF_VariableText::Provider:
  int32_t GetCharWidth(int32_t nFontIndex, uint16_t word) override;
  int32_t GetTypeAscent(int32_t nFontIndex) override;
  int32_t GetTypeDescent(int32_t nFontIndex) override;
  int32_t GetWordFontIndex(uint16_t word,
                           int32_t charset,
                           int32_t nFontIndex) override;
  int32_t GetDefaultFontIndex() override;
  bool IsLatinWord(uint16_t word) override;

 private:
  IPVT_FontMap* m_pFontMap;
};

#endif  // FPDFSDK_FXEDIT_FXET_EDIT_H_
