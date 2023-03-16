/*
 * Once allocated, a string or value cannot ever be changed or re-used
 */


ITEM_VALUE(TouchEvents, 0x0001)


/*
 * In certain cases, we precompute the vaqlue of ShouldRFP for e.g. a Document.
 * (This saves us more computation and casting later.)  This document will still need
 * to check whether an individual target is allowed, but the initial question of
 * "Does this document have any RFP applied to it ever?" can still be precomputed.
 * This enum value will always be included in RFPLite, so when a document asks if they
 * might have RFP enabled, it will return true.
 * (Putting this value in the overrides pref is undefined behavior and may do anything.)
 */
ITEM_VALUE(PrecomputeIsEnabled, 0xFFFE)

/*
 * This value is the default argument value, to allow all callsites to ShouldRFP
 * continue working. We will eventually remove the default argument, and comment
 * out this enum value.
 */
ITEM_VALUE(Unknown, 0xFFFF)
