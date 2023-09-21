export const kTestFolderName = "link-tests";

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
    {   testFileName: "link-initial-load.h2.html",
        expectedRequests: [
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
        ]
    }
];
