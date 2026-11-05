import LinkifyIt from 'linkify-it';

// Custom TLDs Proton wants linkified in addition to linkify-it's built-in list.
export const CUSTOM_TLDS = ['cloud', 'team'];

// Single shared LinkifyIt instance. linkify-it caches its compiled regex on
// first use, so reusing one instance avoids re-building it for every transform.
export const linkifyInstance = new LinkifyIt();
linkifyInstance.tlds(CUSTOM_TLDS, true);

// Don't auto-link bare hostnames (e.g. `proton.com`, `example.com/path`). We
// only want clickable links when the sender explicitly wrote a protocol — too
// much innocent prose contains domain-shaped tokens. Fuzzy emails stay on:
// `name@domain` is unambiguous enough to keep clickable.
linkifyInstance.set({ fuzzyLink: false });

// Schemes we don't want clickable in mail.
//   - http: cleartext; we'd rather render it as plain text than auto-link it.
//   - ftp:  cleartext; poorly supported in modern browsers.
//
// We filter at the match level rather than calling `linkifyInstance.add(s, null)`
// because linkify-it's default schema table aliases https: → http: and
// ftp: → http:. Disabling the alias target leaves the alias entry in the
// internal `__compiled__` map with a null validate, which crashes `match()`.
const BLOCKED_SCHEMES = new Set(['http:', 'ftp:']);

/**
 * Drop matches whose scheme is blocked. Pass-through filter — call this on the
 * result of `linkifyInstance.match(text)` before consuming it.
 */
export const filterAllowedMatches = (matches: LinkifyIt.Match[] | null): LinkifyIt.Match[] => {
    if (!matches) {
        return [];
    }
    return matches.filter((match) => !BLOCKED_SCHEMES.has(match.schema));
};
