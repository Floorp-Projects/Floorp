/*
 * Copyright (C) 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef GONKPERMISSION_H
#define GONKPERMISSION_H

#include <binder/BinderService.h>
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
class PermissionGrant
{
public:
  PermissionGrant(const char* perm, int32_t p) : mPid(p)
  {
    mPermission.Assign(perm);
  }

  PermissionGrant(const nsACString& permission, int32_t pid) : mPid(pid),
    mPermission(permission)
  {
  }

  bool operator==(const PermissionGrant& other) const
  {
    return (mPid == other.pid() && mPermission.Equals(other.permission()));
  }

  int32_t pid() const
  {
    return mPid;
  }

  const nsACString& permission() const
  {
    return mPermission;
  }

private:
  int32_t mPid;
  nsCString mPermission;
};

class PermissionGrant;

class GonkPermissionService :
  public android::BinderService<GonkPermissionService>,
  public android::BnPermissionController
{
public:
  virtual ~GonkPermissionService() {}
  static GonkPermissionService* GetInstance();
  static const char *getServiceName() {
    return "permission";
  }

  static void instantiate();

  virtual android::status_t dump(int fd, const android::Vector<android::String16>& args) {
    return android::NO_ERROR;
  }
  virtual bool checkPermission(const android::String16& permission, int32_t pid,
      int32_t uid);

  void addGrantInfo(const char* permission, int32_t pid);
private:
  GonkPermissionService(): android::BnPermissionController() {}
  nsTArray<PermissionGrant> mGrantArray;
};
} // namespace mozilla
#endif // GONKPERMISSION_H
