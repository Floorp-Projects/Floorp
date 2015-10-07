/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GonkClipboardData
#define mozilla_GonkClipboardData

#include "mozilla/RefPtr.h"
#include "nsString.h"

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

class GonkClipboardData final
{
public:
  explicit GonkClipboardData() = default;
  ~GonkClipboardData() = default;

  // For text/plain
  void SetText(const nsAString &aText);
  bool HasText() const;
  const nsAString& GetText() const;

  // For text/html
  void SetHTML(const nsAString &aHTML);
  bool HasHTML() const;
  const nsAString& GetHTML() const;

  // For images
  void SetImage(gfx::DataSourceSurface* aDataSource);
  bool HasImage() const;
  already_AddRefed<gfx::DataSourceSurface> GetImage() const;

  // For other APIs
  void Clear();

private:
  nsAutoString mPlain;
  nsAutoString mHTML;
  RefPtr<gfx::DataSourceSurface> mImage;
};

} // namespace mozilla

#endif // mozilla_GonkClipboardData
