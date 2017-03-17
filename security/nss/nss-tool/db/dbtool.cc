/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "dbtool.h"
#include "argparse.h"
#include "scoped_ptrs.h"
#include "util.h"

#include <dirent.h>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

#include <cert.h>
#include <certdb.h>
#include <nss.h>
#include <pk11pub.h>
#include <prerror.h>
#include <prio.h>

const std::vector<std::string> kCommandArgs(
    {"--create", "--list-certs", "--import-cert", "--list-keys", "--import-key",
     "--delete-cert", "--delete-key", "--change-password"});

static bool HasSingleCommandArgument(const ArgParser &parser) {
  auto pred = [&](const std::string &cmd) { return parser.Has(cmd); };
  return std::count_if(kCommandArgs.begin(), kCommandArgs.end(), pred) == 1;
}

static bool HasArgumentRequiringWriteAccess(const ArgParser &parser) {
  return parser.Has("--create") || parser.Has("--import-cert") ||
         parser.Has("--import-key") || parser.Has("--delete-cert") ||
         parser.Has("--delete-key") || parser.Has("--change-password");
}

static std::string PrintFlags(unsigned int flags) {
  std::stringstream ss;
  if ((flags & CERTDB_VALID_CA) && !(flags & CERTDB_TRUSTED_CA) &&
      !(flags & CERTDB_TRUSTED_CLIENT_CA)) {
    ss << "c";
  }
  if ((flags & CERTDB_TERMINAL_RECORD) && !(flags & CERTDB_TRUSTED)) {
    ss << "p";
  }
  if (flags & CERTDB_TRUSTED_CA) {
    ss << "C";
  }
  if (flags & CERTDB_TRUSTED_CLIENT_CA) {
    ss << "T";
  }
  if (flags & CERTDB_TRUSTED) {
    ss << "P";
  }
  if (flags & CERTDB_USER) {
    ss << "u";
  }
  if (flags & CERTDB_SEND_WARN) {
    ss << "w";
  }
  if (flags & CERTDB_INVISIBLE_CA) {
    ss << "I";
  }
  if (flags & CERTDB_GOVT_APPROVED_CA) {
    ss << "G";
  }
  return ss.str();
}

static const char *const keyTypeName[] = {"null", "rsa", "dsa", "fortezza",
                                          "dh",   "kea", "ec"};

void DBTool::Usage() {
  std::cerr << "Usage: nss db [--path <directory>]" << std::endl;
  std::cerr << "  --create" << std::endl;
  std::cerr << "  --change-password" << std::endl;
  std::cerr << "  --list-certs" << std::endl;
  std::cerr << "  --import-cert [<path>] --name <name> [--trusts <trusts>]"
            << std::endl;
  std::cerr << "  --list-keys" << std::endl;
  std::cerr << "  --import-key [<path> [-- name <name>]]" << std::endl;
  std::cerr << "  --delete-cert <name>" << std::endl;
  std::cerr << "  --delete-key <name>" << std::endl;
}

