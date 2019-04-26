// Type A) Parser blocking script loads will have an mContentPolicyType
//         indicating a script load and a nsIClassOfService::Leader class of service.
// Type B) I couldn’t find a class flag that corresponds to synchronous loads
//         that block the load event, but I think a simple way to determine
//         which ones they are is to examine whether the channel belongs to a
//         load group and do not have a LOAD_BACKGROUND load flag.
//         If that condition holds, then it is blocking the load event of some document.
// Type C) I think a simple way to find channels that don’t block document loads
//         (e.g. XHR and fetch) is to look for those which are in a load group
//         and have a LOAD_BACKGROUND load flag.


// Y=0 - all requests/connections/bytes that are first party.
DEFINE_CATEGORY(N1, 0)

// Y=1 - all requests/connections/bytes that are third party
// but don’t fall into other categories
DEFINE_CATEGORY(N3Oth, 1)

// Y=2 - all requests/connections/bytes associated with third party loads that
// match the Analytics/Social/Advertising (Basic) Category and have a load of type (A)
DEFINE_CATEGORY(N3BasicLead, 2)

// Y=3 - all requests/connections/bytes associated with third party loads that
// match the Analytics/Social/Advertising (Basic) Category and have a load of type (B)
DEFINE_CATEGORY(N3BasicBg, 3)

// Y=4 - all requests/connections/bytes associated with third party loads that
// match the Analytics/Social/Advertising (Basic) Category and have a load of type (C)
DEFINE_CATEGORY(N3BasicOth, 4)

// Y=5 - all requests/connections/bytes associated with third party loads that
// match the Content Category and have a load of type (A)
DEFINE_CATEGORY(N3ContentLead, 5)

// Y=6 - all requests/connections/bytes associated with third party loads that
// match the Content Category and have a load of type (B)
DEFINE_CATEGORY(N3ContentBg, 6)

// Y=7 - all requests/connections/bytes associated with third party loads that
// match the Content Category and have a load of type (C)
DEFINE_CATEGORY(N3ContentOth, 7)

// Y=8 - all requests/connections/bytes associated with third party loads that
// match the Fingerprinting Category and have a load of type (A)
DEFINE_CATEGORY(N3FpLead, 8)

// Y=9 - all requests/connections/bytes associated with third party loads that
// match the Fingerprinting Category and have a load of type (B)
DEFINE_CATEGORY(N3FpBg, 9)

// Y=10 - all requests/connections/bytes associated with third party loads that
// match the Fingerprinting Category and have a load of type (C)
DEFINE_CATEGORY(N3FpOth, 10)

// Y=11 - private mode and all requests/connections/bytes that are first party.
DEFINE_CATEGORY(P1, 11)

// Y=12 - private mode and all requests/connections/bytes that are third party
// but don’t fall into other categories
DEFINE_CATEGORY(P3Oth, 12)

// Y=13 - private mode and all requests/connections/bytes associated with
// third party loads that match the Analytics/Social/Advertising (Basic) Category
// and have a load of type (A)
DEFINE_CATEGORY(P3ContentLead, 13)

// Y=14 - private mode and all requests/connections/bytes associated with
// third party loads that match the Analytics/Social/Advertising (Basic) Category
// and have a load of type (B)
DEFINE_CATEGORY(P3ContentBg, 14)

// Y=15 - private mode and all requests/connections/bytes associated with
// third party loads that match the Analytics/Social/Advertising (Basic) Category
// and have a load of type (C)
DEFINE_CATEGORY(P3ContentOth, 15)

// Y=16 - private mode and all requests/connections/bytes associated with
// third party loads that match the Content Category and have a load of type (A)
DEFINE_CATEGORY(P3BasicLead, 16)

// Y=17 - private mode and all requests/connections/bytes associated with
// third party loads that match the Content Category and have a load of type (B)
DEFINE_CATEGORY(P3BasicBg, 17)

// Y=18 - private mode and all requests/connections/bytes associated with
// third party loads that match the Content Category and have a load of type (C)
DEFINE_CATEGORY(P3BasicOth, 18)

// Y=19 - private mode and all requests/connections/bytes associated with
// third party loads that match the Fingerprinting Category and have a load of type (A)
DEFINE_CATEGORY(P3FpLead, 19)

// Y=20 - private mode and all requests/connections/bytes associated with
// third party loads that match the Fingerprinting Category and have a load of type (B)
DEFINE_CATEGORY(P3FpBg, 20)

// Y=21 - private mode and all requests/connections/bytes associated with
// third party loads that match the Fingerprinting Category and have a load of type (C)
DEFINE_CATEGORY(P3FpOth, 21)
