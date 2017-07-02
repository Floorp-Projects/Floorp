// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_doc.h"

#include <memory>
#include <vector>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_document.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_null.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfdoc/cpdf_dest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"
#include "third_party/base/ptr_util.h"

#ifdef PDF_ENABLE_XFA
#include "fpdfsdk/fpdfxfa/cpdfxfa_context.h"
#endif  // PDF_ENABLE_XFA

class CPDF_TestDocument : public CPDF_Document {
 public:
  CPDF_TestDocument() : CPDF_Document(nullptr) {}

  void SetRoot(CPDF_Dictionary* root) { m_pRootDict = root; }
  CPDF_IndirectObjectHolder* GetHolder() { return this; }
};

#ifdef PDF_ENABLE_XFA
class CPDF_TestXFAContext : public CPDFXFA_Context {
 public:
  CPDF_TestXFAContext()
      : CPDFXFA_Context(pdfium::MakeUnique<CPDF_TestDocument>()) {}

  void SetRoot(CPDF_Dictionary* root) {
    reinterpret_cast<CPDF_TestDocument*>(GetPDFDoc())->SetRoot(root);
  }

  CPDF_IndirectObjectHolder* GetHolder() { return GetPDFDoc(); }
};
using CPDF_TestPdfDocument = CPDF_TestXFAContext;
#else   // PDF_ENABLE_XFA
using CPDF_TestPdfDocument = CPDF_TestDocument;
#endif  // PDF_ENABLE_XFA

class PDFDocTest : public testing::Test {
 public:
  struct DictObjInfo {
    uint32_t num;
    CPDF_Dictionary* obj;
  };

  void SetUp() override {
    // We don't need page module or render module, but
    // initialize them to keep the code sane.
    CPDF_ModuleMgr* module_mgr = CPDF_ModuleMgr::Get();
    module_mgr->InitPageModule();

    m_pDoc = pdfium::MakeUnique<CPDF_TestPdfDocument>();
    m_pIndirectObjs = m_pDoc->GetHolder();

    // Setup the root directory.
    m_pRootObj = pdfium::MakeUnique<CPDF_Dictionary>();
    m_pDoc->SetRoot(m_pRootObj.get());
  }

  void TearDown() override {
    m_pRootObj.reset();
    m_pIndirectObjs = nullptr;
    m_pDoc.reset();
    CPDF_ModuleMgr::Destroy();
  }

  std::vector<DictObjInfo> CreateDictObjs(int num) {
    std::vector<DictObjInfo> info;
    for (int i = 0; i < num; ++i) {
      // Objects created will be released by the document.
      CPDF_Dictionary* obj = m_pIndirectObjs->NewIndirect<CPDF_Dictionary>();
      info.push_back({obj->GetObjNum(), obj});
    }
    return info;
  }

 protected:
  std::unique_ptr<CPDF_TestPdfDocument> m_pDoc;
  CPDF_IndirectObjectHolder* m_pIndirectObjs;
  std::unique_ptr<CPDF_Dictionary> m_pRootObj;
};

