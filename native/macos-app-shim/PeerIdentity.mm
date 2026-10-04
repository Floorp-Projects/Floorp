// SPDX-License-Identifier: MPL-2.0

#include "PeerIdentity.h"

#import <Security/Security.h>
#include <bsm/libbsm.h>
#include <cstring>
#include <unistd.h>

namespace floorp::shim {

PeerVerifier SignedHostVerifier(pid_t expectedPID, NSString* expectedIdentifier,
                                NSString* designatedRequirement) {
  if (expectedPID <= 0 || !expectedIdentifier.length || !designatedRequirement.length) return {};
  SecCodeRef self = nullptr;
  CFDictionaryRef signing = nullptr;
  if (SecCodeCopySelf(kSecCSDefaultFlags, &self) != errSecSuccess) return {};
  OSStatus status = SecCodeCopySigningInformation(self, kSecCSSigningInformation, &signing);
  bool selfValid = SecCodeCheckValidity(self, kSecCSStrictValidate, nullptr) == errSecSuccess;
  CFRelease(self);
  if (status != errSecSuccess || !selfValid) {
    if (signing) CFRelease(signing);
    return {};
  }
  NSDictionary* info = CFBridgingRelease(signing);
  if (!info[(__bridge NSString*)kSecCodeInfoIdentifier]) return {};
  SecRequirementRef requirement = nullptr;
  if (SecRequirementCreateWithString((__bridge CFStringRef)designatedRequirement,
                                     kSecCSDefaultFlags, &requirement) != errSecSuccess) return {};
  id retainedRequirement = CFBridgingRelease(requirement);
  NSString* pinnedIdentifier = [expectedIdentifier copy];
  uid_t uid = geteuid();
  return [expectedPID, retainedRequirement, pinnedIdentifier, uid, verified = false,
          pinnedToken = audit_token_t{}](const audit_token_t& token) mutable {
    if (audit_token_to_pid(token) != expectedPID || audit_token_to_euid(token) != uid) return false;
    if (verified) return std::memcmp(&pinnedToken, &token, sizeof(token)) == 0;
    NSData* audit = [NSData dataWithBytes:&token length:sizeof(token)];
    NSDictionary* attributes = @{(__bridge NSString*)kSecGuestAttributeAudit: audit};
    SecCodeRef guest = nullptr;
    if (SecCodeCopyGuestWithAttributes(nullptr, (__bridge CFDictionaryRef)attributes,
                                      kSecCSDefaultFlags, &guest) != errSecSuccess) return false;
    bool valid = SecCodeCheckValidity(guest, kSecCSStrictValidate,
        (__bridge SecRequirementRef)retainedRequirement) == errSecSuccess;
    CFDictionaryRef guestSigning = nullptr;
    OSStatus result = SecCodeCopySigningInformation(guest, kSecCSSigningInformation, &guestSigning);
    CFRelease(guest);
    if (result != errSecSuccess || !valid) {
      if (guestSigning) CFRelease(guestSigning);
      return false;
    }
    NSDictionary* guestInfo = CFBridgingRelease(guestSigning);
    if (![guestInfo[(__bridge NSString*)kSecCodeInfoIdentifier] isEqual:pinnedIdentifier]) return false;
    pinnedToken = token;
    verified = true;
    return true;
  };
}

}  // namespace floorp::shim
