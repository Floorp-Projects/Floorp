/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "enctool.h"
#include "argparse.h"
#include "util.h"

#include "nss.h"

#include <assert.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

void EncTool::PrintError(const std::string& m, size_t line_number) {
  std::cerr << m << " - enctool.cc:" << line_number << std::endl;
}

void EncTool::PrintError(const std::string& m, PRErrorCode err,
                         size_t line_number) {
  std::cerr << m << " (error " << err << ")"
            << " - enctool.cc:" << line_number << std::endl;
}

void EncTool::PrintBytes(const std::vector<uint8_t>& bytes,
                         const std::string& txt) {
  if (debug_) {
    std::cerr << txt << ": ";
    for (uint8_t b : bytes) {
      std::cerr << std::setfill('0') << std::setw(2) << std::hex
                << static_cast<int>(b);
    }
    std::cerr << std::endl << std::dec;
  }
}

std::vector<uint8_t> EncTool::GenerateRandomness(size_t num_bytes) {
  std::vector<uint8_t> bytes(num_bytes);
  if (PK11_GenerateRandom(bytes.data(), num_bytes) != SECSuccess) {
    PrintError("No randomness available. Abort!", __LINE__);
    exit(1);
  }
  return bytes;
}

bool EncTool::WriteBytes(const std::vector<uint8_t>& bytes,
                         std::string out_file) {
  std::fstream output(out_file, std::ios::out | std::ios::binary);
  if (!output.good()) {
    return false;
  }
  output.write(reinterpret_cast<const char*>(
                   const_cast<const unsigned char*>(bytes.data())),
               bytes.size());
  output.flush();
  output.close();
  return true;
}

bool EncTool::GetKey(const std::vector<uint8_t>& key_bytes,
                     ScopedSECItem& key_item) {
  if (key_bytes.empty()) {
    return false;
  }

  // Build key.
  key_item =
      ScopedSECItem(SECITEM_AllocItem(nullptr, nullptr, key_bytes.size()));
  if (!key_item) {
    return false;
  }
  key_item->type = siBuffer;
  memcpy(key_item->data, key_bytes.data(), key_bytes.size());
  key_item->len = key_bytes.size();

  return true;
}

bool EncTool::GetAesGcmKey(const std::vector<uint8_t>& aad,
                           const std::vector<uint8_t>& iv_bytes,
                           const std::vector<uint8_t>& key_bytes,
                           ScopedSECItem& aes_key, ScopedSECItem& params) {
  if (iv_bytes.empty()) {
    return false;
  }

  // GCM params.
  CK_GCM_PARAMS* gcm_params =
      static_cast<CK_GCM_PARAMS*>(PORT_Malloc(sizeof(struct CK_GCM_PARAMS)));
  if (!gcm_params) {
    return false;
  }

  uint8_t* iv = static_cast<uint8_t*>(PORT_Malloc(iv_bytes.size()));
  if (!iv) {
    return false;
  }
  memcpy(iv, iv_bytes.data(), iv_bytes.size());
  gcm_params->pIv = iv;
  gcm_params->ulIvLen = iv_bytes.size();
  gcm_params->ulTagBits = 128;
  if (aad.empty()) {
    gcm_params->pAAD = nullptr;
    gcm_params->ulAADLen = 0;
  } else {
    uint8_t* ad = static_cast<uint8_t*>(PORT_Malloc(aad.size()));
    if (!ad) {
      return false;
    }
    memcpy(ad, aad.data(), aad.size());
    gcm_params->pAAD = ad;
    gcm_params->ulAADLen = aad.size();
  }

  params =
      ScopedSECItem(SECITEM_AllocItem(nullptr, nullptr, sizeof(*gcm_params)));
  if (!params) {
    return false;
  }
  params->len = sizeof(*gcm_params);
  params->type = siBuffer;
  params->data = reinterpret_cast<unsigned char*>(gcm_params);

  return GetKey(key_bytes, aes_key);
}

