export const kTestFolderName = "image-tests";

// The internal priorities are specified as implementation-defined,
// (https://fetch.spec.whatwg.org/#concept-fetch, step 15). For web-
// compatibility, Chromium's mapping is chosen
// (https://web.dev/articles/fetch-priority#browser_priority_and_fetchpriority).
const kExpectedRequestsOfInitialLoad = [
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

const kExpectedRequestsOfInitialLoadDisabled = [
    {   fileNameAndSuffix: "square_25px_x_25px.png?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "square_25px_x_25px.png?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "square_25px_x_25px.png?3",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "square_25px_x_25px.png?4",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
];

const kExpectedRequestsOfDynamicLoad = kExpectedRequestsOfInitialLoad;

const kExpectedRequestsOfDynamicLoadDisabled = kExpectedRequestsOfInitialLoadDisabled;

const kExpectedRequestsOfInitialLoadForSVGImageTagDisabled = kExpectedRequestsOfInitialLoadDisabled;

// TODO(bug 1865837): Should SVG's `<image>` element support the `fetchpriority` attribute?
const kExpectedRequestsOfInitialLoadForSVGImageTag = kExpectedRequestsOfInitialLoadForSVGImageTagDisabled;

const kExpectedRequestsOfDynamicLoadForSVGImageTagDisabled = kExpectedRequestsOfDynamicLoadDisabled;

// TODO(bug 1865837): Should SVG's `<image>` element support the `fetchpriority` attribute?
const kExpectedRequestsOfDynamicLoadForSVGImageTag = kExpectedRequestsOfDynamicLoadForSVGImageTagDisabled;

const kExpectedRequestsShapeOutsideImage = [
    {   fileNameAndSuffix: "square_25px_x_25px.png?1",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
    {   fileNameAndSuffix: "square_25px_x_25px.png?2",
        internalPriority: SpecialPowers.Ci.nsISupportsPriority.PRIORITY_LOW
    },
];

const kExpectedRequestsShapeOutsideImageDisabled = kExpectedRequestsShapeOutsideImage;

export const kTestData = [
    {   testFileName: "image-initial-load.h2.html",
        expectedRequests: kExpectedRequestsOfInitialLoad
    },
    {   testFileName: "image-dynamic-load.h2.html",
        expectedRequests: kExpectedRequestsOfDynamicLoad
    },
    {   testFileName: "image-svg-initial-load.h2.html",
        expectedRequests: kExpectedRequestsOfInitialLoadForSVGImageTag,
    },
    {   testFileName: "image-svg-dynamic-load.h2.html",
        expectedRequests: kExpectedRequestsOfDynamicLoadForSVGImageTag,
    },
    {   testFileName: "shape-outside-image.h2.html",
        expectedRequests: kExpectedRequestsShapeOutsideImage
    },
];

export const kTestDataDisabled = [
    {   testFileName: "image-initial-load.h2.html",
        expectedRequests: kExpectedRequestsOfInitialLoadDisabled
    },
    {   testFileName: "image-dynamic-load.h2.html",
        expectedRequests: kExpectedRequestsOfDynamicLoadDisabled
    },
    {   testFileName: "image-svg-initial-load.h2.html",
        expectedRequests: kExpectedRequestsOfInitialLoadForSVGImageTagDisabled,
    },
    {   testFileName: "image-svg-dynamic-load.h2.html",
        expectedRequests: kExpectedRequestsOfDynamicLoadForSVGImageTagDisabled,
    },
    {   testFileName: "shape-outside-image.h2.html",
        expectedRequests: kExpectedRequestsShapeOutsideImageDisabled
    },
];
