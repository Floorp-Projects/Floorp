/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkClipboardData.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"

namespace mozilla {

void
GonkClipboardData::SetText(const nsAString &aText)
{
  mPlain = aText;
}

bool
GonkClipboardData::HasText() const
{
  return !mPlain.IsEmpty();
}

const nsAString&
GonkClipboardData::GetText() const
{
  return mPlain;
}

void
GonkClipboardData::SetHTML(const nsAString &aHTML)
{
  mHTML = aHTML;
}

bool
GonkClipboardData::HasHTML() const
{
  return !mHTML.IsEmpty();
}

const nsAString&
GonkClipboardData::GetHTML() const
{
  return mHTML;
}

void
GonkClipboardData::SetImage(gfx::DataSourceSurface* aDataSource)
{
  // Clone a new DataSourceSurface and store it.
  mImage = gfx::CreateDataSourceSurfaceByCloning(aDataSource);
}

bool
GonkClipboardData::HasImage() const
{
  return static_cast<bool>(mImage);
}

already_AddRefed<gfx::DataSourceSurface>
GonkClipboardData::GetImage() const
{
  // Return cloned DataSourceSurface.
  RefPtr<gfx::DataSourceSurface> cloned = gfx::CreateDataSourceSurfaceByCloning(mImage);
  return cloned.forget();
}

void
GonkClipboardData::Clear()
{
  mPlain.Truncate(0);
  mHTML.Truncate(0);
  mImage = nullptr;
}

} // namespace mozilla
