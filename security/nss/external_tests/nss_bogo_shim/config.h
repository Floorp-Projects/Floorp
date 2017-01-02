/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Generic command line flags system for NSS BoGo shim.  This class
// could actually in principle handle other programs. The flags are
// defined in the consumer code.

#ifndef config_h_
#define config_h_

#include <cassert>

#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <typeinfo>

// Abstract base class for a given config flag.
class ConfigEntryBase {
 public:
  ConfigEntryBase(const std::string& name, const std::string& type)
      : name_(name), type_(type) {}

  const std::string& type() const { return type_; }
  virtual bool Parse(std::queue<const char*>* args) = 0;

 protected:
  bool ParseInternal(std::queue<const char*>* args, std::string* out);
  bool ParseInternal(std::queue<const char*>* args, int* out);
  bool ParseInternal(std::queue<const char*>* args, bool* out);

  const std::string name_;
  const std::string type_;
};

// Template specializations for the concrete flag types.
template <typename T>
class ConfigEntry : public ConfigEntryBase {
 public:
  ConfigEntry(const std::string& name, T init)
      : ConfigEntryBase(name, typeid(T).name()), value_(init) {}
  T get() const { return value_; }

  bool Parse(std::queue<const char*>* args) {
    return ParseInternal(args, &value_);
  }

 private:
  T value_;
};

// The overall configuration (I.e., the total set of flags).
class Config {
 public:
  enum Status { kOK, kUnknownFlag, kMalformedArgument, kMissingValue };

  Config() : entries_() {}

  template <typename T>
  void AddEntry(const std::string& name, T init) {
    entries_[name] = new ConfigEntry<T>(name, init);
  }

  Status ParseArgs(int argc, char** argv);

  template <typename T>
  T get(const std::string& key) const {
    auto e = entry(key);
    assert(e->type() == typeid(T).name());
    return static_cast<const ConfigEntry<T>*>(e)->get();
  }

 private:
  static std::string XformFlag(const std::string& arg);

  std::map<std::string, ConfigEntryBase*> entries_;

  const ConfigEntryBase* entry(const std::string& key) const {
    auto e = entries_.find(key);
    if (e == entries_.end()) return nullptr;
    return e->second;
  }
};

#endif  // config_h_
