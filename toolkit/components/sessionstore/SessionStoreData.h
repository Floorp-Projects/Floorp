/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreData_h
#define mozilla_dom_SessionStoreData_h

#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/dom/SessionStoreUtilsBinding.h"
#include "mozilla/Variant.h"

typedef mozilla::Variant<nsString, bool,
                         mozilla::dom::CollectedNonMultipleSelectValue,
                         CopyableTArray<nsString>>
    InputDataValue;

/*
 * Need two arrays based on this struct.
 * One is for elements with id one is for XPath.
 *
 * id:      id or XPath
 * type:    type of this input element
 *        bool:   value is boolean
 *        string: value is nsString
 *        file:   value is "arrayVal"
 *        singleSelect:   value is "singleSelect"
 *        multipleSelect: value is "arrayVal"
 *
 * There are four value types:
 * strVal:        nsString
 * boolVal:       boolean
 * singleSelect:  single select value
 * arrayVal:      nsString array
 */
struct CollectedInputDataValue {
  nsString id;
  nsString type;
  InputDataValue value{false};

  CollectedInputDataValue() = default;
};

/*
 * Each index of the following array is corresponging to each frame.
 * descendants: number of child frames of this frame
 * innerHTML:   innerHTML of this frame
 * url:         url of this frame
 * numId:       number of containing elements with id for this frame
 * numXPath:    number of containing elements with XPath for this frame
 */
struct InputFormData {
  int32_t descendants;
  nsString innerHTML;
  nsCString url;
  int32_t numId;
  int32_t numXPath;
};

#endif /* mozilla_dom_SessionStoreData_h */
