/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iomanip>
#include <iostream>
#include <memory>

#include "keyhi.h"
#include "pk11pub.h"

#include "FuzzerInternal.h"
#include "registry.h"
#include "shared.h"

using namespace std;

void printUsage(const vector<string> &args) {
  size_t sep = args.at(0).rfind("/") + 1;
  string progName = args.at(0).substr(sep);

  cerr << progName << " - Various libFuzzer targets for NSS" << endl << endl;
  cerr << "Usage: " << progName << " <target> <libFuzzer options>" << endl
       << endl;
  cerr << "Valid targets:" << endl;

  vector<string> names = Registry::Names();

  // Find length of the longest name.
  size_t name_w =
      max_element(names.begin(), names.end(), [](string &a, string &b) {
        return a.size() < b.size();
      })->size();

  // Find length of the longest description.
  auto max = max_element(names.begin(), names.end(), [](string &a, string &b) {
    return Registry::Desc(a).size() < Registry::Desc(b).size();
  });
  size_t desc_w = Registry::Desc(*max).size();

  // Print list of targets.
  for (string name : names) {
    cerr << "  " << left << setw(name_w) << name << " - " << setw(desc_w)
         << Registry::Desc(name)
         << " [default max_len=" << Registry::MaxLen(name) << "]" << endl;
  }

  // Some usage examples.
  cerr << endl << "Run fuzzer with a given corpus directory:" << endl;
  cerr << "  " << progName << " <target> /path/to/corpus" << endl;

  cerr << endl << "Run fuzzer with a single test input:" << endl;
  cerr << "  " << progName
       << " <target> ./crash-14d4355b971092e39572bc306a135ddf9f923e19" << endl;

  cerr << endl
       << "Specify the number of cores you wish to dedicate to fuzzing:"
       << endl;
  cerr << "  " << progName << " <target> -jobs=8 -workers=8 /path/to/corpus"
       << endl;

  cerr << endl << "Override the maximum length of a test input:" << endl;
  cerr << "  " << progName << " <target> -max_len=2048 /path/to/corpus" << endl;

  cerr << endl
       << "Minimize a given corpus and put the result into 'new_corpus':"
       << endl;
  cerr << "  " << progName
       << " <target> -merge=1 -max_len=50000 ./new_corpus /path/to/corpus"
       << endl;

  cerr << endl << "Merge new test inputs into a corpus:" << endl;
  cerr
      << "  " << progName
      << " <target> -merge=1 -max_len=50000 /path/to/corpus ./inputs1 ./inputs2"
      << endl;

  cerr << endl << "Print libFuzzer usage information:" << endl;
  cerr << "  " << progName << " <target> -help=1" << endl << endl;

  cerr << "Check out the docs at http://llvm.org/docs/LibFuzzer.html" << endl;
}

int main(int argc, char **argv) {
  vector<string> args(argv, argv + argc);

  if (args.size() < 2 || !Registry::Has(args[1])) {
    printUsage(args);
    return 1;
  }

  string targetName = args.at(1);
  uint16_t maxLen = Registry::MaxLen(targetName);
  string maxLenArg = "-max_len=" + to_string(maxLen);

  auto find = [](string &a) {
    return a.find("-max_len=") == 0 || a.find("-merge=1") == 0;
  };

  if (any_of(args.begin(), args.end(), find)) {
    // Remove the 2nd argument.
    argv[1] = argv[0];
    argv++;
    argc--;
  } else {
    // Set default max_len arg, if none given and we're not merging.
    argv[1] = const_cast<char *>(maxLenArg.c_str());
  }

  // Hand control to the libFuzzer driver.
  return fuzzer::FuzzerDriver(&argc, &argv, Registry::Func(targetName));
}
