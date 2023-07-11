/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeychainSecret.h"

#include <Security/Security.h>

#include "mozilla/Logging.h"

// This is the implementation of KeychainSecret, an instantiation of OSKeyStore
// for OS X. It uses the system keychain, hence the name.

using namespace mozilla;

LazyLogModule gKeychainSecretLog("keychainsecret");

KeychainSecret::KeychainSecret() {}

KeychainSecret::~KeychainSecret() {}

ScopedCFType<CFStringRef> MozillaStringToCFString(const nsACString& stringIn) {
  // https://developer.apple.com/documentation/corefoundation/1543419-cfstringcreatewithbytes
  ScopedCFType<CFStringRef> stringOut(CFStringCreateWithBytes(
      nullptr, reinterpret_cast<const UInt8*>(stringIn.BeginReading()),
      stringIn.Length(), kCFStringEncodingUTF8, false));
  return stringOut;
}

nsresult KeychainSecret::StoreSecret(const nsACString& aSecret,
                                     const nsACString& aLabel) {
  // This creates a CFDictionary of the form:
  // { class: generic password,
  //   account: the given label,
  //   value: the given secret }
  // "account" is the way we differentiate different secrets.
  // By default, secrets stored by the application (Firefox) in this way are not
  // accessible to other applications, so we shouldn't need to worry about
  // unauthorized access or namespace collisions. This will be the case as long
  // as we never set the kSecAttrAccessGroup attribute on the CFDictionary. The
  // platform enforces this restriction using the application-identifier
  // entitlement that each application bundle should have. See
  // https://developer.apple.com/documentation/security/1401659-secitemadd?language=objc#discussion

  // The keychain does not overwrite secrets by default (unlike other backends
  // like libsecret and credential manager). To be consistent, we first delete
  // any previously-stored secrets that use the given label.
  nsresult rv = DeleteSecret(aLabel);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("DeleteSecret before StoreSecret failed"));
    return rv;
  }
  const CFStringRef keys[] = {kSecClass, kSecAttrAccount, kSecValueData};
  ScopedCFType<CFStringRef> label(MozillaStringToCFString(aLabel));
  if (!label) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("MozillaStringToCFString failed"));
    return NS_ERROR_FAILURE;
  }
  ScopedCFType<CFDataRef> secret(CFDataCreate(
      nullptr, reinterpret_cast<const UInt8*>(aSecret.BeginReading()),
      aSecret.Length()));
  if (!secret) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug, ("CFDataCreate failed"));
    return NS_ERROR_FAILURE;
  }
  const void* values[] = {kSecClassGenericPassword, label.get(), secret.get()};
  static_assert(ArrayLength(keys) == ArrayLength(values),
                "mismatched SecItemAdd key/value array sizes");
  ScopedCFType<CFDictionaryRef> addDictionary(CFDictionaryCreate(
      nullptr, (const void**)&keys, (const void**)&values, ArrayLength(keys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  // https://developer.apple.com/documentation/security/1401659-secitemadd
  OSStatus osrv = SecItemAdd(addDictionary.get(), nullptr);
  if (osrv != errSecSuccess) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("SecItemAdd failed: %d", osrv));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult KeychainSecret::DeleteSecret(const nsACString& aLabel) {
  // To delete a secret, we create a CFDictionary of the form:
  // { class: generic password,
  //   account: the given label }
  // and then call SecItemDelete.
  const CFStringRef keys[] = {kSecClass, kSecAttrAccount};
  ScopedCFType<CFStringRef> label(MozillaStringToCFString(aLabel));
  if (!label) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("MozillaStringToCFString failed"));
    return NS_ERROR_FAILURE;
  }
  const void* values[] = {kSecClassGenericPassword, label.get()};
  static_assert(ArrayLength(keys) == ArrayLength(values),
                "mismatched SecItemDelete key/value array sizes");
  ScopedCFType<CFDictionaryRef> deleteDictionary(CFDictionaryCreate(
      nullptr, (const void**)&keys, (const void**)&values, ArrayLength(keys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  // https://developer.apple.com/documentation/security/1395547-secitemdelete
  OSStatus rv = SecItemDelete(deleteDictionary.get());
  if (rv != errSecSuccess && rv != errSecItemNotFound) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("SecItemDelete failed: %d", rv));
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult KeychainSecret::RetrieveSecret(const nsACString& aLabel,
                                        /* out */ nsACString& aSecret) {
  // To retrieve a secret, we create a CFDictionary of the form:
  // { class: generic password,
  //   account: the given label,
  //   match limit: match one,
  //   return attributes: true,
  //   return data: true }
  // This searches for and returns the attributes and data for the secret
  // matching the given label. We then extract the data (i.e. the secret) and
  // return it.
  const CFStringRef keys[] = {kSecClass, kSecAttrAccount, kSecMatchLimit,
                              kSecReturnAttributes, kSecReturnData};
  ScopedCFType<CFStringRef> label(MozillaStringToCFString(aLabel));
  if (!label) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("MozillaStringToCFString failed"));
    return NS_ERROR_FAILURE;
  }
  const void* values[] = {kSecClassGenericPassword, label.get(),
                          kSecMatchLimitOne, kCFBooleanTrue, kCFBooleanTrue};
  static_assert(ArrayLength(keys) == ArrayLength(values),
                "mismatched SecItemCopyMatching key/value array sizes");
  ScopedCFType<CFDictionaryRef> searchDictionary(CFDictionaryCreate(
      nullptr, (const void**)&keys, (const void**)&values, ArrayLength(keys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  CFTypeRef item;
  // https://developer.apple.com/documentation/security/1398306-secitemcopymatching
  OSStatus rv = SecItemCopyMatching(searchDictionary.get(), &item);
  if (rv != errSecSuccess) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("SecItemCopyMatching failed: %d", rv));
    return NS_ERROR_FAILURE;
  }
  ScopedCFType<CFDictionaryRef> dictionary(
      reinterpret_cast<CFDictionaryRef>(item));
  CFDataRef secret = reinterpret_cast<CFDataRef>(
      CFDictionaryGetValue(dictionary.get(), kSecValueData));
  if (!secret) {
    MOZ_LOG(gKeychainSecretLog, LogLevel::Debug,
            ("CFDictionaryGetValue failed"));
    return NS_ERROR_FAILURE;
  }
  aSecret.Assign(reinterpret_cast<const char*>(CFDataGetBytePtr(secret)),
                 CFDataGetLength(secret));
  return NS_OK;
}
