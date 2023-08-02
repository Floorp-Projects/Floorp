# A primer on macOS SDKs

## Overview

A macOS SDK is an on-disk directory that contains header files and meta information for macOS APIs.
Apple distributes SDKs as part of the Xcode app bundle. Each Xcode version comes with one macOS SDK,
the SDK for the most recent released version of macOS at the time of the Xcode release.
The SDK is located at `/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk`.

Compiling Firefox for macOS requires a macOS SDK. The build system bootstraps an adequate SDK by
default, but you can select a different SDK using the `mozconfig` option `--with-macos-sdk`:

```text
ac_add_options --with-macos-sdk=/Users/username/SDKs/MacOSX11.3.sdk
```

## Supported SDKs

First off, Firefox runs on 10.15 and above. This is called the "minimum deployment target" and is
independent of the SDK version.

Our official Firefox builds compiled in CI (continuous integration) currently use the 13.3 SDK (last updated in [bug 1833998](https://bugzilla.mozilla.org/show_bug.cgi?id=1833998)). This is also the minimum supported SDK version for local builds.

Compiling with different SDKs breaks from time to time. Such breakages should be [reported in Bugzilla](https://bugzilla.mozilla.org/enter_bug.cgi?blocked=mach-busted&bug_type=defect&cc=:spohl,:mstange&component=General&form_name=enter_bug&keywords=regression&op_sys=macOS&product=Firefox%20Build%20System&rep_platform=All) and fixed quickly.

## Obtaining SDKs

Sometimes you need an SDK that's different from the one in your Xcode.app, for example
to check whether your code change breaks building with other SDKs, or to verify the
runtime behavior with the SDK used for CI builds.

The easy but slightly questionable way to obtain an SDK is to download it from a public github repo.

Here's another option:

 1. Have your Apple ID login details ready, and bring enough time and patience for a 5GB download.
 2. Check [these tables in the Xcode wikipedia article](https://en.wikipedia.org/wiki/Xcode#Xcode_7.0_-_10.x_(since_Free_On-Device_Development))
    and find an Xcode version that contains the SDK you need.
 3. Look up the Xcode version number on [xcodereleases.com](https://xcodereleases.com/) and click the Download link for it.
 4. Log in with your Apple ID. Then the download should start.
 5. Wait for the 5GB Xcode_*.xip download to finish.
 6. Open the downloaded xip file. This will extract the Xcode.app bundle.
 7. Inside the app bundle, the SDK is at `Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk`.

## Effects of the SDK version

An SDK only contains declarations of APIs. It does not contain the implementations for these APIs.

The implementation of an API is provided by the OS that the app runs on. It is supplied at runtime,
when your app starts up, by the dynamic linker. For example, the AppKit implementation comes
from `/System/Library/Frameworks/AppKit.framework` from the OS that the app is run on, regardless
of what SDK was used when compiling the app.

In other words, building with a macOS SDK of a higher version doesn't magically make new APIs available
when running on older versions of macOS. And, conversely, building with a lower macOS SDK doesn't limit
which APIs you can use if your app is run on a newer version of macOS, assuming you manage to convince the
compiler to accept your code.

The SDK used for building an app determines three things:

 1. Whether your code compiles at all,
 2. which range of macOS versions your app can run on (available deployment targets), and
 3. certain aspects of runtime behavior.

The first is straightforward: An SDK contains header files. If you call an API that's not declared
anywhere - neither in a header file nor in your own code - then your compiler will emit an error.
(Special case: Calling an unknown Objective-C method usually only emits a warning, not an error.)

The second aspect, available deployment targets, is usually not worth worrying about:
SDKs have large ranges of supported macOS deployment targets.
For example, the 10.15 SDK supports running your app on macOS versions all the way back to 10.6.
This information is written down in the SDK's `SDKSettings.plist`.

The third aspect, varying runtime behavior, is perhaps the most insidious and surprising aspect, and is described
in the next section.

## Runtime differences based on macOS SDK version

When a new version of macOS is released, existing APIs can change their behavior.
These changes are usually described in the AppKit release notes:

 - [macOS 10.15 release notes](https://developer.apple.com/documentation/macos_release_notes/macos_catalina_10_15_release_notes?language=objc)
 - [macOS 10.14 AppKit release notes](https://developer.apple.com/documentation/macos_release_notes/macos_mojave_10_14_release_notes/appkit_release_notes_for_macos_10_14?language=objc)
 - [macOS 10.13 AppKit release notes](https://developer.apple.com/library/archive/releasenotes/AppKit/RN-AppKit/)
 - [macOS 10.12 and older AppKit release notes](https://developer.apple.com/library/archive/releasenotes/AppKit/RN-AppKitOlderNotes/)

Sometimes, these differences in behavior have the potential to break existing apps. In those instances,
Apple often provides the old (compatible) behavior until the app is re-built with the new SDK, expecting
developers to update their apps so that they work with the new behavior, at the same time as
they update to the new SDK.

Here's an [example from the 10.13 release notes](https://developer.apple.com/library/archive/releasenotes/AppKit/RN-AppKit/#10_13NSCollectionView%20Responsive%20Scrolling):

> Responsive Scrolling in NSCollectionViews is enabled only for apps linked on or after macOS 10.13.

Here, "linked on or after macOS 10.13" means "linked against the macOS 10.13 SDK or newer".

Apple's expectation is that you upgrade to the new macOS version when it is released, download a new
Xcode version when it is released, synchronize these updates across the machines of all developers
that work on your app, use the SDK in the newest Xcode to compile your app, and make changes to your
app to be compatible with any behavior changes whenever you update Xcode.
This expectation does not always match reality. It definitely doesn't match what we're doing with Firefox.

For Firefox, SDK-dependent compatibility behaviors mean that developers who build Firefox locally
can see different runtime behavior than the users of our CI builds, if they use a different SDK than
the SDK used in CI.
That is, unless we change the Firefox code so that it has the same behavior regardless of SDK version.
Often this can be achieved by using APIs in a way that's more in line with the API's recommended use.

For example, we've had cases of
[broken placeholder text in search fields](https://bugzilla.mozilla.org/show_bug.cgi?id=1273106),
[missing](https://bugzilla.mozilla.org/show_bug.cgi?id=941325) or [double-drawn focus rings](https://searchfox.org/mozilla-central/rev/9ad88f80aeedcd3cd7d7f63be07f577861727054/widget/cocoa/nsNativeThemeCocoa.mm#149-169),
[a startup crash](https://bugzilla.mozilla.org/show_bug.cgi?id=1516437),
[fully black windows](https://bugzilla.mozilla.org/show_bug.cgi?id=1494022),
[fully gray windows](https://bugzilla.mozilla.org/show_bug.cgi?id=1576113#c4),
[broken vibrancy](https://bugzilla.mozilla.org/show_bug.cgi?id=1475694), and
[broken colors in dark mode](https://bugzilla.mozilla.org/show_bug.cgi?id=1578917).

In most of these cases, the breakage was either very minor, or it was caused by Firefox doing things
that were explicitly discouraged, like creating unexpected NSView hierarchies, or relying on unspecified
implementation details. (With one exception: In 10.14, HiDPI-aware `NSOpenGLContext` rendering in
layer-backed windows simply broke.)

And in all of these cases, it was the SDK-dependent compatibility behavior that protected our users from being
exposed to the breakage. Our CI builds continued to work because they were built with an older SDK.

We have addressed all known cases of breakage when building Firefox with newer SDKs.
I am not aware of any current instances of this problem as of this writing (June 2020).

For more information about how these compatibility tricks work,
read the [Overriding SDK-dependent runtime behavior](#overriding-sdk-dependent-runtime-behavior) section.

## Supporting multiple SDKs

As described under [Supported SDKs](#supported-sdks), Firefox can be built with a wide variety of SDK versions.

This ability comes at the cost of some manual labor; it requires some well-placed `#ifdefs` and
copying of header definitions.

Every SDK defines the macro `MAC_OS_X_VERSION_MAX_ALLOWED` with a value that matches the SDK version,
in the SDK's `AvailabilityMacros.h` header. This header also defines version constants like `MAC_OS_X_VERSION_10_12`.
For example, I have a version of the 10.12 SDK which contains the line

```cpp
#define MAC_OS_X_VERSION_MAX_ALLOWED MAC_OS_X_VERSION_10_12_4
```

The name `MAC_OS_X_VERSION_MAX_ALLOWED` is rather misleading; a better name would be
`MAC_OS_X_VERSION_MAX_KNOWN_BY_SDK`. Compiling with an old SDK *does not* prevent apps from running
on newer versions of macOS.

With the help of the `MAC_OS_X_VERSION_MAX_ALLOWED` macro, we can make our code adapt to the SDK that's
being used. Here's [an example](https://searchfox.org/mozilla-central/rev/9ad88f80aeedcd3cd7d7f63be07f577861727054/toolkit/xre/MacApplicationDelegate.mm#345-351) where the 10.14 SDK changed the signature of
[an `NSApplicationDelegate` method](https://developer.apple.com/documentation/appkit/nsapplicationdelegate/1428471-application?language=objc):

```objc++
- (BOOL)application:(NSApplication*)application
    continueUserActivity:(NSUserActivity*)userActivity
#if defined(MAC_OS_X_VERSION_10_14) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14
      restorationHandler:(void (^)(NSArray<id<NSUserActivityRestoring>>*))restorationHandler {
#else
      restorationHandler:(void (^)(NSArray*))restorationHandler {
#endif
  ...
}
```

We can also use this macro to supply missing API definitions in such a way that
they don't conflict with the definitions from the SDK.
This is described in the "Using macOS APIs" document, under [Using new APIs with old SDKs](./macos-apis.md#using-new-apis-with-old-sdks).

## Overriding SDK-dependent runtime behavior

This section contains some more details on the compatibility tricks that cause different runtime
behavior dependent on the SDK, as described in
[Runtime differences based on macOS SDK version](#runtime-differences-based-on-macos-sdk-version).

### How it works

AppKit is the one system framework I know of that employs these tricks. Let's explore how AppKit makes this work,
by going back to the [NSCollectionView example](https://developer.apple.com/library/archive/releasenotes/AppKit/RN-AppKit/#10_13NSCollectionView%20Responsive%20Scrolling) from above:

> Responsive Scrolling in NSCollectionViews is enabled only for apps linked on or after macOS 10.13.

For each of these SDK-dependent behavior differences, both the old and the new behavior are implemented
in the version of AppKit that ships with the new macOS version.
At runtime, AppKit selects one of the behaviors based on the SDK version, with a call to
`_CFExecutableLinkedOnOrAfter()`. This call checks the SDK version of the main executable of the
process that's running AppKit code; in our case that's the `firefox` or `plugin-container` executable.
The SDK version is stored in the mach-o headers of the executable by the linker.

One interesting design aspect of AppKit's compatibility tricks is the fact that most of these behavior differences
can be toggled with a "user default" preference.
For example, the "responsive scrolling in NSCollectionViews" behavior change can be controlled with
a user default with the name "NSCollectionViewPrefetchingEnabled".
The SDK check only happens if "NSCollectionViewPrefetchingEnabled" is not set to either YES or NO.

More precisely, this example works as follows:

 - `-[NSCollectionView prepareContentInRect:]` is the function that supports both the old and the new behavior.
 - It calls `_NSGetBoolAppConfig` for the value "NSCollectionViewPrefetchingEnabled", and also supplies a "default
   value function".
 - If the user default is not set, the default value function is called. This function has the name
   `NSCollectionViewPrefetchingEnabledDefaultValueFunction`.
 - `NSCollectionViewPrefetchingEnabledDefaultValueFunction` calls `_CFExecutableLinkedOnOrAfter(13)`.

You can find many similar toggles if you list the AppKit symbols that end in `DefaultValueFunction`,
for example by executing `nm /System/Library/Frameworks/AppKit.framework/AppKit | grep DefaultValueFunction`.

### Overriding SDK-dependent runtime behavior

You can set these preferences programmatically, in a way that `_NSGetBoolAppConfig()` can pick them up,
for example with [`registerDefaults`](https://developer.apple.com/documentation/foundation/nsuserdefaults/1417065-registerdefaults?language=objc)
or like this:

```objc++
[[NSUserDefaults standardUserDefaults] setBool:YES forKey:@"NSViewAllowsRootLayerBacking"];
```

The AppKit release notes mention this ability but ask for it to only be used for debugging purposes:

> In some cases, we provide defaults (preferences) settings which can be used to get the old or new behavior,
> independent of what system an application was built against. Often these preferences are provided for
> debugging purposes only; in some cases the preferences can be used to globally modify the behavior
> of an application by registering the values (do it somewhere very early, with `-[NSUserDefaults registerDefaults:]`).

It's interesting that they mention this at all because, as far as I can tell, none of these values are documented.