bool EncTool::GenerateAesGcmKey(const std::vector<uint8_t>& aad,
                                ScopedSECItem& aes_key, ScopedSECItem& params) {
  size_t key_size = 16, iv_size = 12;
  std::vector<uint8_t> iv_bytes = GenerateRandomness(iv_size);
  PrintBytes(iv_bytes, "IV");
  std::vector<uint8_t> key_bytes = GenerateRandomness(key_size);
  PrintBytes(key_bytes, "key");
  // Maybe write out the key and parameters.
  if (write_key_ && !WriteBytes(key_bytes, key_file_)) {
    return false;
  }
  if (write_iv_ && !WriteBytes(iv_bytes, iv_file_)) {
    return false;
  }
  return GetAesGcmKey(aad, iv_bytes, key_bytes, aes_key, params);
}

bool EncTool::ReadAesGcmKey(const std::vector<uint8_t>& aad,
                            ScopedSECItem& aes_key, ScopedSECItem& params) {
  std::vector<uint8_t> iv_bytes = ReadInputData(iv_file_);
  PrintBytes(iv_bytes, "IV");
  std::vector<uint8_t> key_bytes = ReadInputData(key_file_);
  PrintBytes(key_bytes, "key");
  return GetAesGcmKey(aad, iv_bytes, key_bytes, aes_key, params);
}

bool EncTool::GetChachaKey(const std::vector<uint8_t>& aad,
                           const std::vector<uint8_t>& iv_bytes,
                           const std::vector<uint8_t>& key_bytes,
                           ScopedSECItem& chacha_key, ScopedSECItem& params) {
  if (iv_bytes.empty()) {
    return false;
  }

  // AEAD params.
  CK_NSS_AEAD_PARAMS* aead_params = static_cast<CK_NSS_AEAD_PARAMS*>(
      PORT_Malloc(sizeof(struct CK_NSS_AEAD_PARAMS)));
  if (!aead_params) {
    return false;
  }

  uint8_t* iv = static_cast<uint8_t*>(PORT_Malloc(iv_bytes.size()));
  if (!iv) {
    return false;
  }
  memcpy(iv, iv_bytes.data(), iv_bytes.size());
  aead_params->pNonce = iv;
  aead_params->ulNonceLen = iv_bytes.size();
  aead_params->ulTagLen = 16;
  if (aad.empty()) {
    aead_params->pAAD = nullptr;
    aead_params->ulAADLen = 0;
  } else {
    uint8_t* ad = static_cast<uint8_t*>(PORT_Malloc(aad.size()));
    if (!ad) {
      return false;
    }
    memcpy(ad, aad.data(), aad.size());
    aead_params->pAAD = ad;
    aead_params->ulAADLen = aad.size();
  }

  params =
      ScopedSECItem(SECITEM_AllocItem(nullptr, nullptr, sizeof(*aead_params)));
  if (!params) {
    return false;
  }
  params->len = sizeof(*aead_params);
  params->type = siBuffer;
  params->data = reinterpret_cast<unsigned char*>(aead_params);

  return GetKey(key_bytes, chacha_key);
}

bool EncTool::GenerateChachaKey(const std::vector<uint8_t>& aad,
                                ScopedSECItem& chacha_key,
                                ScopedSECItem& params) {
  size_t key_size = 32, iv_size = 12;
  std::vector<uint8_t> iv_bytes = GenerateRandomness(iv_size);
  PrintBytes(iv_bytes, "IV");
  std::vector<uint8_t> key_bytes = GenerateRandomness(key_size);
  PrintBytes(key_bytes, "key");
  // Maybe write out the key and parameters.
  if (write_key_ && !WriteBytes(key_bytes, key_file_)) {
    return false;
  }
  if (write_iv_ && !WriteBytes(iv_bytes, iv_file_)) {
    return false;
  }
  return GetChachaKey(aad, iv_bytes, key_bytes, chacha_key, params);
}

bool EncTool::ReadChachaKey(const std::vector<uint8_t>& aad,
                            ScopedSECItem& chacha_key, ScopedSECItem& params) {
  std::vector<uint8_t> iv_bytes = ReadInputData(iv_file_);
  PrintBytes(iv_bytes, "IV");
  std::vector<uint8_t> key_bytes = ReadInputData(key_file_);
  PrintBytes(key_bytes, "key");
  return GetChachaKey(aad, iv_bytes, key_bytes, chacha_key, params);
}

