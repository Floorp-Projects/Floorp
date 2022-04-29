#include "gtest/gtest.h"

#include <stdio.h>
#include <windows.h>
#include <sddl.h>

#include "nsWindowsHelpers.h"

TEST(MaintenanceServiceTest, ServiceStartInteractiveOnly)
{
  // First, make a restricted token that excludes the Interactive group.
  SID_AND_ATTRIBUTES sid;
  DWORD SIDSize = SECURITY_MAX_SID_SIZE;
  sid.Sid = LocalAlloc(LMEM_FIXED, SIDSize);
  // Automatically free the SID when we are done with it.
  UniqueSidPtr uniqueSid(sid.Sid);
  ASSERT_TRUE(sid.Sid);

  BOOL success =
      CreateWellKnownSid(WinInteractiveSid, nullptr, sid.Sid, &SIDSize);
  ASSERT_TRUE(success);

  HANDLE primaryToken;
  success =
      OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &primaryToken);
  // Automatically close the token when we are done with it.
  nsAutoHandle uniquePrimaryToken(primaryToken);
  ASSERT_TRUE(success);

  HANDLE restrictedToken;
  success = CreateRestrictedToken(primaryToken, 0, 1, &sid, 0, nullptr, 0,
                                  nullptr, &restrictedToken);
  // Automatically close the token when we are done with it.
  nsAutoHandle uniqueRestrictedToken(restrictedToken);
  ASSERT_TRUE(success);

  success = ImpersonateLoggedOnUser(restrictedToken);
  ASSERT_TRUE(success);

  SC_HANDLE scmHandle =
      OpenSCManagerW(L"127.0.0.1", nullptr, SC_MANAGER_CONNECT);
  // Automatically close the SCM when we are done with it.
  nsAutoServiceHandle uniqueScmHandle(scmHandle);
  ASSERT_TRUE(scmHandle);

  SC_HANDLE serviceHandle =
      OpenServiceW(scmHandle, L"MozillaMaintenance", SERVICE_START);
  // Automatically close the SCM when we are done with it.
  nsAutoServiceHandle uniqueServiceHandle(serviceHandle);
  ASSERT_FALSE(serviceHandle);
  ASSERT_EQ(GetLastError(), static_cast<DWORD>(ERROR_ACCESS_DENIED));
}
