* Start date: 2022-06-21
* RFC PR: [#112](https://github.com/mozilla-mobile/shared-docs/pull/112)
---

## Summary

The wallpapers feature was introduced on Android and iOS in v98 during Q1 2022. Due to late scope changes, a networking solution was quickly put together for delivering wallpapers from a remote source. As part of a larger set of improvements planned for Q3 2022, it is intended to revisit this implementation to create a more scalable version.

---

## Motivation

The original version of the networking solution consisted of adding wallpaper image files to deeply nested directories which defined the properties of the wallpaper. For example, an Android wallpaper might be found at:
```
<root>/mobile-wallpapers/android/<resolution>/<orientation>/<light/dark>/<wallpaper collection>/<wallpaper name>
```

This directory structure was confusing and difficult to manage. Copying several versions of a file to different locations was painful, even when automated. Automation would require consistent file naming for each new set of wallpapers, which can be hard to guarantee.

Additionally, downloads were started automatically during app startup if a wallpaper was missing.

In general, we would like to achieve the following goals:

- it is easier for product, UX, and ENG to add new wallpapers
- it is possible to determine important metadata about a wallpaper, like its size or average color
- download is transparent to the user
- download is activated by the user
- require less bandwidth, if possible

---

## Guide-level explanation

Instead of relying on an (undocumented) convention for file paths, wallpaper metadata will be contained in a top-level JSON schema. Using a well-known format will help discoverability, and JSON's flexibility should allow for simpler long-term maintenance. It can also provide a more direct path to any required automation. This schema could define things like:

- Android file path(s)
- iOS file path(s)
- thumbnail path(s)
- average color (for text and background coloring purposes)
- collection grouping for things like seasonal promotions
- wallpaper availability range
- wallpaper region availability

This is not a finalized spec, but an example might look like the following:
```json
{
    "last-updated-date": "2022-01-01",
    "collections": [
        {
            "name": "McCollectionFace",
            "available-locales": ["en-US", "es-US", "en-CA", "fr-CA"],
            "availability-range": {
                "start": "2022-06-27",
                "end": "2022-09-30"
            },
            "wallpapers": [
                {
                    "name": "Sunset",
                    "android-file-paths": ["android/path/to/sunset.svg"],
                    "android-thumbnail-path": "android/path/to/sunset-thumbnail.svg",
                    "iOS-file-paths": ["ios/path/to/sunset/high-resolution.pdf", "ios/path/to/sunset/low-resolution.pdf"],
                    "iOS-thumbnail-path": "ios/path/to/sunset-thumbnail.pdf",
                    "average-color": "0xADD8E6",
                }
            ]
        }
    ]
}
```

We will likely continue to ship some small number of wallpapers as part of our app binaries. This ensures that at least a few wallpapers are available for onboarding. Thumbnails for onboarding wallpapers and non-onboarding wallpapers may also be included.

Additionally, including thumbnails will allow clients to keep download size relatively small. Thumbnails will need to follow the current eager downloading strategy that wallpapers currently do. This will allow thumbnails to be decoupled from app releases, but they will still be visible when the settings page is open. The current suggestion is to display some kind of action overlay above the thumbnails indicating that clicking them will cause a download.

---

## Drawbacks

- JSON becomes tied to app versions. Adding or changing fields could mean that older versions of the app are no longer able to parse the schema.

## Rationale and alternatives

- Generally, JSON is still parsable even if new fields are added, and old versions of the app wouldn't know what to do with new fields anyway. Updating the types of fields would lead more directly to problems. In those cases it would always be possible to add a new field instead, but this could create quite a lot of noise in the definition. Even if clients fail to fetch the latest metadata, it's quite possible users have downloaded the subset of wallpapers they are interested in already.
- Alternatively, we could introduce versioned schema that could be easily tied to client-side upgrades. For example, a list of JSON files like: `json/v1.json`, `json/v2.json`, etc.