TEST_F(PDFDocTest, FindBookmark) {
  {
    // No bookmark information.
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> title =
        GetFPDFWideString(L"");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    title = GetFPDFWideString(L"Preface");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Empty bookmark tree.
    m_pRootObj->SetNewFor<CPDF_Dictionary>("Outlines");
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> title =
        GetFPDFWideString(L"");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    title = GetFPDFWideString(L"Preface");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Check on a regular bookmark tree.
    auto bookmarks = CreateDictObjs(3);

    bookmarks[1].obj->SetNewFor<CPDF_String>("Title", L"Chapter 1");
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs,
                                                bookmarks[0].num);
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs,
                                                bookmarks[2].num);

    bookmarks[2].obj->SetNewFor<CPDF_String>("Title", L"Chapter 2");
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs,
                                                bookmarks[0].num);
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Prev", m_pIndirectObjs,
                                                bookmarks[1].num);

    bookmarks[0].obj->SetNewFor<CPDF_Name>("Type", "Outlines");
    bookmarks[0].obj->SetNewFor<CPDF_Number>("Count", 2);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs,
                                                bookmarks[1].num);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("Last", m_pIndirectObjs,
                                                bookmarks[2].num);

    m_pRootObj->SetNewFor<CPDF_Reference>("Outlines", m_pIndirectObjs,
                                          bookmarks[0].num);

    // Title with no match.
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> title =
        GetFPDFWideString(L"Chapter 3");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with partial match only.
    title = GetFPDFWideString(L"Chapter");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with a match.
    title = GetFPDFWideString(L"Chapter 2");
    EXPECT_EQ(bookmarks[2].obj, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title match is case insensitive.
    title = GetFPDFWideString(L"cHaPter 2");
    EXPECT_EQ(bookmarks[2].obj, FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Circular bookmarks in depth.
    auto bookmarks = CreateDictObjs(3);

    bookmarks[1].obj->SetNewFor<CPDF_String>("Title", L"Chapter 1");
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs,
                                                bookmarks[0].num);
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs,
                                                bookmarks[2].num);

    bookmarks[2].obj->SetNewFor<CPDF_String>("Title", L"Chapter 2");
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs,
                                                bookmarks[1].num);
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs,
                                                bookmarks[1].num);

    bookmarks[0].obj->SetNewFor<CPDF_Name>("Type", "Outlines");
    bookmarks[0].obj->SetNewFor<CPDF_Number>("Count", 2);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs,
                                                bookmarks[1].num);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("Last", m_pIndirectObjs,
                                                bookmarks[2].num);

    m_pRootObj->SetNewFor<CPDF_Reference>("Outlines", m_pIndirectObjs,
                                          bookmarks[0].num);

    // Title with no match.
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> title =
        GetFPDFWideString(L"Chapter 3");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with a match.
    title = GetFPDFWideString(L"Chapter 2");
    EXPECT_EQ(bookmarks[2].obj, FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
  {
    // Circular bookmarks in breadth.
    auto bookmarks = CreateDictObjs(4);

    bookmarks[1].obj->SetNewFor<CPDF_String>("Title", L"Chapter 1");
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs,
                                                bookmarks[0].num);
    bookmarks[1].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs,
                                                bookmarks[2].num);

    bookmarks[2].obj->SetNewFor<CPDF_String>("Title", L"Chapter 2");
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs,
                                                bookmarks[0].num);
    bookmarks[2].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs,
                                                bookmarks[3].num);

    bookmarks[3].obj->SetNewFor<CPDF_String>("Title", L"Chapter 3");
    bookmarks[3].obj->SetNewFor<CPDF_Reference>("Parent", m_pIndirectObjs,
                                                bookmarks[0].num);
    bookmarks[3].obj->SetNewFor<CPDF_Reference>("Next", m_pIndirectObjs,
                                                bookmarks[1].num);

    bookmarks[0].obj->SetNewFor<CPDF_Name>("Type", "Outlines");
    bookmarks[0].obj->SetNewFor<CPDF_Number>("Count", 2);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("First", m_pIndirectObjs,
                                                bookmarks[1].num);
    bookmarks[0].obj->SetNewFor<CPDF_Reference>("Last", m_pIndirectObjs,
                                                bookmarks[2].num);

    m_pRootObj->SetNewFor<CPDF_Reference>("Outlines", m_pIndirectObjs,
                                          bookmarks[0].num);

    // Title with no match.
    std::unique_ptr<unsigned short, pdfium::FreeDeleter> title =
        GetFPDFWideString(L"Chapter 8");
    EXPECT_EQ(nullptr, FPDFBookmark_Find(m_pDoc.get(), title.get()));

    // Title with a match.
    title = GetFPDFWideString(L"Chapter 3");
    EXPECT_EQ(bookmarks[3].obj, FPDFBookmark_Find(m_pDoc.get(), title.get()));
  }
}

TEST_F(PDFDocTest, GetLocationInPage) {
  auto array = pdfium::MakeUnique<CPDF_Array>();
  array->AddNew<CPDF_Number>(0);  // Page Index.
  array->AddNew<CPDF_Name>("XYZ");
  array->AddNew<CPDF_Number>(4);  // X
  array->AddNew<CPDF_Number>(5);  // Y
  array->AddNew<CPDF_Number>(6);  // Zoom.

  FPDF_BOOL hasX;
  FPDF_BOOL hasY;
  FPDF_BOOL hasZoom;
  FS_FLOAT x;
  FS_FLOAT y;
  FS_FLOAT zoom;

  EXPECT_TRUE(FPDFDest_GetLocationInPage(array.get(), &hasX, &hasY, &hasZoom,
                                         &x, &y, &zoom));
  EXPECT_TRUE(hasX);
  EXPECT_TRUE(hasY);
  EXPECT_TRUE(hasZoom);
  EXPECT_EQ(4, x);
  EXPECT_EQ(5, y);
  EXPECT_EQ(6, zoom);

  array->SetNewAt<CPDF_Null>(2);
  array->SetNewAt<CPDF_Null>(3);
  array->SetNewAt<CPDF_Null>(4);
  EXPECT_TRUE(FPDFDest_GetLocationInPage(array.get(), &hasX, &hasY, &hasZoom,
                                         &x, &y, &zoom));
  EXPECT_FALSE(hasX);
  EXPECT_FALSE(hasY);
  EXPECT_FALSE(hasZoom);

  array = pdfium::MakeUnique<CPDF_Array>();
  EXPECT_FALSE(FPDFDest_GetLocationInPage(array.get(), &hasX, &hasY, &hasZoom,
                                          &x, &y, &zoom));
}
