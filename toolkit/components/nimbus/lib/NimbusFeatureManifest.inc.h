/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Maybe<nsCString> GetNimbusFallbackPrefName(const nsACString& aFeatureId,
                                           const nsACString& aVariable) {
  nsAutoCString manifestKey;
  manifestKey.Append(aFeatureId);
  manifestKey.Append("_");
  manifestKey.Append(aVariable);

  for (const auto& pair : NIMBUS_FALLBACK_PREFS) {
    if (pair.first.Equals(manifestKey.get())) {
      return Some(pair.second);
    }
  }
  return Nothing{};
}
