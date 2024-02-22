export const kTestFolderName = "fetch-tests";

// Use RequestInit's priority if specified, or RequestionInfo's priority otherwise.
const kExpectedRequestsOfFetchAPI = [
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
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?5",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?6",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.css?7",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?8",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.css?9",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?10",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.css?11",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?12",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?13",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "dummy.css?14",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_HIGH
    },
    {   fileNameAndSuffix: "dummy.css?15",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?16",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
];

const kExpectedRequestsOfFetchAPIDisabled = [
    {   fileNameAndSuffix: "dummy.css?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?5",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?6",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?7",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?8",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?9",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?10",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?11",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?12",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?13",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?14",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?15",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
    {   fileNameAndSuffix: "dummy.css?16",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_NORMAL
    },
];

export const kTestData = [
    {   testFileName: "fetch-init.h2.html",
        expectedRequests: kExpectedRequestsOfFetchAPI
    }
];

export const kTestDataDisabled = [
    {   testFileName: "fetch-init.h2.html",
        expectedRequests: kExpectedRequestsOfFetchAPIDisabled
    }
];
