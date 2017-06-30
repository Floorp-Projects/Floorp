// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_CPDF_ANNOT_H_
#define CORE_FPDFDOC_CPDF_ANNOT_H_

#include <map>
#include <memory>

#include "core/fxcrt/cfx_maybe_owned.h"
#include "core/fxcrt/fx_coordinates.h"
#include "core/fxcrt/fx_string.h"
#include "core/fxcrt/fx_system.h"

class CFX_RenderDevice;
class CPDF_Dictionary;
class CPDF_Document;
class CPDF_Form;
class CPDF_Page;
class CPDF_RenderContext;
class CPDF_RenderOptions;
class CPDF_Stream;

#define ANNOTFLAG_INVISIBLE 0x0001
#define ANNOTFLAG_HIDDEN 0x0002
#define ANNOTFLAG_PRINT 0x0004
#define ANNOTFLAG_NOZOOM 0x0008
#define ANNOTFLAG_NOROTATE 0x0010
#define ANNOTFLAG_NOVIEW 0x0020
#define ANNOTFLAG_READONLY 0x0040
#define ANNOTFLAG_LOCKED 0x0080
#define ANNOTFLAG_TOGGLENOVIEW 0x0100

class CPDF_Annot {
 public:
  enum AppearanceMode { Normal, Rollover, Down };
  enum class Subtype {
    UNKNOWN = 0,
    TEXT,
    LINK,
    FREETEXT,
    LINE,
    SQUARE,
    CIRCLE,
    POLYGON,
    POLYLINE,
    HIGHLIGHT,
    UNDERLINE,
    SQUIGGLY,
    STRIKEOUT,
    STAMP,
    CARET,
    INK,
    POPUP,
    FILEATTACHMENT,
    SOUND,
    MOVIE,
    WIDGET,
    SCREEN,
    PRINTERMARK,
    TRAPNET,
    WATERMARK,
    THREED,
    RICHMEDIA,
    XFAWIDGET
  };

  static bool IsAnnotationHidden(CPDF_Dictionary* pAnnotDict);
  static CPDF_Annot::Subtype StringToAnnotSubtype(
      const CFX_ByteString& sSubtype);
  static CFX_ByteString AnnotSubtypeToString(CPDF_Annot::Subtype nSubtype);
  static CFX_FloatRect RectFromQuadPoints(CPDF_Dictionary* pAnnotDict);

  // The second constructor does not take ownership of the dictionary.
  CPDF_Annot(std::unique_ptr<CPDF_Dictionary> pDict, CPDF_Document* pDocument);
  CPDF_Annot(CPDF_Dictionary* pDict, CPDF_Document* pDocument);
  ~CPDF_Annot();

  CPDF_Annot::Subtype GetSubtype() const;
  uint32_t GetFlags() const;
  CFX_FloatRect GetRect() const;
  CPDF_Document* GetDocument() const { return m_pDocument; }
  CPDF_Dictionary* GetAnnotDict() const { return m_pAnnotDict.Get(); }

  bool DrawAppearance(CPDF_Page* pPage,
                      CFX_RenderDevice* pDevice,
                      const CFX_Matrix* pUser2Device,
                      AppearanceMode mode,
                      const CPDF_RenderOptions* pOptions);
  bool DrawInContext(const CPDF_Page* pPage,
                     CPDF_RenderContext* pContext,
                     const CFX_Matrix* pUser2Device,
                     AppearanceMode mode);

  void ClearCachedAP();
  void DrawBorder(CFX_RenderDevice* pDevice,
                  const CFX_Matrix* pUser2Device,
                  const CPDF_RenderOptions* pOptions);
  CPDF_Form* GetAPForm(const CPDF_Page* pPage, AppearanceMode mode);
  void SetOpenState(bool bOpenState) { m_bOpenState = bOpenState; }
  CPDF_Annot* GetPopupAnnot() const { return m_pPopupAnnot; }
  void SetPopupAnnot(CPDF_Annot* pAnnot) { m_pPopupAnnot = pAnnot; }

 private:
  void Init();
  void GenerateAPIfNeeded();
  bool ShouldDrawAnnotation();

  CFX_FloatRect RectForDrawing() const;

  CFX_MaybeOwned<CPDF_Dictionary> m_pAnnotDict;
  CPDF_Document* const m_pDocument;
  CPDF_Annot::Subtype m_nSubtype;
  std::map<CPDF_Stream*, std::unique_ptr<CPDF_Form>> m_APMap;
  // |m_bOpenState| is only set for popup annotations.
  bool m_bOpenState = false;
  bool m_bHasGeneratedAP;
  bool m_bIsTextMarkupAnnotation;
  // Not owned. If there is a valid pointer in |m_pPopupAnnot|,
  // then this annot is never a popup.
  CPDF_Annot* m_pPopupAnnot = nullptr;
};

CPDF_Stream* FPDFDOC_GetAnnotAP(CPDF_Dictionary* pAnnotDict,
                                CPDF_Annot::AppearanceMode mode);

#endif  // CORE_FPDFDOC_CPDF_ANNOT_H_
