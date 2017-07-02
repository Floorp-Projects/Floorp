// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_PDFWINDOW_PWL_ICON_H_
#define FPDFSDK_PDFWINDOW_PWL_ICON_H_

#include "core/fxcrt/fx_string.h"
#include "fpdfsdk/pdfwindow/PWL_Wnd.h"

class CPWL_Image : public CPWL_Wnd {
 public:
  CPWL_Image();
  ~CPWL_Image() override;

  virtual CFX_ByteString GetImageAppStream();

  virtual void GetScale(FX_FLOAT& fHScale, FX_FLOAT& fVScale);
  virtual void GetImageOffset(FX_FLOAT& x, FX_FLOAT& y);
  virtual CPDF_Stream* GetPDFStream();

 public:
  void SetPDFStream(CPDF_Stream* pStream);
  void GetImageSize(FX_FLOAT& fWidth, FX_FLOAT& fHeight);
  CFX_Matrix GetImageMatrix();
  CFX_ByteString GetImageAlias();
  void SetImageAlias(const FX_CHAR* sImageAlias);

 protected:
  CPDF_Stream* m_pPDFStream;
  CFX_ByteString m_sImageAlias;
};

class CPWL_Icon : public CPWL_Image {
 public:
  CPWL_Icon();
  ~CPWL_Icon() override;

  virtual CPDF_IconFit* GetIconFit();

  // CPWL_Image
  void GetScale(FX_FLOAT& fHScale, FX_FLOAT& fVScale) override;
  void GetImageOffset(FX_FLOAT& x, FX_FLOAT& y) override;

  int32_t GetScaleMethod();
  bool IsProportionalScale();
  void GetIconPosition(FX_FLOAT& fLeft, FX_FLOAT& fBottom);

  void SetIconFit(CPDF_IconFit* pIconFit) { m_pIconFit = pIconFit; }

 private:
  CPDF_IconFit* m_pIconFit;
};

#endif  // FPDFSDK_PDFWINDOW_PWL_ICON_H_
