// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_APP_CONTAINER_H_
#define SANDBOX_WIN_SRC_APP_CONTAINER_H_

#include <windows.h>

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "sandbox/win/src/sandbox_types.h"

namespace base {
namespace win {
class StartupInformation;
}
}

namespace sandbox {

// Maintains an attribute list to be used during creation of a new sandboxed
// process.
class AppContainerAttributes {
 public:
  AppContainerAttributes();
  ~AppContainerAttributes();

  // Sets the AppContainer and capabilities to be used with the new process.
  ResultCode SetAppContainer(const base::string16& app_container_sid,
                             const std::vector<base::string16>& capabilities);

  // Updates the proc_thred attribute list of the provided startup_information
  // with the app container related data.
  // WARNING: startup_information just points back to our internal memory, so
  // the lifetime of this object has to be greater than the lifetime of the
  // provided startup_information.
  ResultCode ShareForStartup(
      base::win::StartupInformation* startup_information) const;

  bool HasAppContainer() const;

 private:
  SECURITY_CAPABILITIES capabilities_;
  std::vector<SID_AND_ATTRIBUTES> attributes_;

  DISALLOW_COPY_AND_ASSIGN(AppContainerAttributes);
};

// Creates a new AppContainer on the system. |sid| is the identifier of the new
// AppContainer, and |name| will be used as both the display name and moniker.
// This function fails if the OS doesn't support AppContainers, or if there is
// an AppContainer registered with the same id.
ResultCode CreateAppContainer(const base::string16& sid,
                              const base::string16& name);

// Deletes an AppContainer previously created with a successfull call to
// CreateAppContainer.
ResultCode DeleteAppContainer(const base::string16& sid);

// Retrieves the name associated with the provided AppContainer sid. Returns an
// empty string if the AppContainer is not registered with the system.
base::string16 LookupAppContainer(const base::string16& sid);

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_APP_CONTAINER_H_
