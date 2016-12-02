/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef registry_h__
#define registry_h__

#include <map>
#include "FuzzerInternal.h"
#include "nss.h"

using namespace fuzzer;
using namespace std;

typedef decltype(LLVMFuzzerCustomMutator)* Mutator;

class Registry {
 public:
  static void Add(string name, UserCallback func, uint16_t max_len, string desc,
                  vector<Mutator> mutators = {}) {
    assert(!Has(name));
    GetInstance().targets_[name] = TargetData(func, max_len, desc, mutators);
  }

  static bool Has(string name) {
    return GetInstance().targets_.count(name) > 0;
  }

  static UserCallback Func(string name) {
    assert(Has(name));
    return get<0>(Get(name));
  }

  static uint16_t MaxLen(string name) {
    assert(Has(name));
    return get<1>(Get(name));
  }

  static string& Desc(string name) {
    assert(Has(name));
    return get<2>(Get(name));
  }

  static vector<Mutator>& Mutators(string name) {
    assert(Has(name));
    return get<3>(Get(name));
  }

  static vector<string> Names() {
    vector<string> names;
    for (auto& it : GetInstance().targets_) {
      names.push_back(it.first);
    }
    return names;
  }

 private:
  typedef tuple<UserCallback, uint16_t, string, vector<Mutator>> TargetData;

  static Registry& GetInstance() {
    static Registry registry;
    return registry;
  }

  static TargetData& Get(string name) { return GetInstance().targets_[name]; }

  Registry() {}

  map<string, TargetData> targets_;
};

#define REGISTER_FUZZING_TARGET(name, func, max_len, desc, ...) \
  static void __attribute__((constructor)) Register_##func() {  \
    Registry::Add(name, func, max_len, desc, __VA_ARGS__);      \
  }

#endif  // registry_h__
