export const kTestFolderName = "image-tests";

// The internal priorities are specified as implementation-defined,
// (https://fetch.spec.whatwg.org/#concept-fetch, step 15). For web-
// compatibility, Chromium's mapping is chosen
// (https://web.dev/articles/fetch-priority#browser_priority_and_fetchpriority).
const kExpectedRequests = [
    {   fileNameAndSuffix: "square_25px_x_25px.png?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "square_25px_x_25px.png?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "square_25px_x_25px.png?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "square_25px_x_25px.png?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
];

export const kTestData = [
    {   testFileName: "image-dynamic-load.h2.html",
        expectedRequests: kExpectedRequests
    },
];
