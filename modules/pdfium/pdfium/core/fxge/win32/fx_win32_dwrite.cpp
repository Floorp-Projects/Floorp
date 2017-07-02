// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include <dwrite.h>

#include "core/fxcrt/fx_system.h"
#include "core/fxge/ge/cfx_cliprgn.h"
#include "core/fxge/win32/dwrite_int.h"

typedef HRESULT(__stdcall* FuncType_DWriteCreateFactory)(
    __in DWRITE_FACTORY_TYPE,
    __in REFIID,
    __out IUnknown**);
template <typename InterfaceType>
inline void SafeRelease(InterfaceType** currentObject) {
  if (*currentObject) {
    (*currentObject)->Release();
    *currentObject = nullptr;
  }
}
template <typename InterfaceType>
inline InterfaceType* SafeAcquire(InterfaceType* newObject) {
  if (newObject) {
    newObject->AddRef();
  }
  return newObject;
}

class CDwFontFileStream final : public IDWriteFontFileStream {
 public:
  explicit CDwFontFileStream(void const* fontFileReferenceKey,
                             UINT32 fontFileReferenceKeySize);

  // IUnknown.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void** ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  // IDWriteFontFileStream.
  HRESULT STDMETHODCALLTYPE
  ReadFileFragment(void const** fragmentStart,
                   UINT64 fileOffset,
                   UINT64 fragmentSize,
                   OUT void** fragmentContext) override;
  void STDMETHODCALLTYPE ReleaseFileFragment(void* fragmentContext) override;
  HRESULT STDMETHODCALLTYPE GetFileSize(OUT UINT64* fileSize) override;
  HRESULT STDMETHODCALLTYPE
  GetLastWriteTime(OUT UINT64* lastWriteTime) override;

  bool IsInitialized() { return !!resourcePtr_; }

 private:
  ULONG refCount_;
  void const* resourcePtr_;
  DWORD resourceSize_;
};

class CDwFontFileLoader final : public IDWriteFontFileLoader {
 public:
  // IUnknown.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void** ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  // IDWriteFontFileLoader.
  HRESULT STDMETHODCALLTYPE
  CreateStreamFromKey(void const* fontFileReferenceKey,
                      UINT32 fontFileReferenceKeySize,
                      OUT IDWriteFontFileStream** fontFileStream) override;

  static IDWriteFontFileLoader* GetLoader() {
    if (!instance_) {
      instance_ = new CDwFontFileLoader();
    }
    return instance_;
  }
  static bool IsLoaderInitialized() { return !!instance_; }

 private:
  CDwFontFileLoader();
  ULONG refCount_;
  static IDWriteFontFileLoader* instance_;
};

class CDwFontContext {
 public:
  explicit CDwFontContext(IDWriteFactory* dwriteFactory);
  ~CDwFontContext();

  HRESULT Initialize();

 private:
  CDwFontContext(CDwFontContext const&);
  void operator=(CDwFontContext const&);
  HRESULT hr_;
  IDWriteFactory* dwriteFactory_;
};

class CDwGdiTextRenderer {
 public:
  CDwGdiTextRenderer(CFX_DIBitmap* pBitmap,
                     IDWriteBitmapRenderTarget* bitmapRenderTarget,
                     IDWriteRenderingParams* renderingParams);
  ~CDwGdiTextRenderer();

  HRESULT STDMETHODCALLTYPE DrawGlyphRun(const FX_RECT& text_bbox,
                                         __in_opt CFX_ClipRgn* pClipRgn,
                                         __in_opt DWRITE_MATRIX const* pMatrix,
                                         FLOAT baselineOriginX,
                                         FLOAT baselineOriginY,
                                         DWRITE_MEASURING_MODE measuringMode,
                                         __in DWRITE_GLYPH_RUN const* glyphRun,
                                         const COLORREF& textColor);

 private:
  CFX_DIBitmap* pBitmap_;
  IDWriteBitmapRenderTarget* pRenderTarget_;
  IDWriteRenderingParams* pRenderingParams_;
};

CDWriteExt::CDWriteExt()
    : m_hModule(nullptr),
      m_pDWriteFactory(nullptr),
      m_pDwFontContext(nullptr),
      m_pDwTextRenderer(nullptr) {}

void CDWriteExt::Load() {}

