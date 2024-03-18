# [Android Components](../../../README.md) > Service > Pocket

A library for easily getting Pocket recommendations that transparently handles downloading, caching and periodically refreshing Pocket data.

Currently this supports:

- Pocket recommended stories.
- Pocket sponsored stories.

## Usage
1. For Pocket recommended stories:
   - Use `PocketStoriesService#startPeriodicStoriesRefresh` and `PocketStoriesService#stopPeriodicStoriesRefresh`
     as high up in the client app as possible (preferably in the Application object or in a single Activity) to ensure the
     background story refresh functionality works for the entirety of the app lifetime.
   - Use `PocketStoriesService.getStories` to get the current list of Pocket recommended stories.

2. For Pocket sponsored stories:
   - Use `PocketStoriesService#startPeriodicSponsoredStoriesRefresh` and `PocketStoriesService#stopPeriodicSponsoredStoriesRefresh`
     as high up in the client app as possible (preferably in the Application object or in a single Activity) to ensure the
     background story refresh functionality works for the entirety of the app lifetime.
   - Use `PocketStoriesService.getSponsoredStories` to get the current list of Pocket recommended stories.
   - Use `PocketStoriesService,recordStoriesImpressions` to try and persist that a list of sponsored stories were shown to the user. (Safe to call even if those stories are not persisted).
   - Use `PocketStoriesService.deleteProfile` to delete all server stored information about the device to which sponsored stories were previously downloaded. This may include data like network ip and application tokens.

   ##### Pacing and rotating:
   A new `PocketSponsoredStoryCaps` is available in the response from `PocketStoriesService.getSponsoredStories` which allows checking `currentImpressions`, `lifetimeCount`, `flightCount`, `flightPeriod` based on which the client can decide which stories to show.
   All this is based on clients calling `PocketStoriesService,recordStoriesImpressions` to record new impressions in between application restarts.




### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:service-pocket:{latest-version}"
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
