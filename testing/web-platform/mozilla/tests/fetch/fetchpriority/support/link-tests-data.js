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
    {   testFileName: "link-initial-preload.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreload
    },
    {   testFileName: "link-dynamic-preload.h2.html",
        expectedRequests: kExpectedRequestsOfLinkPreload
    },
    {   testFileName: "link-initial-modulepreload.h2.html",
        expectedRequests: kExpectedRequestsOfModulepreload
    },
    {   testFileName: "link-dynamic-modulepreload.h2.html",
        expectedRequests: kExpectedRequestsOfModulepreload
    },
    {   testFileName: "link-initial-prefetch.h2.html",
        expectedRequests: kExpectedRequestsOfPrefetch
    },
    {   testFileName: "link-dynamic-prefetch.h2.html",
        expectedRequests: kExpectedRequestsOfPrefetch
    }
];
