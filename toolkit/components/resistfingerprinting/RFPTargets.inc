/*
 * Once allocated, a string or value cannot ever be changed or re-used
 */

/*
 * This value is the default argument value, to allow all callsites to ShouldRFP
 * continue working. We will eventually remove the default argument, and comment
 * out this enum value.
 */
ITEM_VALUE(Unknown, 0xFFFF)
