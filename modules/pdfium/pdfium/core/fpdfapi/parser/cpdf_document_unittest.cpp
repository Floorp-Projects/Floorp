// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_document.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_boolean.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_linearized_header.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_parser.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fxcrt/fx_memory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/base/ptr_util.h"

namespace {

CPDF_Dictionary* CreatePageTreeNode(std::unique_ptr<CPDF_Array> kids,
                                    CPDF_Document* pDoc,
                                    int count) {
  CPDF_Array* pUnowned = pDoc->AddIndirectObject(std::move(kids))->AsArray();
  CPDF_Dictionary* pageNode = pDoc->NewIndirect<CPDF_Dictionary>();
  pageNode->SetNewFor<CPDF_String>("Type", "Pages", false);
  pageNode->SetNewFor<CPDF_Reference>("Kids", pDoc, pUnowned->GetObjNum());
  pageNode->SetNewFor<CPDF_Number>("Count", count);
  for (size_t i = 0; i < pUnowned->GetCount(); i++) {
    pUnowned->GetDictAt(i)->SetNewFor<CPDF_Reference>("Parent", pDoc,
                                                      pageNode->GetObjNum());
  }
  return pageNode;
}

std::unique_ptr<CPDF_Dictionary> CreateNumberedPage(size_t number) {
  auto page = pdfium::MakeUnique<CPDF_Dictionary>();
  page->SetNewFor<CPDF_String>("Type", "Page", false);
  page->SetNewFor<CPDF_Number>("PageNumbering", static_cast<int>(number));
  return page;
}

class CPDF_TestDocumentForPages : public CPDF_Document {
 public:
  CPDF_TestDocumentForPages() : CPDF_Document(nullptr) {
    // Set up test
    auto zeroToTwo = pdfium::MakeUnique<CPDF_Array>();
    zeroToTwo->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(0))->GetObjNum());
    zeroToTwo->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(1))->GetObjNum());
    zeroToTwo->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(2))->GetObjNum());
    CPDF_Dictionary* branch1 =
        CreatePageTreeNode(std::move(zeroToTwo), this, 3);

    auto zeroToThree = pdfium::MakeUnique<CPDF_Array>();
    zeroToThree->AddNew<CPDF_Reference>(this, branch1->GetObjNum());
    zeroToThree->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(3))->GetObjNum());
    CPDF_Dictionary* branch2 =
        CreatePageTreeNode(std::move(zeroToThree), this, 4);

    auto fourFive = pdfium::MakeUnique<CPDF_Array>();
    fourFive->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(4))->GetObjNum());
    fourFive->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(5))->GetObjNum());
    CPDF_Dictionary* branch3 = CreatePageTreeNode(std::move(fourFive), this, 2);

    auto justSix = pdfium::MakeUnique<CPDF_Array>();
    justSix->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(6))->GetObjNum());
    CPDF_Dictionary* branch4 = CreatePageTreeNode(std::move(justSix), this, 1);

    auto allPages = pdfium::MakeUnique<CPDF_Array>();
    allPages->AddNew<CPDF_Reference>(this, branch2->GetObjNum());
    allPages->AddNew<CPDF_Reference>(this, branch3->GetObjNum());
    allPages->AddNew<CPDF_Reference>(this, branch4->GetObjNum());
    CPDF_Dictionary* pagesDict =
        CreatePageTreeNode(std::move(allPages), this, 7);

    m_pOwnedRootDict = pdfium::MakeUnique<CPDF_Dictionary>();
    m_pOwnedRootDict->SetNewFor<CPDF_Reference>("Pages", this,
                                                pagesDict->GetObjNum());
    m_pRootDict = m_pOwnedRootDict.get();
    m_PageList.resize(7);
  }

 private:
  std::unique_ptr<CPDF_Dictionary> m_pOwnedRootDict;
};

class CPDF_TestDocumentWithPageWithoutPageNum : public CPDF_Document {
 public:
  CPDF_TestDocumentWithPageWithoutPageNum() : CPDF_Document(nullptr) {
    // Set up test
    auto allPages = pdfium::MakeUnique<CPDF_Array>();
    allPages->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(0))->GetObjNum());
    allPages->AddNew<CPDF_Reference>(
        this, AddIndirectObject(CreateNumberedPage(1))->GetObjNum());
    // Page without pageNum.
    allPages->Add(CreateNumberedPage(2));
    CPDF_Dictionary* pagesDict =
        CreatePageTreeNode(std::move(allPages), this, 3);
    m_pOwnedRootDict = pdfium::MakeUnique<CPDF_Dictionary>();
    m_pOwnedRootDict->SetNewFor<CPDF_Reference>("Pages", this,
                                                pagesDict->GetObjNum());
    m_pRootDict = m_pOwnedRootDict.get();
    m_PageList.resize(3);
  }

 private:
  std::unique_ptr<CPDF_Dictionary> m_pOwnedRootDict;
};

