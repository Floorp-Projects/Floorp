/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iomanip>
#include <iostream>

#include "FuzzerInternal.h"
#include "FuzzerMutate.h"
#include "FuzzerRandom.h"
#include "registry.h"
#include "shared.h"

using namespace std;

static vector<Mutator> gMutators;

class Args {
 public:
  Args(int argc, char **argv) : args_(argv, argv + argc) {}

  string &operator[](const int idx) { return args_[idx]; }

  bool Has(const string &arg) {
    return any_of(args_.begin(), args_.end(),
                  [&arg](string &a) { return a.find(arg) == 0; });
  }

  void Append(const string &arg) { args_.push_back(arg); }

  void Remove(const int index) {
    assert(index < count());
    args_.erase(args_.begin() + index);
  }

  vector<char *> argv() {
    vector<char *> out;
    out.resize(count());

    transform(args_.begin(), args_.end(), out.begin(),
              [](string &a) { return const_cast<char *>(a.c_str()); });

    return out;
  }

  size_t count() { return args_.size(); }

 private:
  vector<string> args_;
};

void printUsage(Args &args) {
  size_t sep = args[0].rfind("/") + 1;
  string progName = args[0].substr(sep);

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
  Args args(argc, argv);

  if (args.count() < 2 || !Registry::Has(args[1])) {
    printUsage(args);
    return 1;
  }

  string targetName(args[1]);

  // Add target mutators.
  auto mutators = Registry::Mutators(targetName);
  gMutators.insert(gMutators.end(), mutators.begin(), mutators.end());

  // Remove the target argument when -workers=x or -jobs=y is NOT given.
  // If both are given, libFuzzer will spawn multiple processes for the target.
  if (!args.Has("-workers=") || !args.Has("-jobs=")) {
    args.Remove(1);
  }

  // Set default max_len arg, if none given and we're not merging.
  if (!args.Has("-max_len=") && !args.Has("-merge=1")) {
    uint16_t maxLen = Registry::MaxLen(targetName);
    args.Append("-max_len=" + to_string(maxLen));
  }

  // Hand control to the libFuzzer driver.
  vector<char *> args_new(args.argv());
  argc = args_new.size();
  argv = args_new.data();

  return fuzzer::FuzzerDriver(&argc, &argv, Registry::Func(targetName));
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *Data, size_t Size,
                                          size_t MaxSize, unsigned int Seed) {
  if (gMutators.empty()) {
    return 0;
  }

  // Forward to a pseudorandom mutator.
  fuzzer::Random R(Seed);
  return gMutators.at(R(gMutators.size()))(Data, Size, MaxSize, Seed);
}