void CDWriteExt::Unload() {
  if (m_pDwFontContext) {
    delete (CDwFontContext*)m_pDwFontContext;
    m_pDwFontContext = nullptr;
  }
  SafeRelease((IDWriteFactory**)&m_pDWriteFactory);
}

CDWriteExt::~CDWriteExt() {
  Unload();
}

LPVOID CDWriteExt::DwCreateFontFaceFromStream(uint8_t* pData,
                                              uint32_t size,
                                              int simulation_style) {
  IDWriteFactory* pDwFactory = (IDWriteFactory*)m_pDWriteFactory;
  IDWriteFontFile* pDwFontFile = nullptr;
  IDWriteFontFace* pDwFontFace = nullptr;
  BOOL isSupportedFontType = false;
  DWRITE_FONT_FILE_TYPE fontFileType;
  DWRITE_FONT_FACE_TYPE fontFaceType;
  UINT32 numberOfFaces;
  DWRITE_FONT_SIMULATIONS fontStyle =
      (DWRITE_FONT_SIMULATIONS)(simulation_style & 3);
  HRESULT hr = S_OK;
  hr = pDwFactory->CreateCustomFontFileReference(
      (void const*)pData, (UINT32)size, CDwFontFileLoader::GetLoader(),
      &pDwFontFile);
  if (FAILED(hr)) {
    goto failed;
  }
  hr = pDwFontFile->Analyze(&isSupportedFontType, &fontFileType, &fontFaceType,
                            &numberOfFaces);
  if (FAILED(hr) || !isSupportedFontType ||
      fontFaceType == DWRITE_FONT_FACE_TYPE_UNKNOWN) {
    goto failed;
  }
  hr = pDwFactory->CreateFontFace(fontFaceType, 1, &pDwFontFile, 0, fontStyle,
                                  &pDwFontFace);
  if (FAILED(hr)) {
    goto failed;
  }
  SafeRelease(&pDwFontFile);
  return pDwFontFace;
failed:
  SafeRelease(&pDwFontFile);
  return nullptr;
}

bool CDWriteExt::DwCreateRenderingTarget(CFX_DIBitmap* pBitmap,
                                         void** renderTarget) {
  if (pBitmap->GetFormat() > FXDIB_Argb) {
    return false;
  }
  IDWriteFactory* pDwFactory = (IDWriteFactory*)m_pDWriteFactory;
  IDWriteGdiInterop* pGdiInterop = nullptr;
  IDWriteBitmapRenderTarget* pBitmapRenderTarget = nullptr;
  IDWriteRenderingParams* pRenderingParams = nullptr;
  HRESULT hr = S_OK;
  hr = pDwFactory->GetGdiInterop(&pGdiInterop);
  if (FAILED(hr)) {
    goto failed;
  }
  hr = pGdiInterop->CreateBitmapRenderTarget(
      nullptr, pBitmap->GetWidth(), pBitmap->GetHeight(), &pBitmapRenderTarget);
  if (FAILED(hr)) {
    goto failed;
  }
  hr = pDwFactory->CreateCustomRenderingParams(
      1.0f, 0.0f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB,
      DWRITE_RENDERING_MODE_DEFAULT, &pRenderingParams);
  if (FAILED(hr)) {
    goto failed;
  }
  hr = pBitmapRenderTarget->SetPixelsPerDip(1.0f);
  if (FAILED(hr)) {
    goto failed;
  }
  *(CDwGdiTextRenderer**)renderTarget =
      new CDwGdiTextRenderer(pBitmap, pBitmapRenderTarget, pRenderingParams);
  SafeRelease(&pGdiInterop);
  SafeRelease(&pBitmapRenderTarget);
  SafeRelease(&pRenderingParams);
  return true;
failed:
  SafeRelease(&pGdiInterop);
  SafeRelease(&pBitmapRenderTarget);
  SafeRelease(&pRenderingParams);
  return false;
}

