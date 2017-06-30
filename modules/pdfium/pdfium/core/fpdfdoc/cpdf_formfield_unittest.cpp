// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_indirect_object_holder.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "core/fpdfdoc/cpdf_formfield.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(cpdf_formfield, FPDF_GetFullName) {
  CFX_WideString name = FPDF_GetFullName(nullptr);
  EXPECT_TRUE(name.IsEmpty());

  CPDF_IndirectObjectHolder obj_holder;
  CPDF_Dictionary* root = obj_holder.NewIndirect<CPDF_Dictionary>();
  root->SetNewFor<CPDF_Name>("T", "foo");
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("foo", name.UTF8Encode().c_str());

  CPDF_Dictionary* dict1 = obj_holder.NewIndirect<CPDF_Dictionary>();
  root->SetNewFor<CPDF_Reference>("Parent", &obj_holder, dict1->GetObjNum());
  dict1->SetNewFor<CPDF_Name>("T", "bar");
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("bar.foo", name.UTF8Encode().c_str());

  CPDF_Dictionary* dict2 = dict1->SetNewFor<CPDF_Dictionary>("Parent");
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("bar.foo", name.UTF8Encode().c_str());

  CPDF_Dictionary* dict3 = obj_holder.NewIndirect<CPDF_Dictionary>();
  dict2->SetNewFor<CPDF_Reference>("Parent", &obj_holder, dict3->GetObjNum());

  dict3->SetNewFor<CPDF_Name>("T", "qux");
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("qux.bar.foo", name.UTF8Encode().c_str());

  dict3->SetNewFor<CPDF_Reference>("Parent", &obj_holder, root->GetObjNum());
  name = FPDF_GetFullName(root);
  EXPECT_STREQ("qux.bar.foo", name.UTF8Encode().c_str());
  name = FPDF_GetFullName(dict1);
  EXPECT_STREQ("foo.qux.bar", name.UTF8Encode().c_str());
  name = FPDF_GetFullName(dict2);
  EXPECT_STREQ("bar.foo.qux", name.UTF8Encode().c_str());
  name = FPDF_GetFullName(dict3);
  EXPECT_STREQ("bar.foo.qux", name.UTF8Encode().c_str());
}