class TestLinearized : public CPDF_LinearizedHeader {
 public:
  explicit TestLinearized(CPDF_Dictionary* dict)
      : CPDF_LinearizedHeader(dict) {}
};
}  // namespace

class cpdf_document_test : public testing::Test {
 public:
  void SetUp() override {
    CPDF_ModuleMgr* module_mgr = CPDF_ModuleMgr::Get();
    module_mgr->InitPageModule();
  }
  void TearDown() override { CPDF_ModuleMgr::Destroy(); }
};

TEST_F(cpdf_document_test, GetPages) {
  std::unique_ptr<CPDF_TestDocumentForPages> document =
      pdfium::MakeUnique<CPDF_TestDocumentForPages>();
  for (int i = 0; i < 7; i++) {
    CPDF_Dictionary* page = document->GetPage(i);
    ASSERT_TRUE(page);
    ASSERT_TRUE(page->KeyExist("PageNumbering"));
    EXPECT_EQ(i, page->GetIntegerFor("PageNumbering"));
  }
  CPDF_Dictionary* page = document->GetPage(7);
  EXPECT_FALSE(page);
}

TEST_F(cpdf_document_test, GetPageWithoutObjNumTwice) {
  std::unique_ptr<CPDF_TestDocumentWithPageWithoutPageNum> document =
      pdfium::MakeUnique<CPDF_TestDocumentWithPageWithoutPageNum>();
  const CPDF_Dictionary* page = document->GetPage(2);
  ASSERT_TRUE(page);
  // This is page without obj num.
  ASSERT_EQ(0ul, page->GetObjNum());
  const CPDF_Dictionary* second_call_page = document->GetPage(2);
  EXPECT_TRUE(second_call_page);
  EXPECT_EQ(page, second_call_page);
}

TEST_F(cpdf_document_test, GetPagesReverseOrder) {
  std::unique_ptr<CPDF_TestDocumentForPages> document =
      pdfium::MakeUnique<CPDF_TestDocumentForPages>();
  for (int i = 6; i >= 0; i--) {
    CPDF_Dictionary* page = document->GetPage(i);
    ASSERT_TRUE(page);
    ASSERT_TRUE(page->KeyExist("PageNumbering"));
    EXPECT_EQ(i, page->GetIntegerFor("PageNumbering"));
  }
  CPDF_Dictionary* page = document->GetPage(7);
  EXPECT_FALSE(page);
}

TEST_F(cpdf_document_test, GetPagesInDisorder) {
  std::unique_ptr<CPDF_TestDocumentForPages> document =
      pdfium::MakeUnique<CPDF_TestDocumentForPages>();

  CPDF_Dictionary* page = document->GetPage(1);
  ASSERT_TRUE(page);
  ASSERT_TRUE(page->KeyExist("PageNumbering"));
  EXPECT_EQ(1, page->GetIntegerFor("PageNumbering"));

  page = document->GetPage(3);
  ASSERT_TRUE(page);
  ASSERT_TRUE(page->KeyExist("PageNumbering"));
  EXPECT_EQ(3, page->GetIntegerFor("PageNumbering"));

  page = document->GetPage(7);
  EXPECT_FALSE(page);

  page = document->GetPage(6);
  ASSERT_TRUE(page);
  ASSERT_TRUE(page->KeyExist("PageNumbering"));
  EXPECT_EQ(6, page->GetIntegerFor("PageNumbering"));
}

TEST_F(cpdf_document_test, UseCachedPageObjNumIfHaveNotPagesDict) {
  // ObjNum can be added in CPDF_DataAvail::IsPageAvail, and PagesDict
  // can be not exists in this case.
  // (case, when hint table is used to page check in CPDF_DataAvail).
  CPDF_Document document(pdfium::MakeUnique<CPDF_Parser>());
  auto dict = pdfium::MakeUnique<CPDF_Dictionary>();
  dict->SetNewFor<CPDF_Boolean>("Linearized", true);
  const int page_count = 100;
  dict->SetNewFor<CPDF_Number>("N", page_count);
  TestLinearized linearized(dict.get());
  document.LoadLinearizedDoc(&linearized);
  ASSERT_EQ(page_count, document.GetPageCount());
  CPDF_Object* page_stub = document.NewIndirect<CPDF_Dictionary>();
  const uint32_t obj_num = page_stub->GetObjNum();
  const int test_page_num = 33;

  EXPECT_FALSE(document.IsPageLoaded(test_page_num));
  EXPECT_EQ(nullptr, document.GetPage(test_page_num));

  document.SetPageObjNum(test_page_num, obj_num);
  EXPECT_TRUE(document.IsPageLoaded(test_page_num));
  EXPECT_EQ(page_stub, document.GetPage(test_page_num));
}