bool CDWriteExt::DwRendingString(void* renderTarget,
                                 CFX_ClipRgn* pClipRgn,
                                 FX_RECT& stringRect,
                                 CFX_Matrix* pMatrix,
                                 void* font,
                                 FX_FLOAT font_size,
                                 FX_ARGB text_color,
                                 int glyph_count,
                                 unsigned short* glyph_indices,
                                 FX_FLOAT baselineOriginX,
                                 FX_FLOAT baselineOriginY,
                                 void* glyph_offsets,
                                 FX_FLOAT* glyph_advances) {
  if (!renderTarget) {
    return true;
  }
  CDwGdiTextRenderer* pTextRenderer = (CDwGdiTextRenderer*)renderTarget;
  DWRITE_MATRIX transform;
  DWRITE_GLYPH_RUN glyphRun;
  HRESULT hr = S_OK;
  if (pMatrix) {
    transform.m11 = pMatrix->a;
    transform.m12 = pMatrix->b;
    transform.m21 = pMatrix->c;
    transform.m22 = pMatrix->d;
    transform.dx = pMatrix->e;
    transform.dy = pMatrix->f;
  }
  glyphRun.fontFace = (IDWriteFontFace*)font;
  glyphRun.fontEmSize = font_size;
  glyphRun.glyphCount = glyph_count;
  glyphRun.glyphIndices = glyph_indices;
  glyphRun.glyphAdvances = glyph_advances;
  glyphRun.glyphOffsets = (DWRITE_GLYPH_OFFSET*)glyph_offsets;
  glyphRun.isSideways = false;
  glyphRun.bidiLevel = 0;
  hr = pTextRenderer->DrawGlyphRun(
      stringRect, pClipRgn, pMatrix ? &transform : nullptr, baselineOriginX,
      baselineOriginY, DWRITE_MEASURING_MODE_NATURAL, &glyphRun,
      RGB(FXARGB_R(text_color), FXARGB_G(text_color), FXARGB_B(text_color)));
  return SUCCEEDED(hr);
}

void CDWriteExt::DwDeleteRenderingTarget(void* renderTarget) {
  delete (CDwGdiTextRenderer*)renderTarget;
}

void CDWriteExt::DwDeleteFont(void* pFont) {
  if (pFont) {
    SafeRelease((IDWriteFontFace**)&pFont);
  }
}

CDwFontFileStream::CDwFontFileStream(void const* fontFileReferenceKey,
                                     UINT32 fontFileReferenceKeySize) {
  refCount_ = 0;
  resourcePtr_ = fontFileReferenceKey;
  resourceSize_ = fontFileReferenceKeySize;
}

