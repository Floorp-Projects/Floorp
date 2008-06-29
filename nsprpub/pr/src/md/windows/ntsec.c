/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "primpl.h"

/*
 * ntsec.c
 *
 * Implement the POSIX-style mode bits (access permissions) for
 * files and other securable objects in Windows NT using Windows
 * NT's security descriptors with appropriate discretionary
 * access-control lists.
 */

/*
 * The security identifiers (SIDs) for owner, primary group,
 * and the Everyone (World) group.
 *
 * These SIDs are looked up during NSPR initialization and
 * saved in this global structure (see _PR_NT_InitSids) so
 * that _PR_NT_MakeSecurityDescriptorACL doesn't need to
 * look them up every time.
 */
static struct {
    PSID owner;
    PSID group;
    PSID everyone;
} _pr_nt_sids;

/*
 * Initialize the SIDs for owner, primary group, and the Everyone
 * group in the _pr_nt_sids structure.
 *
 * This function needs to be called by NSPR initialization.
 */
void _PR_NT_InitSids(void)
{
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
    HANDLE hToken = NULL; /* initialized to an arbitrary value to
                           * silence a Purify UMR warning */
    UCHAR infoBuffer[1024];
    PTOKEN_OWNER pTokenOwner = (PTOKEN_OWNER) infoBuffer;
    PTOKEN_PRIMARY_GROUP pTokenPrimaryGroup
            = (PTOKEN_PRIMARY_GROUP) infoBuffer;
    DWORD dwLength;
    BOOL rv;

    /*
     * Look up and make a copy of the owner and primary group
     * SIDs in the access token of the calling process.
     */
    rv = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken);
    if (rv == 0) {
        /*
         * On non-NT systems, this function is not implemented
         * (error code ERROR_CALL_NOT_IMPLEMENTED), and neither are
         * the other security functions.  There is no point in
         * going further.
         *
         * A process with insufficient access permissions may fail
         * with the error code ERROR_ACCESS_DENIED.
         */
        PR_LOG(_pr_io_lm, PR_LOG_DEBUG,
                ("_PR_NT_InitSids: OpenProcessToken() failed. Error: %d",
                GetLastError()));
        return;
    }

    rv = GetTokenInformation(hToken, TokenOwner, infoBuffer,
            sizeof(infoBuffer), &dwLength);
    PR_ASSERT(rv != 0);
    dwLength = GetLengthSid(pTokenOwner->Owner);
    _pr_nt_sids.owner = (PSID) PR_Malloc(dwLength);
    PR_ASSERT(_pr_nt_sids.owner != NULL);
    rv = CopySid(dwLength, _pr_nt_sids.owner, pTokenOwner->Owner);
    PR_ASSERT(rv != 0);

    rv = GetTokenInformation(hToken, TokenPrimaryGroup, infoBuffer,
            sizeof(infoBuffer), &dwLength);
    PR_ASSERT(rv != 0);
    dwLength = GetLengthSid(pTokenPrimaryGroup->PrimaryGroup);
    _pr_nt_sids.group = (PSID) PR_Malloc(dwLength);
    PR_ASSERT(_pr_nt_sids.group != NULL);
    rv = CopySid(dwLength, _pr_nt_sids.group,
            pTokenPrimaryGroup->PrimaryGroup);
    PR_ASSERT(rv != 0);

    rv = CloseHandle(hToken);
    PR_ASSERT(rv != 0);

    /* Create a well-known SID for the Everyone group. */
    rv = AllocateAndInitializeSid(&SIDAuthWorld, 1,
            SECURITY_WORLD_RID,
            0, 0, 0, 0, 0, 0, 0,
            &_pr_nt_sids.everyone);
    PR_ASSERT(rv != 0);
}

/*
 * Free the SIDs for owner, primary group, and the Everyone group
 * in the _pr_nt_sids structure.
 *
 * This function needs to be called by NSPR cleanup.
 */