bool DBTool::Run(const std::vector<std::string> &arguments) {
  ArgParser parser(arguments);

  if (!HasSingleCommandArgument(parser)) {
    Usage();
    return false;
  }

  PRAccessHow how = PR_ACCESS_READ_OK;
  bool readOnly = true;
  if (HasArgumentRequiringWriteAccess(parser)) {
    how = PR_ACCESS_WRITE_OK;
    readOnly = false;
  }

  std::string initDir(".");
  if (parser.Has("--path")) {
    initDir = parser.Get("--path");
  }
  if (PR_Access(initDir.c_str(), how) != PR_SUCCESS) {
    std::cerr << "Directory '" << initDir
              << "' does not exist or you don't have permissions!" << std::endl;
    return false;
  }

  std::cout << "Using database directory: " << initDir << std::endl
            << std::endl;

  bool dbFilesExist = PathHasDBFiles(initDir);
  if (parser.Has("--create") && dbFilesExist) {
    std::cerr << "Trying to create database files in a directory where they "
                 "already exists. Delete the db files before creating new ones."
              << std::endl;
    return false;
  }
  if (!parser.Has("--create") && !dbFilesExist) {
    std::cerr << "No db files found." << std::endl;
    std::cerr << "Create them using 'nss db --create [--path /foo/bar]' before "
                 "continuing."
              << std::endl;
    return false;
  }

  // init NSS
  const char *certPrefix = "";  // certutil -P option  --- can leave this empty
  SECStatus rv = NSS_Initialize(initDir.c_str(), certPrefix, certPrefix,
                                "secmod.db", readOnly ? NSS_INIT_READONLY : 0);
  if (rv != SECSuccess) {
    std::cerr << "NSS init failed!" << std::endl;
    return false;
  }

  bool ret = true;
  if (parser.Has("--list-certs")) {
    ListCertificates();
  } else if (parser.Has("--import-cert")) {
    ret = ImportCertificate(parser);
  } else if (parser.Has("--create")) {
    ret = InitSlotPassword();
    if (ret) {
      std::cout << "DB files created successfully." << std::endl;
    }
  } else if (parser.Has("--list-keys")) {
    ret = ListKeys();
  } else if (parser.Has("--import-key")) {
    ret = ImportKey(parser);
  } else if (parser.Has("--delete-cert")) {
    ret = DeleteCert(parser);
  } else if (parser.Has("--delete-key")) {
    ret = DeleteKey(parser);
  } else if (parser.Has("--change-password")) {
    ret = ChangeSlotPassword();
  }

  // shutdown nss
  if (NSS_Shutdown() != SECSuccess) {
    std::cerr << "NSS Shutdown failed!" << std::endl;
    return false;
  }

  return ret;
}

bool DBTool::PathHasDBFiles(std::string path) {
  std::regex certDBPattern("cert.*\\.db");
  std::regex keyDBPattern("key.*\\.db");

  DIR *dir;
  if (!(dir = opendir(path.c_str()))) {
    std::cerr << "Directory " << path << " could not be accessed!" << std::endl;
    return false;
  }

  struct dirent *ent;
  bool dbFileExists = false;
  while ((ent = readdir(dir))) {
    if (std::regex_match(ent->d_name, certDBPattern) ||
        std::regex_match(ent->d_name, keyDBPattern) ||
        "secmod.db" == std::string(ent->d_name)) {
      dbFileExists = true;
      break;
    }
  }

  closedir(dir);
  return dbFileExists;
}

void DBTool::ListCertificates() {
  ScopedCERTCertList list(PK11_ListCerts(PK11CertListAll, nullptr));
  CERTCertListNode *node;

  std::cout << std::setw(60) << std::left << "Certificate Nickname"
            << " "
            << "Trust Attributes" << std::endl;
  std::cout << std::setw(60) << std::left << ""
            << " "
            << "SSL,S/MIME,JAR/XPI" << std::endl
            << std::endl;

  for (node = CERT_LIST_HEAD(list); !CERT_LIST_END(node, list);
       node = CERT_LIST_NEXT(node)) {
    CERTCertificate *cert = node->cert;

    std::string name("(unknown)");
    char *appData = static_cast<char *>(node->appData);
    if (appData && strlen(appData) > 0) {
      name = appData;
    } else if (cert->nickname && strlen(cert->nickname) > 0) {
      name = cert->nickname;
    } else if (cert->emailAddr && strlen(cert->emailAddr) > 0) {
      name = cert->emailAddr;
    }

    CERTCertTrust trust;
    std::string trusts;
    if (CERT_GetCertTrust(cert, &trust) == SECSuccess) {
      std::stringstream ss;
      ss << PrintFlags(trust.sslFlags);
      ss << ",";
      ss << PrintFlags(trust.emailFlags);
      ss << ",";
      ss << PrintFlags(trust.objectSigningFlags);
      trusts = ss.str();
    } else {
      trusts = ",,";
    }
    std::cout << std::setw(60) << std::left << name << " " << trusts
              << std::endl;
  }
}

