/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dbtool_h__
#define dbtool_h__

#include <string>
#include <vector>
#include "argparse.h"
#include "tool.h"

class DBTool : public Tool {
 public:
  bool Run(const std::vector<std::string>& arguments) override;

 private:
  void Usage() override;
  bool PathHasDBFiles(std::string path);
  void ListCertificates();
  bool ImportCertificate(const ArgParser& parser);
  bool ListKeys();
  bool ImportKey(const ArgParser& parser);
  bool DeleteCert(const ArgParser& parser);
  bool DeleteKey(const ArgParser& parser);
};

#endif  // dbtool_h__