bool EncTool::DoCipher(std::string file_name, std::string out_file,
                       bool encrypt, key_func_t get_params) {
  SECStatus rv;
  unsigned int outLen = 0, chunkSize = 1024;
  char buffer[1040];
  const unsigned char* bufferStart =
      reinterpret_cast<const unsigned char*>(buffer);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    PrintError("Unable to find security device", PR_GetError(), __LINE__);
    return false;
  }

  ScopedSECItem key, params;
  if (!(this->*get_params)(std::vector<uint8_t>(), key, params)) {
    PrintError("Geting keys and params failed.", __LINE__);
    return false;
  }

  ScopedPK11SymKey symKey(
      PK11_ImportSymKey(slot.get(), cipher_mech_, PK11_OriginUnwrap,
                        CKA_DECRYPT | CKA_ENCRYPT, key.get(), nullptr));
  if (!symKey) {
    PrintError("Failure to import key into NSS", PR_GetError(), __LINE__);
    return false;
  }

  std::streambuf* buf;
  std::ofstream output_file(out_file, std::ios::out | std::ios::binary);
  if (!out_file.empty()) {
    if (!output_file.good()) {
      return false;
    }
    buf = output_file.rdbuf();
  } else {
    buf = std::cout.rdbuf();
  }
  std::ostream output(buf);

  // Read from stdin.
  if (file_name.empty()) {
    std::vector<uint8_t> data = ReadInputData("");
    std::vector<uint8_t> out(data.size() + 16);
    if (encrypt) {
      rv = PK11_Encrypt(symKey.get(), cipher_mech_, params.get(), out.data(),
                        &outLen, data.size() + 16, data.data(), data.size());
    } else {
      rv = PK11_Decrypt(symKey.get(), cipher_mech_, params.get(), out.data(),
                        &outLen, data.size() + 16, data.data(), data.size());
    }
    if (rv != SECSuccess) {
      PrintError(encrypt ? "Error encrypting" : "Error decrypting",
                 PR_GetError(), __LINE__);
      return false;
    };
    output.write(reinterpret_cast<char*>(out.data()), outLen);
    output.flush();
    if (output_file.good()) {
      output_file.close();
    } else {
      output << std::endl;
    }

    std::cerr << "Done " << (encrypt ? "encrypting" : "decrypting")
              << std::endl;
    return true;
  }

  // Read file from file_name.
  std::ifstream input(file_name, std::ios::binary);
  if (!input.good()) {
    return false;
  }
  uint8_t out[1040];
  while (input) {
    if (encrypt) {
      input.read(buffer, chunkSize);
      rv = PK11_Encrypt(symKey.get(), cipher_mech_, params.get(), out, &outLen,
                        chunkSize + 16, bufferStart, input.gcount());
    } else {
      // We have to read the tag when decrypting.
      input.read(buffer, chunkSize + 16);
      rv = PK11_Decrypt(symKey.get(), cipher_mech_, params.get(), out, &outLen,
                        chunkSize + 16, bufferStart, input.gcount());
    }
    if (rv != SECSuccess) {
      PrintError(encrypt ? "Error encrypting" : "Error decrypting",
                 PR_GetError(), __LINE__);
      return false;
    };
    output.write(reinterpret_cast<const char*>(out), outLen);
    output.flush();
  }
  if (output_file.good()) {
    output_file.close();
  } else {
    output << std::endl;
  }
  std::cerr << "Done " << (encrypt ? "encrypting" : "decrypting") << std::endl;

  return true;
}

size_t EncTool::PrintFileSize(std::string file_name) {
  std::ifstream input(file_name, std::ifstream::ate | std::ifstream::binary);
  auto size = input.tellg();
  std::cerr << "Size of file to encrypt: " << size / 1024 / 1024 << " MB"
            << std::endl;
  return size;
}