bool DBTool::ImportCertificate(const ArgParser &parser) {
  if (!parser.Has("--name")) {
    std::cerr << "A name (--name) is required to import a certificate."
              << std::endl;
    Usage();
    return false;
  }

  std::string derFilePath = parser.Get("--import-cert");
  std::string certName = parser.Get("--name");
  std::string trustString("TCu,Cu,Tu");
  if (parser.Has("--trusts")) {
    trustString = parser.Get("--trusts");
  }

  CERTCertTrust trust;
  SECStatus rv = CERT_DecodeTrustString(&trust, trustString.c_str());
  if (rv != SECSuccess) {
    std::cerr << "Cannot decode trust string!" << std::endl;
    return false;
  }

  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (slot.get() == nullptr) {
    std::cerr << "Error: Init PK11SlotInfo failed!" << std::endl;
    return false;
  }

  std::vector<char> certData = ReadInputData(derFilePath);

  ScopedCERTCertificate cert(
      CERT_DecodeCertFromPackage(certData.data(), certData.size()));
  if (cert.get() == nullptr) {
    std::cerr << "Error: Could not decode certificate!" << std::endl;
    return false;
  }

  rv = PK11_ImportCert(slot.get(), cert.get(), CK_INVALID_HANDLE,
                       certName.c_str(), PR_FALSE);
  if (rv != SECSuccess) {
    // TODO handle authentication -> PK11_Authenticate (see certutil.c line
    // 134)
    std::cerr << "Error: Could not add certificate to database!" << std::endl;
    return false;
  }

  rv = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert.get(), &trust);
  if (rv != SECSuccess) {
    std::cerr << "Cannot change cert's trust" << std::endl;
    return false;
  }

  std::cout << "Certificate import was successful!" << std::endl;
  // TODO show information about imported certificate
  return true;
}

bool DBTool::ListKeys() {
  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (slot.get() == nullptr) {
    std::cerr << "Error: Init PK11SlotInfo failed!" << std::endl;
    return false;
  }

  if (!DBLoginIfNeeded(slot)) {
    return false;
  }

  ScopedSECKEYPrivateKeyList list(PK11_ListPrivateKeysInSlot(slot.get()));
  if (list.get() == nullptr) {
    std::cerr << "Listing private keys failed with error "
              << PR_ErrorToName(PR_GetError()) << std::endl;
    return false;
  }

  SECKEYPrivateKeyListNode *node;
  int count = 0;
  for (node = PRIVKEY_LIST_HEAD(list.get());
       !PRIVKEY_LIST_END(node, list.get()); node = PRIVKEY_LIST_NEXT(node)) {
    char *keyNameRaw = PK11_GetPrivateKeyNickname(node->key);
    std::string keyName(keyNameRaw ? keyNameRaw : "");

    if (keyName.empty()) {
      ScopedCERTCertificate cert(PK11_GetCertFromPrivateKey(node->key));
      if (cert.get()) {
        if (cert->nickname && strlen(cert->nickname) > 0) {
          keyName = cert->nickname;
        } else if (cert->emailAddr && strlen(cert->emailAddr) > 0) {
          keyName = cert->emailAddr;
        }
      }
      if (keyName.empty()) {
        keyName = "(none)";  // default value
      }
    }

    SECKEYPrivateKey *key = node->key;
    ScopedSECItem keyIDItem(PK11_GetLowLevelKeyIDForPrivateKey(key));
    if (keyIDItem.get() == nullptr) {
      std::cerr << "Error: PK11_GetLowLevelKeyIDForPrivateKey failed!"
                << std::endl;
      continue;
    }

    std::string keyID = StringToHex(keyIDItem);

    if (count++ == 0) {
      // print header
      std::cout << std::left << std::setw(20) << "<key#, key name>"
                << std::setw(20) << "key type"
                << "key id" << std::endl;
    }

    std::stringstream leftElem;
    leftElem << "<" << count << ", " << keyName << ">";
    std::cout << std::left << std::setw(20) << leftElem.str() << std::setw(20)
              << keyTypeName[key->keyType] << keyID << std::endl;
  }

  if (count == 0) {
    std::cout << "No keys found." << std::endl;
  }

  return true;
}

