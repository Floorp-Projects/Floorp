---
layout: page
title: Migrate feature-sitepermissions to be compatible with the new GeckoView permission API.
permalink: /rfc/0005-migrate-sitepermission-ac-store-geckoview-store
---

* Start date: 2020-07-26
* RFC PR: [#7855](https://github.com/mozilla-mobile/android-components/pull/7855)

## Summary

Migrate our generic [SitePermissionsStorage](https://github.com/mozilla-mobile/android-components/blob/56e6f5ba55734014804b3f3b0af5c32414fa1d16/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsStorage.kt#L33) to be compatible with the new [GeckoView permissions API](https://docs.google.com/document/d/1KUq0gejnFm5erkHNkswm8JsT7nLOmWvs1KEGFz9FWxk/edit#heading=h.ls1dr18v7zrx).

## Motivation

We are currently storing all site permissions on disk in AC (both GeckoView and WebView). [With the new GeckoView API](https://docs.google.com/document/d/1KUq0gejnFm5erkHNkswm8JsT7nLOmWvs1KEGFz9FWxk/edit#heading=h.ls1dr18v7zrx), GeckoView is now going to store all the permissions on their end instead of storing it ourselves. They will expose APIs for managing permissions(query, add, delete, and update).

That put us in a situation where we have to migrate the existing user data to the new [GeckoView APIs](https://docs.google.com/document/d/1KUq0gejnFm5erkHNkswm8JsT7nLOmWvs1KEGFz9FWxk/edit#heading=h.ls1dr18v7zrx).

## Reference-level explanation

####  Doing a one-shot migration.
We decide on the timing (i.e. app startup) to do a background operation to transfer all the data to GeckoView in a one-shot. Until this operation is not finished, all the site permissions operations will be put on hold until the migration is over.

We transform [SitePermissionsStorage](https://github.com/mozilla-mobile/android-components/blob/a93eae2cbf81a17e80a98ec234db414d1f7e84f5/components/feature/sitepermissions/src/main/java/mozilla/components/feature/sitepermissions/SitePermissionsStorage.kt#L33) into an interface, with which we can have two distinctive implementations - one for GeckoView and the other for other engines. Inside the GeckoView implementation, we can have a preference that indicates if the migration is over or not.  When the `SitePermissionsFeature.start()` gets called, we start the migration by guarding all the operations(read/write) on GeckoViewStorage and holding its response until the
migration is over.

To mark when the migration will be over, we can define a time frame and after that we can remove the code for the migration.

## Drawbacks
We are going to do a fast migration but depending on how much data the user has, it could impact the user's experience, as users could load a page that requires permissions and this functionality will no be active until we finish the migration.

## Rationale and alternatives
**Re-prompt**, we could start from the scratch and ask again users to grant permissions to sites. One drawback is users will lost their previous data.