bool EncTool::IsValidCommand(ArgParser arguments) {
  // Either encrypt or decrypt is fine.
  bool valid = arguments.Has("--encrypt") != arguments.Has("--decrypt");
  // An input file is required for decryption only.
  valid &= arguments.Has("--in") || arguments.Has("--encrypt");
  // An output file is required for encryption only.
  valid &= arguments.Has("--out") || arguments.Has("--decrypt");
  // Files holding the IV and key are required for decryption.
  valid &= arguments.Has("--iv") || arguments.Has("--encrypt");
  valid &= arguments.Has("--key") || arguments.Has("--encrypt");
  // Cipher is always required.
  valid &= arguments.Has("--cipher");
  return valid;
}

bool EncTool::Run(const std::vector<std::string>& arguments) {
  ArgParser parser(arguments);

  if (!IsValidCommand(parser)) {
    Usage();
    return false;
  }

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    PrintError("NSS initialization failed", PR_GetError(), __LINE__);
    return false;
  }

  if (parser.Has("--debug")) {
    debug_ = 1;
  }
  if (parser.Has("--iv")) {
    iv_file_ = parser.Get("--iv");
  } else {
    write_iv_ = false;
  }
  if (parser.Has("--key")) {
    key_file_ = parser.Get("--key");
  } else {
    write_key_ = false;
  }

  key_func_t get_params;
  bool encrypt = parser.Has("--encrypt");
  if (parser.Get("--cipher") == kAESCommand) {
    cipher_mech_ = CKM_AES_GCM;
    if (encrypt) {
      get_params = &EncTool::GenerateAesGcmKey;
    } else {
      get_params = &EncTool::ReadAesGcmKey;
    }
  } else if (parser.Get("--cipher") == kChaChaCommand) {
    cipher_mech_ = CKM_NSS_CHACHA20_POLY1305;
    if (encrypt) {
      get_params = &EncTool::GenerateChachaKey;
    } else {
      get_params = &EncTool::ReadChachaKey;
    }
  } else {
    Usage();
    return false;
  }
  // Don't write out key and iv when decrypting.
  if (!encrypt) {
    write_key_ = false;
    write_iv_ = false;
  }

  std::string input_file = parser.Has("--in") ? parser.Get("--in") : "";
  std::string output_file = parser.Has("--out") ? parser.Get("--out") : "";
  size_t file_size = 0;
  if (!input_file.empty()) {
    file_size = PrintFileSize(input_file);
  }
  auto begin = std::chrono::high_resolution_clock::now();
  if (!DoCipher(input_file, output_file, encrypt, get_params)) {
    (void)NSS_Shutdown();
    return false;
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto ns =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
  auto seconds = ns / 1000000000;
  std::cerr << ns << " ns (~" << seconds << " s) and " << std::endl;
  std::cerr << "That's approximately " << (double)file_size / ns << " b/ns"
            << std::endl;

  if (NSS_Shutdown() != SECSuccess) {
    return false;
  }

  return true;
}

void EncTool::Usage() {
  std::string const txt = R"~(
Usage: nss encrypt|decrypt --cipher aes|chacha [--in <file>] [--out <file>]
           [--key <file>] [--iv <file>]

    --cipher         Set the cipher to use.
                     --cipher aes:    Use AES-GCM to encrypt/decrypt.
                     --cipher chacha: Use ChaCha20/Poly1305 to encrypt/decrypt.
    --in             The file to encrypt/decrypt. If no file is given, we read
                     from stdin (only when encrypting).
    --out            The file to write the ciphertext/plaintext to. If no file
                     is given we write the plaintext to stdout (only when
                     decrypting).
    --key            The file to write the used key to/to read the key
                     from. Optional parameter. When not given, don't write out
                     the key.
    --iv             The file to write the used IV to/to read the IV
                     from. Optional parameter. When not given, don't write out
                     the IV.

    Examples:
        nss encrypt --cipher aes --iv iv --key key --out ciphertext
        nss decrypt --cipher chacha --iv iv --key key --in ciphertex

    Note: This tool overrides files without asking.
)~";
  std::cerr << txt << std::endl;
}
