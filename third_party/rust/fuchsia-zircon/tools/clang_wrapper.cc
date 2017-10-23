// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wrap the clang toolchain to supply triple-specific configuration parameters
// as arguments.

// Gnu-style toolchains encode the target triple in the executable name (each
// executable is specialized to a single triple), and each toolchain also knows
// how to find the corresponding sysroot. By contrast, clang uses a single
// binary, and the target is specified as a command line argument. In addition,
// in the Fuchsia world, the sysroot is not bundled with the compiler.
//
// This wrapper infers the relevant configuration parameters from the command
// name used to invoke the wrapper, and also finds the sysroot at a known
// location relative to the jiri root, then invokes clang with the additional
// arguments.

#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>

using std::map;
using std::string;
using std::vector;

// Get the real path of this executable, resolving symbolic links.
string GetPath(const char* argv0) {
  char* path_c = realpath(argv0, nullptr);
  string path = string(path_c);
  free(path_c);
  return path;
}

// Return path of Jiri root, or empty string if not found.
string FindJiriRoot(const string& self_path) {
  string path = self_path;
  while (true) {
    size_t pos = path.rfind('/');
    if (pos == string::npos) {
      return "";
    }
    string basedir = path.substr(0, pos + 1);
    string trial = basedir + ".jiri_root";
    struct stat stat_buf;
    int status = stat(trial.c_str(), &stat_buf);
    if (status == 0) {
      return basedir;
    }
    path = path.substr(0, pos);
  }
}

// Get the basename of the command used to invoke this wrapper.
// Typical return value: "x86-64-unknown-fuchsia-cc"
string GetCmd(const char* argv0) {
  string cmd = argv0;
  size_t pos = cmd.rfind('/');
  if (pos != string::npos) {
    cmd = cmd.substr(pos + 1);
  }
  return cmd;
}

// Given the command basename, get the llvm target triple. Empty string on failure.
// Typical return value: "x86_64-unknown-fuchsia"
string TargetTriple(const string& cmd) {
  size_t pos = cmd.rfind('-');
  if (pos == string::npos) {
    return "";
  }
  string triple = cmd.substr(0, pos);
  if (triple.find("x86-64") == 0) {
    triple[3] = '_';
  }
  return triple;
}

// Given the path to the command, calculate the matching sysroot
string SysrootPath(const string& triple, const string& self_path) {
  const string out_dir_name = "out/";
  size_t pos = self_path.find(out_dir_name);
  if (pos != string::npos) {
    string out_path = self_path.substr(0, pos + out_dir_name.length());
    string zircon_name = triple == "x86_64-unknown-fuchsia" ?
      "build-zircon-pc-x86-64" : "build-zircon-qemu-arm64";
    string sysroot_path = out_path + "build-zircon/" + zircon_name + "/sysroot";
    struct stat stat_buf;
    int status = stat(sysroot_path.c_str(), &stat_buf);
    if (status == 0) {
      return sysroot_path;
    }
  }
  return "";
}

// Detect the host, get the host identifier (used to select a prebuilt toolchain).
// Empty string on failure.
// Typical return value: "mac-x64"
string HostDouble() {
  struct utsname name;
  int status = uname(&name);
  if (status != 0) {
    return "";
  }
  map<string, string> cpumap = {
    {"aarch64", "arm64"},
    {"x86_64", "x64"}
  };
  map<string, string> osmap = {
    {"Linux", "linux"},
    {"Darwin", "mac"}
  };
  return osmap[name.sysname] + "-" + cpumap[name.machine];
}

// Given the command baseline, get the llvm binary to invoke.
// Note: the "llvm-ar" special case should probably go away, as Rust no longer
// invokes an external "ar" tool, and the llvm-ar one probably wouldn't work.
// Typical return value: "clang"
string InferTool(const string& cmd) {
  size_t pos = cmd.rfind('-');
  string base;
  if (pos != string::npos) {
    base = cmd.substr(pos + 1);
  } else {
    base = cmd;
  }
  if (base == "cc" || base == "gcc") {
    return "clang";
  } else if (base == "ar") {
    return "llvm-ar";
  }
  return base;
}

// Collect C-style argc/argv into a vector of strings.
vector<string> CollectArgs(int argc, char** argv) {
  vector<string> result;
  for (int i = 0; i < argc; i++) {
    result.push_back(argv[i]);
  }
  return result;
}

// Do "execv" given command path as a string and args as a vector of strings.
int DoExecv(const string& cmd, vector<string>& args) {
  vector<const char*> argvec;
  for (auto& it : args) {
    argvec.push_back(it.c_str());
  }
  argvec.push_back(nullptr);
  char* const* argv = const_cast<char* const*>(argvec.data());
  return execv(cmd.c_str(), argv);
}

void Die(const string& message) {
  std::cerr << message << std::endl;
  exit(1);
}

int main(int argc, char** argv) {
  string self_path = GetPath(argv[0]);
  string root = FindJiriRoot(self_path);
  if (root.empty()) {
    Die("Can't find .jiri_root in any parent of " + self_path);
  }
  string host = HostDouble();
  if (host.empty()) {
    Die("Can't detect host (uname failed)");
  }
  string cmd = GetCmd(argv[0]);
  string tool = InferTool(cmd);
  string triple = TargetTriple(cmd);
  vector<string> args = CollectArgs(argc, argv);

  string newcmd = root + "buildtools/" + host + "/clang/bin/" + tool;
  string sysroot = SysrootPath(triple, self_path);
  if (sysroot.empty()) {
    Die("Can't find sysroot from wrapper path");
  }
  vector<string> newargs;
  newargs.push_back(newcmd);
  if (tool != "llvm-ar") {
    newargs.push_back("-target");
    newargs.push_back(triple);
    newargs.push_back("--sysroot=" + sysroot);
  }
  for (auto it = args.begin() + 1; it != args.end(); ++it) {
    newargs.push_back(*it);
  }
  int status = DoExecv(newcmd, newargs);
  if (status != 0) {
    Die("error invoking " + newcmd + ": " + strerror(errno));
  }
  return 0;
}
