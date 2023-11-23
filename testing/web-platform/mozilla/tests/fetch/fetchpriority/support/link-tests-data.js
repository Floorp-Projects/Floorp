export const kTestFolderName = "link-tests";

const kExpectedRequestsOfLoadStylesheet = [
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
    {   fileNameAndSuffix: "dummy.css?5",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?6",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?7",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?8",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    // `media=print` doesn't match the environment
    // (https://html.spec.whatwg.org/multipage/common-microsyntaxes.html#matches-the-environment)
    // hence all internal priorities should be low.
    {   fileNameAndSuffix: "dummy.css?9",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?10",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?11",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?12",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
];

const kExpectedRequestsOfLinkPreloadFont = [
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
];
const kExpectedRequestsOfLinkPreloadImage = [
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
];

const kExpectedRequestsOfLinkPreloadFetch = [
    {   fileNameAndSuffix: "dummy.txt?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.txt?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.txt?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.txt?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
];

const kExpectedRequestsOfPreloadScript = [
    {   fileNameAndSuffix: "dummy.js?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.js?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.js?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.js?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
];

const kExpectedRequestsOfLinkPreloadStyle = [
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

const kExpectedRequestsOfModulepreload = kExpectedRequestsOfPreloadScript;

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
  "|header(Link,<dummy.css?4>; rel=stylesheet,True)" +
  "|header(Link,<dummy.css?5>; rel=\"alternate stylesheet\"; title=5; fetchpriority=low,True)" +
  "|header(Link,<dummy.css?6>; rel=\"alternate stylesheet\"; title=6; fetchpriority=high,True)" +
  "|header(Link,<dummy.css?7>; rel=\"alternate stylesheet\"; title=7; fetchpriority=auto,True)" +
  "|header(Link,<dummy.css?8>; rel=\"alternate stylesheet\"; title=8,True)" +
  "|header(Link,<dummy.css?9>; rel=stylesheet; fetchpriority=low; media=print,True)" +
  "|header(Link,<dummy.css?10>; rel=stylesheet; fetchpriority=high; media=print,True)" +
  "|header(Link,<dummy.css?11>; rel=stylesheet; fetchpriority=auto; media=print,True)" +
  "|header(Link,<dummy.css?12>; rel=stylesheet; media=print,True)";

const kPipeHeaderPreloadFontLinks =
  "=header(Link,<dummy.font?1>; rel=preload; as=font; fetchpriority=low,True)" +
  "|header(Link,<dummy.font?2>; rel=preload; as=font; fetchpriority=high,True)" +
  "|header(Link,<dummy.font?3>; rel=preload; as=font; fetchpriority=auto,True)" +
  "|header(Link,<dummy.font?4>; rel=preload; as=font,True)";

const kPipeHeaderPreloadImageLinks =
  "=|header(Link,<dummy.image?1>; rel=preload; as=image; fetchpriority=low,True)" +
  "|header(Link,<dummy.image?2>; rel=preload; as=image; fetchpriority=high,True)" +
  "|header(Link,<dummy.image?3>; rel=preload; as=image; fetchpriority=auto,True)" +
  "|header(Link,<dummy.image?4>; rel=preload; as=image,True)";

const kPipeHeaderPreloadFetchLinks =
  "=header(Link,<dummy.txt?1>; rel=preload; as=fetch; fetchpriority=low,True)" +
  "|header(Link,<dummy.txt?2>; rel=preload; as=fetch; fetchpriority=high,True)" +
  "|header(Link,<dummy.txt?3>; rel=preload; as=fetch; fetchpriority=auto,True)" +
  "|header(Link,<dummy.txt?4>; rel=preload; as=fetch,True)";

const kPipeHeaderPreloadScriptLinks =
  "=header(Link,<dummy.js?1>; rel=preload; as=script; fetchpriority=low,True)" +
  "|header(Link,<dummy.js?2>; rel=preload; as=script; fetchpriority=high,True)" +
  "|header(Link,<dummy.js?3>; rel=preload; as=script; fetchpriority=auto,True)" +
  "|header(Link,<dummy.js?4>; rel=preload; as=script,True)";

  const kPipeHeaderPreloadStyleLinks =
  "=header(Link,<dummy.css?1>; rel=preload; as=style; fetchpriority=low,True)" +
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
    {   testFileName: "link-initial-preload-image.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadImage
    },
    {   testFileName: "link-initial-preload-font.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadFont
    },
    {   testFileName: "link-initial-preload-fetch.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadFetch
    },
    {   testFileName: "link-initial-preload-script.h2.html",
        expectedRequests: kExpectedRequestsOfPreloadScript
    },
    {   testFileName: "link-initial-preload-style.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadStyle
    },
    {   testFileName: "link-dynamic-preload-image.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadImage
    },
    {   testFileName: "link-dynamic-preload-font.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadFont
    },
    {   testFileName: "link-dynamic-preload-fetch.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadFetch
    },
    {   testFileName: "link-dynamic-preload-script.h2.html",
        expectedRequests: kExpectedRequestsOfPreloadScript
    },
    {   testFileName: "link-dynamic-preload-style.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreloadStyle
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderPreloadImageLinks,
        expectedRequests: kExpectedRequestsOfLinkPreloadImage
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderPreloadFontLinks,
        expectedRequests: kExpectedRequestsOfLinkPreloadFont
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderPreloadFetchLinks,
        expectedRequests: kExpectedRequestsOfLinkPreloadFetch
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderPreloadScriptLinks,
        expectedRequests: kExpectedRequestsOfPreloadScript
    },
    {   testFileName: "link-header.h2.html?pipe" + kPipeHeaderPreloadStyleLinks,
        expectedRequests: kExpectedRequestsOfLinkPreloadStyle
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
