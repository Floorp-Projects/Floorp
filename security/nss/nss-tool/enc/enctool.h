/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef enctool_h__
#define enctool_h__

#include <string>
#include <vector>
#include "argparse.h"
#include "nss_scoped_ptrs.h"
#include "prerror.h"
#include "tool.h"

class EncTool : public Tool {
 public:
  bool Run(const std::vector<std::string>& arguments) override;
  void Usage() override;

 private:
  typedef bool (EncTool::*key_func_t)(const std::vector<uint8_t>& aad,
                                      ScopedSECItem& chacha_key,
                                      ScopedSECItem& params);
  void PrintBytes(const std::vector<uint8_t>& bytes, const std::string& txt);
  bool WriteBytes(const std::vector<uint8_t>& bytes, std::string out_file);
  void PrintError(const std::string& m, PRErrorCode err, size_t line_number);
  void PrintError(const std::string& m, size_t line_number);
  bool GetKey(const std::vector<uint8_t>& key_bytes, ScopedSECItem& key_item);
  bool GetAesGcmKey(const std::vector<uint8_t>& aad,
                    const std::vector<uint8_t>& iv_bytes,
                    const std::vector<uint8_t>& key_bytes,
                    ScopedSECItem& aes_key, ScopedSECItem& params);
  bool GetChachaKey(const std::vector<uint8_t>& aad,
                    const std::vector<uint8_t>& iv_bytes,
                    const std::vector<uint8_t>& key_bytes,
                    ScopedSECItem& chacha_key, ScopedSECItem& params);
  bool GenerateAesGcmKey(const std::vector<uint8_t>& aad,
                         ScopedSECItem& aes_key, ScopedSECItem& params);
  bool ReadAesGcmKey(const std::vector<uint8_t>& aad, ScopedSECItem& aes_key,
                     ScopedSECItem& params);
  std::vector<uint8_t> GenerateRandomness(size_t num_bytes);
  bool GenerateChachaKey(const std::vector<uint8_t>& aad,
                         ScopedSECItem& chacha_key, ScopedSECItem& params);
  bool ReadChachaKey(const std::vector<uint8_t>& aad, ScopedSECItem& chacha_key,
                     ScopedSECItem& params);
  bool DoCipher(std::string fileName, std::string outFile, bool encrypt,
                key_func_t get_params);
  size_t PrintFileSize(std::string fileName);
  bool IsValidCommand(ArgParser arguments);

  bool debug_ = false;
  bool write_key_ = true;
  bool write_iv_ = true;
  std::string key_file_ = "/tmp/key";
  std::string iv_file_ = "/tmp/iv";
  CK_MECHANISM_TYPE cipher_mech_;

  const std::string kAESCommand = "aes";
  const std::string kChaChaCommand = "chacha";
};

#endif  // enctool_h__
