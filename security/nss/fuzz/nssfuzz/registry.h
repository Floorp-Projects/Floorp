/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef registry_h__
#define registry_h__

#include <map>
#include "nss.h"
#include "FuzzerInternal.h"

class Registry {
 public:
  static void Add(std::string name, fuzzer::UserCallback func,
                  uint16_t max_len, std::string desc) {
    assert(!Has(name));
    GetInstance().targets_[name] = TargetData(func, max_len, desc);
  }

  static bool Has(std::string name) {
    return GetInstance().targets_.count(name) > 0;
  }

  static fuzzer::UserCallback Func(std::string name) {
    assert(Has(name));
    return std::get<0>(Get(name));
  }

  static uint16_t MaxLen(std::string name) {
    assert(Has(name));
    return std::get<1>(Get(name));
  }

  static std::string& Desc(std::string name) {
    assert(Has(name));
    return std::get<2>(Get(name));
  }

  static std::vector<std::string> Names() {
    std::vector<std::string> names;
    for (auto &it : GetInstance().targets_) {
      names.push_back(it.first);
    }
    return names;
  }

 private:
  typedef std::tuple<fuzzer::UserCallback, uint16_t, std::string> TargetData;

  static Registry& GetInstance() {
    static Registry registry;
    return registry;
  }

  static TargetData& Get(std::string name) {
    return GetInstance().targets_[name];
  }

  Registry() {}

  std::map<std::string, TargetData> targets_;
};

#define REGISTER_FUZZING_TARGET(name, func, max_len, desc)            \
  static void __attribute__ ((constructor)) RegisterFuzzingTarget() { \
    Registry::Add(name, func, max_len, desc);                         \
  }

#endif // registry_h__
