/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <prinit.h>

#include "argparse.h"
#include "db/dbtool.h"
#include "digest/digesttool.h"
#include "enc/enctool.h"
#include "tool.h"

static void Usage() {
  std::cerr << "Usage: nss <command> <subcommand> [options]" << std::endl;
  std::cerr << "       nss db [--path <directory>] <commands>" << std::endl;
  std::cerr << "       nss encrypt <options>" << std::endl;
  std::cerr << "       nss decrypt <options>" << std::endl;
  std::cerr << "       nss digest <options>" << std::endl;
}

static const std::string kDbCommand = "db";
static const std::string kEncryptCommand = "encrypt";
static const std::string kDecryptCommand = "decrypt";
static const std::string kDigestCommand = "digest";

int main(int argc, char **argv) {
  if (argc < 2) {
    Usage();
    return 1;
  }
  std::vector<std::string> arguments(argv + 2, argv + argc);

  std::unique_ptr<Tool> tool = nullptr;
  if (argv[1] == kDbCommand) {
    tool = std::unique_ptr<Tool>(new DBTool());
  }
  if (argv[1] == kEncryptCommand) {
    tool = std::unique_ptr<Tool>(new EncTool());
    arguments.push_back("--encrypt");
  }
  if (argv[1] == kDecryptCommand) {
    tool = std::unique_ptr<Tool>(new EncTool());
    arguments.push_back("--decrypt");
  }
  if (argv[1] == kDigestCommand) {
    tool = std::unique_ptr<Tool>(new DigestTool());
  }
  if (!tool) {
    Usage();
    return 1;
  }

  int exit_code = 0;
  PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

  if (!tool->Run(arguments)) {
    exit_code = 1;
  }

  PR_Cleanup();

  return exit_code;
}