HRESULT STDMETHODCALLTYPE CDwFontFileStream::QueryInterface(REFIID iid,
                                                            void** ppvObject) {
  if (iid == IID_IUnknown || iid == __uuidof(IDWriteFontFileStream)) {
    *ppvObject = this;
    AddRef();
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CDwFontFileStream::AddRef() {
  return InterlockedIncrement((long*)(&refCount_));
}

ULONG STDMETHODCALLTYPE CDwFontFileStream::Release() {
  ULONG newCount = InterlockedDecrement((long*)(&refCount_));
  if (newCount == 0) {
    delete this;
  }
  return newCount;
}

HRESULT STDMETHODCALLTYPE
CDwFontFileStream::ReadFileFragment(void const** fragmentStart,
                                    UINT64 fileOffset,
                                    UINT64 fragmentSize,
                                    OUT void** fragmentContext) {
  if (fileOffset <= resourceSize_ &&
      fragmentSize <= resourceSize_ - fileOffset) {
    *fragmentStart = static_cast<uint8_t const*>(resourcePtr_) +
                     static_cast<size_t>(fileOffset);
    *fragmentContext = nullptr;
    return S_OK;
  }
  *fragmentStart = nullptr;
  *fragmentContext = nullptr;
  return E_FAIL;
}

void STDMETHODCALLTYPE
CDwFontFileStream::ReleaseFileFragment(void* fragmentContext) {}
HRESULT STDMETHODCALLTYPE CDwFontFileStream::GetFileSize(OUT UINT64* fileSize) {
  *fileSize = resourceSize_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
CDwFontFileStream::GetLastWriteTime(OUT UINT64* lastWriteTime) {
  *lastWriteTime = 0;
  return E_NOTIMPL;
}

IDWriteFontFileLoader* CDwFontFileLoader::instance_ = nullptr;
CDwFontFileLoader::CDwFontFileLoader() : refCount_(0) {}
HRESULT STDMETHODCALLTYPE CDwFontFileLoader::QueryInterface(REFIID iid,
                                                            void** ppvObject) {
  if (iid == IID_IUnknown || iid == __uuidof(IDWriteFontFileLoader)) {
    *ppvObject = this;
    AddRef();
    return S_OK;
  }
  *ppvObject = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CDwFontFileLoader::AddRef() {
  return InterlockedIncrement((long*)(&refCount_));
}

ULONG STDMETHODCALLTYPE CDwFontFileLoader::Release() {
  ULONG newCount = InterlockedDecrement((long*)(&refCount_));
  if (newCount == 0) {
    instance_ = nullptr;
    delete this;
  }
  return newCount;
}

HRESULT STDMETHODCALLTYPE CDwFontFileLoader::CreateStreamFromKey(
    void const* fontFileReferenceKey,
    UINT32 fontFileReferenceKeySize,
    OUT IDWriteFontFileStream** fontFileStream) {
  *fontFileStream = nullptr;
  CDwFontFileStream* stream =
      new CDwFontFileStream(fontFileReferenceKey, fontFileReferenceKeySize);
  if (!stream->IsInitialized()) {
    delete stream;
    return E_FAIL;
  }
  *fontFileStream = SafeAcquire(stream);
  return S_OK;
}

CDwFontContext::CDwFontContext(IDWriteFactory* dwriteFactory)
    : hr_(S_FALSE), dwriteFactory_(SafeAcquire(dwriteFactory)) {}

CDwFontContext::~CDwFontContext() {
  if (dwriteFactory_ && hr_ == S_OK) {
    dwriteFactory_->UnregisterFontFileLoader(CDwFontFileLoader::GetLoader());
  }
  SafeRelease(&dwriteFactory_);
}

HRESULT CDwFontContext::Initialize() {
  if (hr_ == S_FALSE) {
    return hr_ = dwriteFactory_->RegisterFontFileLoader(
               CDwFontFileLoader::GetLoader());
  }
  return hr_;
}

CDwGdiTextRenderer::CDwGdiTextRenderer(
    CFX_DIBitmap* pBitmap,
    IDWriteBitmapRenderTarget* bitmapRenderTarget,
    IDWriteRenderingParams* renderingParams)
    : pBitmap_(pBitmap),
      pRenderTarget_(SafeAcquire(bitmapRenderTarget)),
      pRenderingParams_(SafeAcquire(renderingParams)) {}
CDwGdiTextRenderer::~CDwGdiTextRenderer() {
  SafeRelease(&pRenderTarget_);
  SafeRelease(&pRenderingParams_);
}

STDMETHODIMP CDwGdiTextRenderer::DrawGlyphRun(
    const FX_RECT& text_bbox,
    __in_opt CFX_ClipRgn* pClipRgn,
    __in_opt DWRITE_MATRIX const* pMatrix,
    FLOAT baselineOriginX,
    FLOAT baselineOriginY,
    DWRITE_MEASURING_MODE measuringMode,
    __in DWRITE_GLYPH_RUN const* glyphRun,
    const COLORREF& textColor) {
  HRESULT hr = S_OK;
  if (pMatrix) {
    hr = pRenderTarget_->SetCurrentTransform(pMatrix);
    if (FAILED(hr)) {
      return hr;
    }
  }
  HDC hDC = pRenderTarget_->GetMemoryDC();
  HBITMAP hBitmap = (HBITMAP)::GetCurrentObject(hDC, OBJ_BITMAP);
  BITMAP bitmap;
  GetObject(hBitmap, sizeof bitmap, &bitmap);
  CFX_DIBitmap dib;
  dib.Create(bitmap.bmWidth, bitmap.bmHeight,
             bitmap.bmBitsPixel == 24 ? FXDIB_Rgb : FXDIB_Rgb32,
             (uint8_t*)bitmap.bmBits);
  dib.CompositeBitmap(text_bbox.left, text_bbox.top, text_bbox.Width(),
                      text_bbox.Height(), pBitmap_, text_bbox.left,
                      text_bbox.top, FXDIB_BLEND_NORMAL, nullptr);
  hr = pRenderTarget_->DrawGlyphRun(baselineOriginX, baselineOriginY,
                                    measuringMode, glyphRun, pRenderingParams_,
                                    textColor);
  if (FAILED(hr)) {
    return hr;
  }
  pBitmap_->CompositeBitmap(text_bbox.left, text_bbox.top, text_bbox.Width(),
                            text_bbox.Height(), &dib, text_bbox.left,
                            text_bbox.top, FXDIB_BLEND_NORMAL, pClipRgn);
  return hr;
}
