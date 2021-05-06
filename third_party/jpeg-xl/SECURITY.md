# Security and Vulnerability Policy for JPEG XL

The current focus of the reference implementation is to provide a vehicle for
evaluating the JPEG XL codec compression density, quality, features and its
actual performance on different platforms. With this focus in mind we provide
source code releases with improvements on performance and quality so developers
can evaluate the codec.

At this time, **we don't provide security and vulnerability support** for any
of these releases. This means that the source code may contain bugs, including
security bugs, that may be added or fixed between releases and will **not** be
individually documented. All of these
[releases](https://gitlab.com/wg1/jpeg-xl/-/releases) include the following
note to that effect:

* Note: This release is for evaluation purposes and may contain bugs, including
  security bugs, that will *not* be individually documented when fixed. Always
  prefer to use the latest release. Please provide feedback and report bugs
  [here](https://gitlab.com/wg1/jpeg-xl/-/issues).

To be clear, this means that because a release doesn't mention any CVE it
doesn't mean that no security issues in previous versions were fixed. You should
assume that any previous release contains security issues if that's a concern
for your use case.

This however doesn't impede you from evaluating the codec with your own trusted
inputs, such as `.jxl` you encoded yourself, or when taking appropriate measures
for your application like sandboxing if processing untrusted inputs.

## Future plans

To help our users and developers integrating this implementation into their
software we plan to provide support for security and vulnerability tracking of
this implementation in the future.

When we can provide such support we will update this Policy with the details and
expectations and clearly mention that fact in the release notes.
