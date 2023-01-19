/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "config.h"

#include <cstdlib>
#include <queue>
#include <string>

bool ConfigEntryBase::ParseInternal(std::queue<const char *> &args,
                                    std::vector<int> &out) {
  if (args.empty()) return false;

  char *endptr;
  out.push_back(strtol(args.front(), &endptr, 10));
  args.pop();

  return !*endptr;
}

bool ConfigEntryBase::ParseInternal(std::queue<const char *> &args,
                                    std::string &out) {
  if (args.empty()) return false;
  out = args.front();
  args.pop();
  return true;
}

bool ConfigEntryBase::ParseInternal(std::queue<const char *> &args, int &out) {
  if (args.empty()) return false;

  char *endptr;
  out = strtol(args.front(), &endptr, 10);
  args.pop();

  return !*endptr;
}

bool ConfigEntryBase::ParseInternal(std::queue<const char *> &args, bool &out) {
  out = true;
  return true;
}

std::string Config::XformFlag(const std::string &arg) {
  if (arg.empty()) return "";

  if (arg[0] != '-') return "";

  return arg.substr(1);
}

Config::Status Config::ParseArgs(int argc, char **argv) {
  std::queue<const char *> args;
  for (int i = 1; i < argc; ++i) {
    args.push(argv[i]);
  }
  while (!args.empty()) {
    auto e = entries_.find(XformFlag(args.front()));
    if (e == entries_.end()) {
      std::cerr << "Unimplemented shim flag: " << args.front() << std::endl;
      return kUnknownFlag;
    }
    args.pop();
    if (!e->second->Parse(args)) return kMalformedArgument;
  }

  return kOK;
}
