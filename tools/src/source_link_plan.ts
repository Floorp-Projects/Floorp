// SPDX-License-Identifier: MPL-2.0

/** Source directories linked into bridge packages before feature compilation. */
export const SOURCE_LINKS = [
  {
    link: ["bridge", "loader-features", "link-features-chrome"],
    target: ["browser-features", "chrome"],
  },
  {
    link: ["bridge", "loader-features", "link-i18n"],
    target: ["i18n"],
  },
  {
    link: ["bridge", "loader-modules", "link-modules"],
    target: ["browser-features", "modules"],
  },
] as const;
