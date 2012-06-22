// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_MULTIPROCESS_FUNC_LIST_H_
#define TESTING_MULTIPROCESS_FUNC_LIST_H_

#include <string>

// This file provides the plumbing to register functions to be executed
// as the main function of a child process in a multi-process test.
// This complements the MultiProcessTest class which provides facilities
// for launching such tests.
//
// The MULTIPROCESS_TEST_MAIN() macro registers a string -> func_ptr mapping
// by creating a new global instance of the AppendMultiProcessTest() class
// this means that by the time that we reach our main() function the mapping
// is already in place.
//
// Example usage:
//  MULTIPROCESS_TEST_MAIN(a_test_func) {
//    // Code here runs in a child process.
//    return 0;
//  }
//
// The prototype of a_test_func is implicitly
//   int test_main_func_name();

namespace multi_process_function_list {

// Type for child process main functions.
typedef int (*ChildFunctionPtr)();

// Helper class to append a test function to the global mapping.
// Used by the MULTIPROCESS_TEST_MAIN macro.
class AppendMultiProcessTest {
 public:
  AppendMultiProcessTest(std::string test_name, ChildFunctionPtr func_ptr);
};

// Invoke the main function of a test previously registered with
// MULTIPROCESS_TEST_MAIN()
int InvokeChildProcessTest(std::string test_name);

// This macro creates a global MultiProcessTest::AppendMultiProcessTest object
// whose constructor does the work of adding the global mapping.
#define MULTIPROCESS_TEST_MAIN(test_main) \
  int test_main(); \
  namespace { \
    multi_process_function_list::AppendMultiProcessTest \
    AddMultiProcessTest##_##test_main(#test_main, (test_main)); \
  } \
  int test_main()

}  // namespace multi_process_function_list

#endif  // TESTING_MULTIPROCESS_FUNC_LIST_H_
