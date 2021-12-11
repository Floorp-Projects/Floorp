/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "readstrings.h"

#define SVC_DISPLAY_NAME L"Mozilla Maintenance Service"

enum SvcInstallAction { UpgradeSvc, InstallSvc, ForceInstallSvc };
BOOL SvcInstall(SvcInstallAction action);
BOOL SvcUninstall();
BOOL StopService();
BOOL SetUserAccessServiceDACL(SC_HANDLE hService);
DWORD SetUserAccessServiceDACL(SC_HANDLE hService, PACL& pNewAcl,
                               PSECURITY_DESCRIPTOR psd);

struct MaintenanceServiceStringTable {
  mozilla::UniquePtr<char[]> serviceDescription;
};
