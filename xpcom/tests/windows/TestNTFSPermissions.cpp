/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Aaron Nowack <anowack@mimiru.net>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * Test for NTFS File Permissions being correctly changed to match the new
 * directory upon moving a file.  (Bug 224692.)
 */

#include "../TestHarness.h"
#include "nsEmbedString.h"
#include "nsILocalFile.h"
#include <windows.h>
#include <aclapi.h>

#define BUFFSIZE 512



nsresult TestPermissions()
{

    nsresult rv; // Return value

    // File variables
    HANDLE tempFileHandle;
    nsCOMPtr<nsILocalFile> tempFile;
    nsCOMPtr<nsILocalFile> tempDirectory1;
    nsCOMPtr<nsILocalFile> tempDirectory2;
    WCHAR filePath[MAX_PATH];
    WCHAR dir1Path[MAX_PATH];
    WCHAR dir2Path[MAX_PATH];

    // Security variables
    DWORD result;
    PSID everyoneSID = NULL, adminSID = NULL;
    PACL dirACL = NULL, fileACL = NULL;
    PSECURITY_DESCRIPTOR dirSD = NULL, fileSD = NULL;
    EXPLICIT_ACCESS ea[2];
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld =
            SECURITY_WORLD_SID_AUTHORITY;
    SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;
    SECURITY_ATTRIBUTES sa;
    TRUSTEE everyoneTrustee;
    ACCESS_MASK everyoneRights;

    // Create a well-known SID for the Everyone group.
    if(!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                     SECURITY_WORLD_RID,
                     0, 0, 0, 0, 0, 0, 0,
                     &everyoneSID))
    {
        fail("NTFS Permissions: AllocateAndInitializeSid Error");
        return NS_ERROR_FAILURE;
    }

    // Create a SID for the Administrators group.
    if(! AllocateAndInitializeSid(&SIDAuthNT, 2,
                     SECURITY_BUILTIN_DOMAIN_RID,
                     DOMAIN_ALIAS_RID_ADMINS,
                     0, 0, 0, 0, 0, 0,
                     &adminSID)) 
    {
        fail("NTFS Permissions: AllocateAndInitializeSid Error");
        return NS_ERROR_FAILURE; 
    }

    // Initialize an EXPLICIT_ACCESS structure for an ACE.
    // The ACE will allow Everyone read access to the directory.
    ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESS));
    ea[0].grfAccessPermissions = GENERIC_READ;
    ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance= SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea[0].Trustee.ptstrName  = (LPTSTR) everyoneSID;

    // Initialize an EXPLICIT_ACCESS structure for an ACE.
    // The ACE will allow the Administrators group full access
    ea[1].grfAccessPermissions = GENERIC_ALL | STANDARD_RIGHTS_ALL;
    ea[1].grfAccessMode = SET_ACCESS;
    ea[1].grfInheritance= SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea[1].Trustee.ptstrName  = (LPTSTR) adminSID;

    // Create a new ACL that contains the new ACEs.
    result = SetEntriesInAcl(2, ea, NULL, &dirACL);
    if (ERROR_SUCCESS != result) 
    {
        fail("NTFS Permissions: SetEntriesInAcl Error");
        return NS_ERROR_FAILURE; 
    }

    // Initialize a security descriptor.  
    dirSD = (PSECURITY_DESCRIPTOR) LocalAlloc(LPTR, 
                             SECURITY_DESCRIPTOR_MIN_LENGTH); 
    if (NULL == dirSD) 
    { 
        fail("NTFS Permissions: LocalAlloc Error");
        return NS_ERROR_FAILURE; 
    }

    if (!InitializeSecurityDescriptor(dirSD,
            SECURITY_DESCRIPTOR_REVISION)) 
    {  
        fail("NTFS Permissions: InitializeSecurityDescriptor Error");
        return NS_ERROR_FAILURE; 
    } 

    // Add the ACL to the security descriptor. 
    if (!SetSecurityDescriptorDacl(dirSD, PR_TRUE, dirACL, PR_FALSE)) 
    {  
        fail("NTFS Permissions: SetSecurityDescriptorDacl Error");
        return NS_ERROR_FAILURE;  
    } 

    // Initialize a security attributes structure.
    sa.nLength = sizeof (SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = dirSD;
    sa.bInheritHandle = PR_FALSE;

    // Create and open first temporary directory
    if(!CreateDirectoryW(L".\\NTFSPERMTEMP1", &sa))
    {
        fail("NTFS Permissions: Creating Temporary Directory");
        return NS_ERROR_FAILURE;
    }

    GetFullPathNameW((LPCWSTR)L".\\NTFSPERMTEMP1", MAX_PATH, dir1Path, NULL);


    rv = NS_NewLocalFile(nsEmbedString(dir1Path), PR_FALSE,
                         getter_AddRefs(tempDirectory1));
    if (NS_FAILED(rv))
    {
        fail("NTFS Permissions: Opening Temporary Directory 1");
        return rv;
    }


    // Create and open temporary file
    tempFileHandle = CreateFileW(L".\\NTFSPERMTEMP1\\NTFSPerm.tmp", 
                            GENERIC_READ | GENERIC_WRITE,
                            0, 
                            NULL, //default security
                            CREATE_ALWAYS,        
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);  

    if(tempFileHandle == INVALID_HANDLE_VALUE)
    {
        fail("NTFS Permissions: Creating Temporary File");
        return NS_ERROR_FAILURE;
    }

    CloseHandle(tempFileHandle);

    GetFullPathNameW((LPCWSTR)L".\\NTFSPERMTEMP1\\NTFSPerm.tmp", 
                        MAX_PATH, filePath, NULL);

    rv = NS_NewLocalFile(nsEmbedString(filePath), PR_FALSE,
                         getter_AddRefs(tempFile));
    if (NS_FAILED(rv))
    {
        fail("NTFS Permissions: Opening Temporary File");
                return rv;
    }

    // Update Everyone Explict_Acess to full access.
    ea[0].grfAccessPermissions = GENERIC_ALL | STANDARD_RIGHTS_ALL;

    // Update the ACL to contain the new ACEs.
    result = SetEntriesInAcl(2, ea, NULL, &dirACL);
    if (ERROR_SUCCESS != result) 
    {
        fail("NTFS Permissions: SetEntriesInAcl 2 Error");
        return NS_ERROR_FAILURE; 
    }

    // Add the new ACL to the security descriptor. 
    if (!SetSecurityDescriptorDacl(dirSD, PR_TRUE, dirACL, PR_FALSE)) 
    {  
        fail("NTFS Permissions: SetSecurityDescriptorDacl 2 Error");
        return NS_ERROR_FAILURE;  
    } 

    // Create and open second temporary directory
    if(!CreateDirectoryW(L".\\NTFSPERMTEMP2", &sa))
    {
        fail("NTFS Permissions: Creating Temporary Directory 2");
        return NS_ERROR_FAILURE;
    }

    GetFullPathNameW((LPCWSTR)L".\\NTFSPERMTEMP2", MAX_PATH, dir2Path, NULL);


    rv = NS_NewLocalFile(nsEmbedString(dir2Path), PR_FALSE,
                         getter_AddRefs(tempDirectory2));
    if (NS_FAILED(rv))
    {
        fail("NTFS Permissions: Opening Temporary Directory 2");
        return rv;
    }

    // Move the file.
    rv = tempFile->MoveTo(tempDirectory2, EmptyString());

    if (NS_FAILED(rv))
    {
        fail("NTFS Permissions: Moving");
        return rv;
    }

    // Access the ACL of the file
    result = GetNamedSecurityInfoW(L".\\NTFSPERMTEMP2\\NTFSPerm.tmp", 
                                        SE_FILE_OBJECT,
                                        DACL_SECURITY_INFORMATION | 
                                        UNPROTECTED_DACL_SECURITY_INFORMATION,
                                        NULL, NULL, &fileACL, NULL, &fileSD);
    if (ERROR_SUCCESS != result) 
    {
        fail("NTFS Permissions: GetNamedSecurityDescriptor Error");
        return NS_ERROR_FAILURE; 
    }

    // Build a trustee representing "Everyone"
    BuildTrusteeWithSid(&everyoneTrustee, everyoneSID);

    // Get Everyone's effective rights.
    result = GetEffectiveRightsFromAcl(fileACL, &everyoneTrustee, 
                                        &everyoneRights);
    if (ERROR_SUCCESS != result) 
    {
        fail("NTFS Permissions: GetEffectiveRightsFromAcl Error");
        return NS_ERROR_FAILURE; 
    }

    // Check for delete access, which we won't have unless permissions have 
    // updated
    if((everyoneRights & DELETE) == (DELETE))
    {
        passed("NTFS Permissions Test");
        rv = NS_OK;
    }
    else
    {
        fail("NTFS Permissions: Access check.");
        rv = NS_ERROR_FAILURE;
    }

    // Cleanup
    if (everyoneSID) 
        FreeSid(everyoneSID);
    if (adminSID) 
        FreeSid(adminSID);
    if (dirACL) 
        LocalFree(dirACL);
    if (dirSD) 
        LocalFree(dirSD);
    if(fileACL)
        LocalFree(fileACL);

    tempDirectory1->Remove(PR_TRUE);
    tempDirectory2->Remove(PR_TRUE);
    
    return rv;
}

int main(int argc, char** argv)
{
    ScopedXPCOM xpcom("NTFSPermissionsTests"); // name for tests being run
    if (xpcom.failed())
        return 1;

    int rv = 0;

    if(NS_FAILED(TestPermissions()))
        rv = 1;

    return rv;

}