bool DBTool::ImportKey(const ArgParser &parser) {
  std::string privKeyFilePath = parser.Get("--import-key");
  std::string name;
  if (parser.Has("--name")) {
    name = parser.Get("--name");
  }

  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (slot.get() == nullptr) {
    std::cerr << "Error: Init PK11SlotInfo failed!" << std::endl;
    return false;
  }

  if (!DBLoginIfNeeded(slot)) {
    return false;
  }

  std::vector<char> privKeyData = ReadInputData(privKeyFilePath);
  if (privKeyData.empty()) {
    return false;
  }
  SECItem pkcs8PrivKeyItem = {
      siBuffer, reinterpret_cast<unsigned char *>(privKeyData.data()),
      static_cast<unsigned int>(privKeyData.size())};

  SECItem nickname = {siBuffer, nullptr, 0};
  if (!name.empty()) {
    nickname.data = const_cast<unsigned char *>(
        reinterpret_cast<const unsigned char *>(name.c_str()));
    nickname.len = static_cast<unsigned int>(name.size());
  }

  SECStatus rv = PK11_ImportDERPrivateKeyInfo(
      slot.get(), &pkcs8PrivKeyItem,
      nickname.data == nullptr ? nullptr : &nickname, nullptr /*publicValue*/,
      true /*isPerm*/, false /*isPrivate*/, KU_ALL, nullptr);
  if (rv != SECSuccess) {
    std::cerr << "Importing a private key in DER format failed with error "
              << PR_ErrorToName(PR_GetError()) << std::endl;
    return false;
  }

  std::cout << "Key import succeeded." << std::endl;
  return true;
}

bool DBTool::DeleteCert(const ArgParser &parser) {
  std::string certName = parser.Get("--delete-cert");
  if (certName.empty()) {
    std::cerr << "A name is required to delete a certificate." << std::endl;
    Usage();
    return false;
  }

  ScopedCERTCertificate cert(CERT_FindCertByNicknameOrEmailAddr(
      CERT_GetDefaultCertDB(), certName.c_str()));
  if (!cert) {
    std::cerr << "Could not find certificate with name " << certName << "."
              << std::endl;
    return false;
  }

  SECStatus rv = SEC_DeletePermCertificate(cert.get());
  if (rv != SECSuccess) {
    std::cerr << "Unable to delete certificate with name " << certName << "."
              << std::endl;
    return false;
  }

  std::cout << "Certificate with name " << certName << " deleted successfully."
            << std::endl;
  return true;
}

bool DBTool::DeleteKey(const ArgParser &parser) {
  std::string keyName = parser.Get("--delete-key");
  if (keyName.empty()) {
    std::cerr << "A name is required to delete a key." << std::endl;
    Usage();
    return false;
  }

  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (slot.get() == nullptr) {
    std::cerr << "Error: Init PK11SlotInfo failed!" << std::endl;
    return false;
  }

  if (!DBLoginIfNeeded(slot)) {
    return false;
  }

  ScopedSECKEYPrivateKeyList list(PK11_ListPrivKeysInSlot(
      slot.get(), const_cast<char *>(keyName.c_str()), nullptr));
  if (list.get() == nullptr) {
    std::cerr << "Fetching private keys with nickname " << keyName
              << " failed with error " << PR_ErrorToName(PR_GetError())
              << std::endl;
    return false;
  }

  unsigned int foundKeys = 0, deletedKeys = 0;
  SECKEYPrivateKeyListNode *node;
  for (node = PRIVKEY_LIST_HEAD(list.get());
       !PRIVKEY_LIST_END(node, list.get()); node = PRIVKEY_LIST_NEXT(node)) {
    SECKEYPrivateKey *privKey = node->key;
    foundKeys++;
    // see PK11_DeleteTokenPrivateKey for example usage
    // calling PK11_DeleteTokenPrivateKey directly does not work because it also
    // destroys the SECKEYPrivateKey (by calling SECKEY_DestroyPrivateKey) -
    // then SECKEY_DestroyPrivateKeyList does not
    // work because it also calls SECKEY_DestroyPrivateKey
    SECStatus rv =
        PK11_DestroyTokenObject(privKey->pkcs11Slot, privKey->pkcs11ID);
    if (rv == SECSuccess) {
      deletedKeys++;
    }
  }

  if (foundKeys > deletedKeys) {
    std::cerr << "Some keys could not be deleted." << std::endl;
  }

  if (deletedKeys > 0) {
    std::cout << "Found " << foundKeys << " keys." << std::endl;
    std::cout << "Successfully deleted " << deletedKeys
              << " key(s) with nickname " << keyName << "." << std::endl;
  } else {
    std::cout << "No key with nickname " << keyName << " found to delete."
              << std::endl;
  }

  return true;
}
