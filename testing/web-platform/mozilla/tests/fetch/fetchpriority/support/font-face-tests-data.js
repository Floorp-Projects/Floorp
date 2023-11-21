const kExpectedRequests = [
    {   fileNameAndSuffix: "dummy.font",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
];

export const kTestFolderName = "font-face-tests";

export const kTestData = [
    {   testFileName: "load-font-face-from-head.h2.html",
        expectedRequests: kExpectedRequests
    },
    {   testFileName: "load-font-face-from-worker.h2.html",
        expectedRequests: kExpectedRequests
    },
    {   testFileName: "load-font-face-from-script.h2.html",
        expectedRequests: kExpectedRequests
    },
];
