/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrintSettingsDialogParent.h"

// C++ file contents
namespace mozilla {
namespace embedding {

PrintSettingsDialogParent::PrintSettingsDialogParent()
{
  MOZ_COUNT_CTOR(PrintSettingsDialogParent);
}

PrintSettingsDialogParent::~PrintSettingsDialogParent()
{
  MOZ_COUNT_DTOR(PrintSettingsDialogParent);
}

void
PrintSettingsDialogParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace embedding
} // namespace mozilla

