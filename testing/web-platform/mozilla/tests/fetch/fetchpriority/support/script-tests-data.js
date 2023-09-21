const kFetchPriorityLowRequestFileNameAndSuffix = "dummy.js?1";
const kFetchPriorityHighRequestFileNameAndSuffix = "dummy.js?2";
const kFetchPriorityAutoRequestFileNameAndSuffix = "dummy.js?3";
const kNoFetchPriorityRequestFileNameAndSuffix = "dummy.js?4";

const kExpectedJSRequestsForNonAutoFetchPriority = [
    {   fileNameAndSuffix: kFetchPriorityLowRequestFileNameAndSuffix,
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: kFetchPriorityHighRequestFileNameAndSuffix,
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: kFetchPriorityAutoRequestFileNameAndSuffix,
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: kNoFetchPriorityRequestFileNameAndSuffix,
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    }
];

export const kTestFolderName = "script-tests";

export const kTestData = [
    {   testFileName: "script-initial-load.h2.html",
        expectedRequests: kExpectedJSRequestsForNonAutoFetchPriority
    },
    {   testFileName: "async-script-initial-load.h2.html",
        expectedRequests: kExpectedJSRequestsForNonAutoFetchPriority
    },
    {   testFileName: "deferred-script-initial-load.h2.html",
        expectedRequests: kExpectedJSRequestsForNonAutoFetchPriority
    },
    {   testFileName: "module-script-initial-load.h2.html",
        expectedRequests: kExpectedJSRequestsForNonAutoFetchPriority
    },
    {   testFileName: "async-module-script-initial-load.h2.html",
        expectedRequests: kExpectedJSRequestsForNonAutoFetchPriority
    },
    // Dynamic insertion executes non-speculative-parsing
    // (https://developer.mozilla.org/en-US/docs/Glossary/speculative_parsing)
    // code paths.
    {   testFileName: "script-dynamic-insertion.h2.html",
        expectedRequests: kExpectedJSRequestsForNonAutoFetchPriority
    },
    {   testFileName: "module-script-dynamic-insertion.h2.html",
        expectedRequests: kExpectedJSRequestsForNonAutoFetchPriority
    }
];
