# Using macOS APIs

With each new macOS release, new APIs are added. Due to the wide range of platforms that Firefox runs on,
and due to the [wide range of SDKs that we support building with](./sdks.html#supported-sdks),
using macOS APIs in Firefox requires some extra care.

## Availability of APIs, and runtime checks

First of all, if you use an API that is supported by all versions of macOS that Firefox runs on,
i.e. 10.9 and above, then you don't need to worry about anything:
The API declaration will be present in any of the supported SDKs, and you don't need any runtime checks.

If you want to use a macOS API that was added after 10.9, then you have to have a runtime check.
This requirement is completely independent of what SDK is being used for building.

The runtime check [should have the following form](https://developer.apple.com/documentation/macos_release_notes/macos_mojave_10_14_release_notes/appkit_release_notes_for_macos_10_14?language=objc#3014609)
(replace `10.14` with the appropriate version):

```objc++
if (@available(macOS 10.14, *)) {
    // Code for macOS 10.14 or later
} else {
    // Code for versions earlier than 10.14.
}
```

`@available` guards can be used in Objective-C(++) code.
(In C++ code, you can use [these `nsCocoaFeatures` methods](https://searchfox.org/mozilla-central/rev/9ad88f80aeedcd3cd7d7f63be07f577861727054/widget/cocoa/nsCocoaFeatures.h#21-27) instead.)

For each API, the API declarations in the SDK headers are annotated with `API_AVAILABLE` macros.
For example, the definition of the `NSVisualEffectMaterial` enum looks like this:

```objc++
typedef NS_ENUM(NSInteger, NSVisualEffectMaterial) {
    NSVisualEffectMaterialTitlebar = 3,
    NSVisualEffectMaterialSelection = 4,
    NSVisualEffectMaterialMenu API_AVAILABLE(macos(10.11)) = 5,
   // [...]
    NSVisualEffectMaterialSheet API_AVAILABLE(macos(10.14)) = 11,
   // [...]
} API_AVAILABLE(macos(10.10));
```

The compiler understands these annotations and makes sure that you wrap all uses of the annotated APIs
in appropriate `@available` runtime checks.

### Frameworks

In some rare cases, you need functionality from frameworks that are not available on all supported macOS versions.
Examples of this are `Metal.framework` (added in 10.11) and `MediaPlayer.framework` (added in 10.12.2).

In that case, you can either `dlopen` your framework at runtime ([like we do for MediaPlayer](https://searchfox.org/mozilla-central/rev/9ad88f80aeedcd3cd7d7f63be07f577861727054/widget/cocoa/MediaPlayerWrapper.mm#21-27)),
or you can [use `-weak_framework`](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPFrameworks/Concepts/WeakLinking.html#//apple_ref/doc/uid/20002378-107026)
[like we do for Metal](https://searchfox.org/mozilla-central/rev/9ad88f80aeedcd3cd7d7f63be07f577861727054/toolkit/library/moz.build#301-304):

```python
if CONFIG['OS_ARCH'] == 'Darwin':
    OS_LIBS += [
        # Link to Metal as required by the Metal gfx-hal backend
        '-weak_framework Metal',
    ]
```

## Using new APIs with old SDKs

If you want to use an API that was introduced after 11.0, you now have one extra thing to worry about.
In addition to the runtime check [described in the previous section](#using-macos-apis), you also
have to jump through extra hoops in order to allow the build to succeed with older SDKs, because
[we need to support building Firefox with SDK versions all the way down to the 11.0 SDK](./sdks.html#supported-sdks).

In order to make the compiler accept your code, you will need to copy some amount of the API declaration
into your own code. Copy it from the newest recent SDK you can get your hands on.
The exact procedure varies based on the type of API (enum, objc class, method, etc.),
but the general approach looks like this:

```objc++
#if !defined(MAC_OS_VERSION_12_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_12_0
@interface NSScreen (NSScreen12_0)
// https://developer.apple.com/documentation/appkit/nsscreen/3882821-safeareainsets?language=objc&changes=latest_major
@property(readonly) NSEdgeInsets safeAreaInsets;
#endif
@end
```

See the [Supporting Multiple SDKs](./sdks.html#supporting-multiple-sdks) docs for more information on the `MAC_OS_X_VERSION_MAX_ALLOWED` macro.

Keep these three things in mind:

 - Copy only what you need.
 - Wrap your declaration in `MAC_OS_X_VERSION_MAX_ALLOWED` checks so that, if an SDK is used that
   already contains these declarations, your declaration does not conflict with the declaration in the SDK.
 - Include the `API_AVAILABLE` annotations so that the compiler can protect you from accidentally
   calling the API on unsupported macOS versions.

Our current code does not always follow the `API_AVAILABLE` advice, but it should.

### Enum types and C structs

If you need a new enum type or C struct, copy the entire type declaration and wrap it in the appropriate ifdefs. Example:

```objc++
#if !defined(MAC_OS_X_VERSION_10_12_2) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12_2
typedef NS_ENUM(NSUInteger, MPNowPlayingPlaybackState) {
    MPNowPlayingPlaybackStateUnknown = 0,
    MPNowPlayingPlaybackStatePlaying,
    MPNowPlayingPlaybackStatePaused,
    MPNowPlayingPlaybackStateStopped,
    MPNowPlayingPlaybackStateInterrupted
} MP_API(ios(11.0), tvos(11.0), macos(10.12.2), watchos(5.0));
#endif
```
### New enum values for existing enum type

If the enum type itself already exists, but gained a new value, define the value in an unnamed enum:

```objc++
#if !defined(MAC_OS_X_VERSION_10_12) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
enum { NSVisualEffectMaterialSelection = 4 };
#endif
```

(This is an example of an interesting case: `NSVisualEffectMaterialSelection` is available starting with
macOS 10.10, but it's only defined in SDKs starting with the 10.12 SDK.)

### Objective-C classes

For a new Objective-C class, copy the entire `@interface` declaration and wrap it in the appropriate ifdefs.

I haven't personally tested this. If this does not compile (or maybe link?), you can use the following workaround:

 - Define your methods and properties as a category on `NSObject`.
 - Look up the class at runtime using `NSClassFromString()`.
 - If you need to create a subclass, do it at runtime using `objc_allocateClassPair` and `class_addMethod`.
   [Here's an example of that.](https://searchfox.org/mozilla-central/rev/9ad88f80aeedcd3cd7d7f63be07f577861727054/widget/cocoa/VibrancyManager.mm#44-60)

### Objective-C properties and methods on an existing class

If an Objective-C class that already exists gains a new method or property, you can "add" it to the
existing class declaration with the help of a [category](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/CustomizingExistingClasses/CustomizingExistingClasses.html):

```objc++
@interface ExistingClass (YourMadeUpCategoryName)
// methods and properties here
@end
```

### Functions

With free-standing functions I'm not entirely sure what to do.
In theory, copying the declarations from the new SDK headers should work. Example:

```objc++
extern "C" {
  __attribute__((warn_unused_result)) bool
SecTrustEvaluateWithError(SecTrustRef trust, CFErrorRef _Nullable * _Nullable CF_RETURNS_RETAINED error)
    API_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0));

  __nullable
CFDataRef SecCertificateCopyNormalizedSubjectSequence(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12_4, __IPHONE_10_3);
}
```

I'm not sure what the linker or the dynamic linker do when the symbol is not available.
Does this require [`__attribute__((weak_import))` annotations](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPFrameworks/Concepts/WeakLinking.html#//apple_ref/doc/uid/20002378-107262-CJBJAEID)?

And maybe this is where .tbd files in the SDK come in? So that the linker knows which symbols to allow?
So then that part cannot be worked around by copying code from headers.

Anyway, what always works is the pure runtime approach:

 1. Define types for the functions you need, but not the functions themselves.
 2. At runtime, look up the functions using `dlsym`.

## Notes on Rust

If you call macOS APIs from Rust code, you're kind of on your own. Apple does not provide any Rust
"headers", so there isn't really an SDK to speak of. So you have to supply your own API declarations
anyway, regardless of what SDK is being used for building.

In a way, you're side-stepping some of the build time trouble. You don't need to worry about any
`#ifdefs` because there are no system headers you could conflict with.

On the other hand, you still need to worry about API availability at runtime.
And in Rust, there are no [availability attributes](https://clang.llvm.org/docs/AttributeReference.html#availability)
on your API declarations, and there are no
[`@available` runtime check helpers](https://clang.llvm.org/docs/LanguageExtensions.html#objective-c-available),
and the compiler cannot warn you if you call APIs outside of availability checks.
