## Marking an unused string to be removed

Removing strings manually could cause crashes in **Beta** and **Release** versions 💥 , as the removed strings could be [uplifted to release branches](https://github.com/mozilla-mobile/fenix/pull/20364) by mistake, where strings are still needed. For this reason, we need a special process to remove them.

Any landed string that is not removed while in development in Nightly will persist through 3 Firefox releases (Nightly, Beta, Release) before we can remove them from our code base. For example,
if you want to remove a string that has already shipped prior to **Firefox Nightly 93**, the same string will still be in-use in **Firefox Beta 92** and **Firefox Release 91**. This means the string will be marked as unused and removed in 93 while still riding the train, and it can be removed safely when **Firefox Release 93** no longer ships, for instance, **Firefox Release 94** and beyond.

To keep us safe when you want to remove strings from nightly: 

1. Add these attributes to the target strings `moz:removedIn="<<ACTUAL_NIGHTLY_VERSION>>"` and `tools:ignore="UnusedResources"`.

```xml   
    <string name="onboarding_close" moz:removedIn="93" tools:ignore="UnusedResources">Close</string>
```
Example PR https://github.com/mozilla-mobile/fenix/pull/20980.

## When to remove an unused string and how

Strings that have been tagged with `moz:removedIn` attributes are safe to be removed after the marked version is no longer shipping and no longer in-use or needed. 

Consult the [Firefox release calendar](https://wiki.mozilla.org/Release_Management/Calendar). Let's say the Beta cut just happened and we are at Firefox Nightly 109, Firefox Beta 108 and Firefox Release 107. Everything marked with `moz:removedIn` <= 106 can now be removed.

You only need to remove the en-US strings within [values/strings.xml](https://searchfox.org/mozilla-mobile/source/fenix/app/src/main/res/values/strings.xml), and this change will propagate to the other locales.

## Future

It would be nice to add some automatization to delete the strings that have the `moz:removedIn` attributes where a full cycle has happened (3 releases versions from the actual release version).
