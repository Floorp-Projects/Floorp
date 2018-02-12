// Copyright © 2016-2017 winapi-rs developers
// Licensed under the Apache License, Version 2.0
// <LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your option.
// All files in the project carrying such notice may not be copied, modified, or distributed
// except according to those terms.
//! FFI bindings to psapi.

use shared::minwindef::{BOOL, DWORD, PBOOL, PDWORD, PUCHAR, PULONG, ULONG}
use um::winnt::{
    HANDLE, PACL, PCLAIM_SECURITY_ATTRIBUTES_INFORMATION, PHANDLE, PSID, PTOKEN_PRIVILEGES,
}

extern "system" {
    // pub fn AccessCheck();
    // pub fn AccessCheckAndAuditAlarmW();
    // pub fn AccessCheckByType();
    // pub fn AccessCheckByTypeResultList();
    // pub fn AccessCheckByTypeAndAuditAlarmW();
    // pub fn AccessCheckByTypeResultListAndAuditAlarmW();
    // pub fn AccessCheckByTypeResultListAndAuditAlarmByHandleW();
    // pub fn AddAccessAllowedAce();
    // pub fn AddAccessAllowedAceEx();
    // pub fn AddAccessAllowedObjectAce();
    // pub fn AddAccessDeniedAce();
    // pub fn AddAccessDeniedAceEx();
    // pub fn AddAccessDeniedObjectAce();
    // pub fn AddAce();
    // pub fn AddAuditAccessAce();
    // pub fn AddAuditAccessAceEx();
    // pub fn AddAuditAccessObjectAce();
    // pub fn AddMandatoryAce();
    pub fn AddResourceAttributeAce(
        pAcl: PACL,
        dwAceRevision: DWORD,
        AceFlags: DWORD,
        AccessMask: DWORD, pSid: PSID,
        pAttributeInfo: PCLAIM_SECURITY_ATTRIBUTES_INFORMATION,
        pReturnLength: PDWORD,
    ) -> BOOL;
    pub fn AddScopedPolicyIDAce(
        pAcl: PACL,
        dwAceRevision: DWORD,
        AceFlags: DWORD,
        AccessMask: DWORD,
        pSid: PSID,
    ) -> BOOL;
    // pub fn AdjustTokenGroups();
    pub fn AdjustTokenPrivileges(
        TokenHandle: HANDLE,
        DisableAllPrivileges: BOOL,
        NewState: PTOKEN_PRIVILEGES,
        BufferLength: DWORD,
        PreviousState: PTOKEN_PRIVILEGES,
        ReturnLength: PDWORD,
    ) -> BOOL;
    // pub fn AllocateAndInitializeSid();
    pub fn AllocateLocallyUniqueId(
        Luid: PLUID,
    ) -> BOOL;
    pub fn AreAllAccessesGranted(
        GrantedAccess: DWORD,
        DesiredAccess: DWORD,
    ) -> BOOL;
    pub fn AreAnyAccessesGranted(
        GrantedAccess: DWORD,
        DesiredAccess: DWORD,
    ) -> BOOL;
    pub fn CheckTokenMembershipEx(
        TokenHandle: HANDLE,
        SidToCheck: PSID,
        Flags: DWORD,
        IsMember: PBOOL,
    ) -> BOOL;
    pub fn CheckTokenCapability(
        TokenHandle: HANDLE,
        CapabilitySidToCheck: PSID,
        HasCapability: PBOOL,
    ) -> BOOL;
    pub fn GetAppContainerAce(
        Acl: PACL,
        StartingAceIndex: DWORD,
        AppContainerAce: *mut PVOID,
        AppContainerAceIndex: *mut DWORD,
    ) -> BOOL;
    // pub fn CheckTokenMembershipEx();
    // pub fn ConvertToAutoInheritPrivateObjectSecurity();
    // pub fn CopySid();
    // pub fn CreatePrivateObjectSecurity();
    // pub fn CreatePrivateObjectSecurityEx();
    // pub fn CreatePrivateObjectSecurityWithMultipleInheritance();
    // pub fn CreateRestrictedToken();
    // pub fn CreateWellKnownSid();
    // pub fn EqualDomainSid();
    // pub fn DeleteAce();
    // pub fn DestroyPrivateObjectSecurity();
    // pub fn DuplicateToken();
    // pub fn DuplicateTokenEx();
    // pub fn EqualPrefixSid();
    // pub fn EqualSid();
    // pub fn FindFirstFreeAce();
    // pub fn FreeSid();
    // pub fn GetAce();
    // pub fn GetAclInformation();
    // pub fn GetFileSecurityW();
    // pub fn GetKernelObjectSecurity();
    // pub fn GetLengthSid();
    // pub fn GetPrivateObjectSecurity();
    // pub fn GetSecurityDescriptorControl();
    // pub fn GetSecurityDescriptorDacl();
    // pub fn GetSecurityDescriptorGroup();
    // pub fn GetSecurityDescriptorLength();
    // pub fn GetSecurityDescriptorOwner();
    // pub fn GetSecurityDescriptorRMControl();
    // pub fn GetSecurityDescriptorSacl();
    // pub fn GetSidIdentifierAuthority();
    // pub fn GetSidLengthRequired();
    // pub fn GetSidSubAuthority();
    // pub fn GetSidSubAuthorityCount();
    // pub fn GetTokenInformation();
    // pub fn GetWindowsAccountDomainSid();
    // pub fn ImpersonateAnonymousToken();
    // pub fn ImpersonateLoggedOnUser();
    // pub fn ImpersonateSelf();
    // pub fn InitializeAcl();
    // pub fn InitializeSecurityDescriptor();
    // pub fn InitializeSid();
    // pub fn IsTokenRestricted();
    // pub fn IsValidAcl();
    // pub fn IsValidSecurityDescriptor();
    // pub fn IsValidSid();
    // pub fn IsWellKnownSid();
    // pub fn MakeAbsoluteSD();
    // pub fn MakeSelfRelativeSD();
    // pub fn MapGenericMask();
    // pub fn ObjectCloseAuditAlarmW();
    // pub fn ObjectDeleteAuditAlarmW();
    // pub fn ObjectOpenAuditAlarmW();
    // pub fn ObjectPrivilegeAuditAlarmW();
    // pub fn PrivilegeCheck();
    // pub fn PrivilegedServiceAuditAlarmW();
    // pub fn QuerySecurityAccessMask();
    // pub fn RevertToSelf();
    // pub fn SetAclInformation();
    // pub fn SetFileSecurityW();
    // pub fn SetKernelObjectSecurity();
    // pub fn SetPrivateObjectSecurity();
    // pub fn SetPrivateObjectSecurityEx();
    // pub fn SetSecurityAccessMask();
    // pub fn SetSecurityDescriptorControl();
    // pub fn SetSecurityDescriptorDacl();
    // pub fn SetSecurityDescriptorGroup();
    // pub fn SetSecurityDescriptorOwner();
    // pub fn SetSecurityDescriptorRMControl();
    // pub fn SetSecurityDescriptorSacl();
    // pub fn SetTokenInformation();
    pub fn SetCachedSigningLevel(
        SourceFiles: PHANDLE,
        SourceFileCount: ULONG,
        Flags: ULONG,
        TargetFile: HANDLE,
    ) -> BOOL;
    pub fn GetCachedSigningLevel(
        File: HANDLE,
        Flags: PULONG,
        SigningLevel: PULONG,
        Thumbprint: PUCHAR,
        ThumbprintSize: PULONG,
        ThumbprintAlgorithm: PULONG,
    ) -> BOOL;
    // pub fn CveEventWrite();
}
