export const kTestFolderName = "link-tests";

const kExpectedRequestsOfLoadStylesheet = [
    {   fileNameAndSuffix: "dummy.css?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.css?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    }
];

const kExpectedRequestsOfLinkPreload = [
    {   fileNameAndSuffix: "dummy.txt?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.txt?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.txt?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.txt?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.font?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.font?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.font?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.font?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.image?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.image?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.image?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.image?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.css?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGHEST
    },
    {   fileNameAndSuffix: "dummy.css?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGHEST
    },
    {   fileNameAndSuffix: "dummy.css?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGHEST
    },
];

const kExpectedRequestsOfModulepreload = [
    {   fileNameAndSuffix: "dummy.js?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.js?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGHEST
    },
    {   fileNameAndSuffix: "dummy.js?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGHEST
    },
    {   fileNameAndSuffix: "dummy.js?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGHEST
    }
];

const kExpectedRequestsOfPrefetch = [
    {   fileNameAndSuffix: "dummy.txt?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOWEST
    },
    {   fileNameAndSuffix: "dummy.txt?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOWEST
    },
    {   fileNameAndSuffix: "dummy.txt?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOWEST
    },
    {   fileNameAndSuffix: "dummy.txt?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOWEST
    }
];

const kPipeHeaderLinksToStylesheets =
  "=header(Link,<dummy.css?1>; rel=stylesheet; fetchpriority=low,True)" +
  "|header(Link,<dummy.css?2>; rel=stylesheet; fetchpriority=high,True)" +
  "|header(Link,<dummy.css?3>; rel=stylesheet; fetchpriority=auto,True)" +
  "|header(Link,<dummy.css?4>; rel=stylesheet,True)"

const kPipeHeaderPreloadLinks =
  // as="fetch"
  "=header(Link,<dummy.txt?1>; rel=preload; as=fetch; fetchpriority=low,True)" +
  "|header(Link,<dummy.txt?2>; rel=preload; as=fetch; fetchpriority=high,True)" +
  "|header(Link,<dummy.txt?3>; rel=preload; as=fetch; fetchpriority=auto,True)" +
  "|header(Link,<dummy.txt?4>; rel=preload; as=fetch,True)" +
  // as="font"
  "|header(Link,<dummy.font?1>; rel=preload; as=font; fetchpriority=low,True)" +
  "|header(Link,<dummy.font?2>; rel=preload; as=font; fetchpriority=high,True)" +
  "|header(Link,<dummy.font?3>; rel=preload; as=font; fetchpriority=auto,True)" +
  "|header(Link,<dummy.font?4>; rel=preload; as=font,True)" +
  // as="image"
  "|header(Link,<dummy.image?1>; rel=preload; as=image; fetchpriority=low,True)" +
  "|header(Link,<dummy.image?2>; rel=preload; as=image; fetchpriority=high,True)" +
  "|header(Link,<dummy.image?3>; rel=preload; as=image; fetchpriority=auto,True)" +
  "|header(Link,<dummy.image?4>; rel=preload; as=image,True)" +
  // as="script"
  "|header(Link,<dummy.script?1>; rel=preload; as=script; fetchpriority=low,True)" +
  "|header(Link,<dummy.script?2>; rel=preload; as=script; fetchpriority=high,True)" +
  "|header(Link,<dummy.script?3>; rel=preload; as=script; fetchpriority=auto,True)" +
  "|header(Link,<dummy.script?4>; rel=preload; as=script,True)" +
  // as="style"
  "|header(Link,<dummy.css?1>; rel=preload; as=style; fetchpriority=low,True)" +
  "|header(Link,<dummy.css?2>; rel=preload; as=style; fetchpriority=high,True)" +
  "|header(Link,<dummy.css?3>; rel=preload; as=style; fetchpriority=auto,True)" +
  "|header(Link,<dummy.css?4>; rel=preload; as=style,True)";

const kPipeHeaderModulepreloadLinks =
  "=header(Link,<dummy.js?1>; rel=modulepreload; fetchpriority=low,True)" +
  "|header(Link,<dummy.js?2>; rel=modulepreload; fetchpriority=high,True)" +
  "|header(Link,<dummy.js?3>; rel=modulepreload; fetchpriority=auto,True)" +
  "|header(Link,<dummy.js?4>; rel=modulepreload,True)";

const kPipeHeaderPrefetchLinks =
  "=header(Link,<dummy.txt?1>; rel=prefetch; fetchpriority=low,True)" +
  "|header(Link,<dummy.txt?2>; rel=prefetch; fetchpriority=high,True)" +
  "|header(Link,<dummy.txt?3>; rel=prefetch; fetchpriority=auto,True)" +
  "|header(Link,<dummy.txt?4>; rel=prefetch,True)";

// The expected internal priorites of the test data are specified as
// implementation-defined. See step 11. of
// <https://html.spec.whatwg.org/#create-a-link-request> and step 15. of
// <https://fetch.spec.whatwg.org/#concept-fetch>.
//
// The internal priorities already differ for browsers. The ones for Chromium,
// including fetchpriority's effect on them, are reported at
// <https://web.dev/fetch-priority/#browser-priority-and-fetchpriority>.
// When Gecko's internal priorities match those, the fetchpriority attribute in
// Gecko is expected to have the same effect as in Chromium. When not, applying
// "fetchpriority=low" ("high") is expected to adjust the internal priority to
// the next lower (higher) priority.
export const kTestData = [
    {   testFileName: "link-initial-load-stylesheet.h2.html",
        expectedRequests: kExpectedRequestsOfLoadStylesheet
    },
    {   testFileName: "link-dynamic-load-stylesheet.h2.html",
        expectedRequests: kExpectedRequestsOfLoadStylesheet
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderLinksToStylesheets,
        expectedRequests: kExpectedRequestsOfLoadStylesheet
    },
    {   testFileName: "link-initial-preload.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreload
    },
    {   testFileName: "link-dynamic-preload.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreload
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderPreloadLinks,
        expectedRequests: kExpectedRequestsOfLinkPreload
    },
    {   testFileName: "link-initial-modulepreload.h2.html",
        expectedRequests: kExpectedRequestsOfModulepreload
    },
    {   testFileName: "link-dynamic-modulepreload.h2.html",
        expectedRequests: kExpectedRequestsOfModulepreload
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderModulepreloadLinks,
        expectedRequests: kExpectedRequestsOfModulepreload
    },
    {   testFileName: "link-initial-prefetch.h2.html",
        expectedRequests: kExpectedRequestsOfPrefetch
    },
    {   testFileName: "link-dynamic-prefetch.h2.html",
        expectedRequests: kExpectedRequestsOfPrefetch
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderPrefetchLinks,
        expectedRequests: kExpectedRequestsOfPrefetch
    }
];