void
_PR_NT_FreeSids(void)
{
    if (_pr_nt_sids.owner) {
        PR_Free(_pr_nt_sids.owner);
    }
    if (_pr_nt_sids.group) {
        PR_Free(_pr_nt_sids.group);
    }
    if (_pr_nt_sids.everyone) {
        FreeSid(_pr_nt_sids.everyone);
    }
}

/*
 * Construct a security descriptor whose discretionary access-control
 * list implements the specified mode bits.  The SIDs for owner, group,
 * and everyone are obtained from the global _pr_nt_sids structure.
 * Both the security descriptor and access-control list are returned
 * and should be freed by a _PR_NT_FreeSecurityDescriptorACL call.
 *
 * The accessTable array maps NSPR's read, write, and execute access
 * rights to the corresponding NT access rights for the securable
 * object.
 */
PRStatus
_PR_NT_MakeSecurityDescriptorACL(
    PRIntn mode,
    DWORD accessTable[],
    PSECURITY_DESCRIPTOR *resultSD,
    PACL *resultACL)
{
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pACL = NULL;
    DWORD cbACL;  /* size of ACL */
    DWORD accessMask;

    if (_pr_nt_sids.owner == NULL) {
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        return PR_FAILURE;
    }

    pSD = (PSECURITY_DESCRIPTOR) PR_Malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (pSD == NULL) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }
    if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }
    if (!SetSecurityDescriptorOwner(pSD, _pr_nt_sids.owner, FALSE)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }
    if (!SetSecurityDescriptorGroup(pSD, _pr_nt_sids.group, FALSE)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }

    /*
     * Construct a discretionary access-control list with three
     * access-control entries, one each for owner, primary group,
     * and Everyone.
     */

    cbACL = sizeof(ACL)
          + 3 * (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD))
          + GetLengthSid(_pr_nt_sids.owner)
          + GetLengthSid(_pr_nt_sids.group)
          + GetLengthSid(_pr_nt_sids.everyone);
    pACL = (PACL) PR_Malloc(cbACL);
    if (pACL == NULL) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }
    if (!InitializeAcl(pACL, cbACL, ACL_REVISION)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }
    accessMask = 0;
    if (mode & 00400) accessMask |= accessTable[0];
    if (mode & 00200) accessMask |= accessTable[1];
    if (mode & 00100) accessMask |= accessTable[2];
    if (accessMask && !AddAccessAllowedAce(pACL, ACL_REVISION, accessMask,
            _pr_nt_sids.owner)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }
    accessMask = 0;
    if (mode & 00040) accessMask |= accessTable[0];
    if (mode & 00020) accessMask |= accessTable[1];
    if (mode & 00010) accessMask |= accessTable[2];
    if (accessMask && !AddAccessAllowedAce(pACL, ACL_REVISION, accessMask,
            _pr_nt_sids.group)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }
    accessMask = 0;
    if (mode & 00004) accessMask |= accessTable[0];
    if (mode & 00002) accessMask |= accessTable[1];
    if (mode & 00001) accessMask |= accessTable[2];
    if (accessMask && !AddAccessAllowedAce(pACL, ACL_REVISION, accessMask,
            _pr_nt_sids.everyone)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }

    if (!SetSecurityDescriptorDacl(pSD, TRUE, pACL, FALSE)) {
        _PR_MD_MAP_DEFAULT_ERROR(GetLastError());
        goto failed;
    }

    *resultSD = pSD;
    *resultACL = pACL;
    return PR_SUCCESS;

failed:
    if (pSD) {
        PR_Free(pSD);
    }
    if (pACL) {
        PR_Free(pACL);
    }
    return PR_FAILURE;
}

/*
 * Free the specified security descriptor and access-control list
 * previously created by _PR_NT_MakeSecurityDescriptorACL.
 */
void
_PR_NT_FreeSecurityDescriptorACL(PSECURITY_DESCRIPTOR pSD, PACL pACL)
{
    if (pSD) {
        PR_Free(pSD);
    }
    if (pACL) {
        PR_Free(pACL);
    }
}
