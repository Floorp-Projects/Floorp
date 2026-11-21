// SPDX-License-Identifier: MPL-2.0
// CommonJS lets actions/github-script and the Deno CLI share the lookup policy.

/** @typedef {import("./fdh_review.ts").ReviewCommentInput} ReviewCommentInput */

const FDH_REVIEW_RECEIPT_MARKER = "fdh-review-receipt:v1";
const TRUSTED_RECEIPT_AUTHOR_ASSOCIATIONS = Object.freeze([
  "OWNER",
  "MEMBER",
  "COLLABORATOR",
]);

/**
 * Fetch permissions only for eligible receipt authors, once per GitHub login.
 * The caller supplies the REST transport and translates a 404 to null.
 * @param {readonly ReviewCommentInput[]} comments
 * @param {(login: string) => Promise<unknown>} lookup
 * @returns {Promise<ReviewCommentInput[]>}
 */
async function loadReceiptAuthorPermissions(comments, lookup) {
  /** @type {Map<string, string | null>} */
  const permissions = new Map();
  const enriched = [];
  for (const comment of comments) {
    const author = comment.author;
    if (
      !author || !comment.body?.includes(FDH_REVIEW_RECEIPT_MARKER) ||
      !TRUSTED_RECEIPT_AUTHOR_ASSOCIATIONS.includes(
        (comment.authorAssociation ?? "").toUpperCase(),
      )
    ) {
      enriched.push(comment);
      continue;
    }
    const key = author.toLowerCase();
    if (!permissions.has(key)) {
      const permission = await lookup(author);
      if (
        permission !== null && permission !== "none" && permission !== "read" &&
        permission !== "write" && permission !== "admin"
      ) {
        throw new Error("unexpected collaborator permission response");
      }
      permissions.set(key, permission);
    }
    enriched.push({ ...comment, authorPermission: permissions.get(key) });
  }
  return enriched;
}

module.exports = {
  FDH_REVIEW_RECEIPT_MARKER,
  TRUSTED_RECEIPT_AUTHOR_ASSOCIATIONS,
  loadReceiptAuthorPermissions,
};
