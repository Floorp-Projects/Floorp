/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedding_PrintSettingsDialogParent_h
#define mozilla_embedding_PrintSettingsDialogParent_h

#include "mozilla/embedding/PPrintSettingsDialogParent.h"

// Header file contents
namespace mozilla {
namespace embedding {

class PrintSettingsDialogParent final : public PPrintSettingsDialogParent
{
public:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  MOZ_IMPLICIT PrintSettingsDialogParent();

private:
  virtual ~PrintSettingsDialogParent();
};

} // namespace embedding
} // namespace mozilla

#endif
