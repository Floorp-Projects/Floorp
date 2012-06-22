// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "multiprocess_func_list.h"

#include <map>

// Helper functions to maintain mapping of "test name"->test func.
// The information is accessed via a global map.
namespace multi_process_function_list {

namespace {

typedef std::map<std::string, ChildFunctionPtr> MultiProcessTestMap;

// Retrieve a reference to the global 'func name' -> func ptr map.
MultiProcessTestMap &GetMultiprocessFuncMap() {
  static MultiProcessTestMap test_name_to_func_ptr_map;
  return test_name_to_func_ptr_map;
}

}  // namespace

AppendMultiProcessTest::AppendMultiProcessTest(std::string test_name,
                                               ChildFunctionPtr func_ptr) {
  GetMultiprocessFuncMap()[test_name] = func_ptr;
}

int InvokeChildProcessTest(std::string test_name) {
  MultiProcessTestMap &func_lookup_table = GetMultiprocessFuncMap();
  MultiProcessTestMap::iterator it = func_lookup_table.find(test_name);
  if (it != func_lookup_table.end()) {
    ChildFunctionPtr func = it->second;
    if (func) {
      return (*func)();
    }
  }

  return -1;
}

}  // namespace multi_process_function_list
