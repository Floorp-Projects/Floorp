/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.validation;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

public class BookmarkValidator {
    private List<BookmarkRecord> localRecords;
    private List<BookmarkRecord> remoteRecords;

    private Map<String, BookmarkRecord> remoteGUIDToRecord = new HashMap<>();


    /**
     * Construct a bookmark validator from local and remote records
     * */
    public BookmarkValidator(List<BookmarkRecord> localRecords, List<BookmarkRecord> remoteRecords) {
        this.localRecords = localRecords;
        this.remoteRecords = remoteRecords;
        for (BookmarkRecord r : remoteRecords) {
            remoteGUIDToRecord.put(r.guid, r);
        }
    }

    private void checkServerFolder(BookmarkRecord r, BookmarkValidationResults results) {
        if (r.children == null) {
            return;
        }
        HashSet<String> seenChildGUIDs = new HashSet<>();

        for (int i = 0; i < r.children.size(); ++i) {
            String childGUID = (String) r.children.get(i);
            if (seenChildGUIDs.contains(childGUID)) {
                results.duplicateChildren.add(
                        new BookmarkValidationResults.ParentChildPair(r.guid, childGUID)
                );
                continue;
            }
            seenChildGUIDs.add(childGUID);

            BookmarkRecord child = remoteGUIDToRecord.get(childGUID);

            if (child == null) {
                results.missingChildren.add(
                        new BookmarkValidationResults.ParentChildPair(r.guid, childGUID)
                );
                continue;
            }

            List<String> parentsContainingChild = results.multipleParents.get(childGUID);
            if (parentsContainingChild == null) {
                // Common case, we've never seen a parent with this child before, so we add a
                // record indicating we've seen one, and what the id was. If there aren't any
                // other parents of this record, we clean this up before returning the results
                // to the validator's caller (in cleanupValidationResults).
                parentsContainingChild = new ArrayList<>();
                parentsContainingChild.add(r.guid);
                results.multipleParents.put(childGUID, parentsContainingChild);
            } else {
                parentsContainingChild.add(r.guid);
            }

            if (child.deleted) {
                results.deletedChildren.add(
                        new BookmarkValidationResults.ParentChildPair(r.guid, childGUID)
                );
                continue;
            }

            if (!child.parentID.equals(r.guid)) {
                results.parentChildMismatches.add(
                        new BookmarkValidationResults.ParentChildPair(r.guid, childGUID)
                );
            }
        }
    }

    // Check for errors with the parent that aren't covered (or that might only be partially
    // covered) by the checks in `checkServerFolder`.
    private void checkServerParent(BookmarkRecord record, BookmarkValidationResults results) {
        String parentID = record.parentID;
        if (parentID.equals("places")) {
            // Parent is the places root, so we don't care.
            return;
        }
        BookmarkRecord listedParent = remoteGUIDToRecord.get(parentID);
        if (listedParent == null) {
            results.orphans.add(
                    new BookmarkValidationResults.ParentChildPair(parentID, record.guid)
            );
            return;
        }
        if (listedParent.deleted) {
            results.deletedParents.add(
                    new BookmarkValidationResults.ParentChildPair(parentID, record.guid)
            );
        } else if (!listedParent.isFolder()) {
            results.parentNotFolder.add(
                    new BookmarkValidationResults.ParentChildPair(parentID, record.guid)
            );
        } else {
            boolean foundChild = false;
            for (int i = 0; i < listedParent.children.size() && !foundChild; ++i) {
                String childGUID = (String) listedParent.children.get(i);
                foundChild = childGUID.equals(record.guid);
            }
            if (!foundChild) {
                results.parentChildMismatches.add(
                        new BookmarkValidationResults.ParentChildPair(parentID, record.guid)
                );
            }
        }
    }

    private void inspectServerRecords(BookmarkValidationResults results) {
        for (BookmarkRecord record : remoteRecords) {
            if (record.deleted) {
                continue;
            }
            if (record.guid.equals("places")) {
                results.rootOnServer = true;
                continue;
            }
            if (record.isFolder()) {
                checkServerFolder(record, results);
            }
            checkServerParent(record, results);
        }
    }

    private static class ClientServerPair {
        BookmarkRecord client;
        BookmarkRecord server;
        ClientServerPair(BookmarkRecord client, BookmarkRecord server) {
            this.client = client;
            this.server = server;
        }
    }

    // Should return false if one or both are missing (and so further comparisons are meaningless.
    // If they're inconsistent, the error should be recorded in `results`.
    private boolean checkMissing(ClientServerPair p, BookmarkValidationResults results) {

        boolean clientExists = p.client != null && !p.client.deleted;
        boolean serverExists = p.server != null && !p.server.deleted;

        boolean serverTombstone = p.server != null && p.server.deleted;

        if (clientExists && !serverExists) {
            if (serverTombstone) {
                results.serverDeleted.add(p.client.guid);
            } else {
                results.serverMissing.add(p.client.guid);
            }
        } else if (serverExists && !clientExists) {
            results.clientMissing.add(p.server.guid);
            // Should we distinguish between deleted and missing here? Desktop doesn't,
            // so to keep consistent metrics we don't either, but maybe both should?
        }

        return clientExists && serverExists;
    }

    // Returns true if it found any structural differences, and records them in `results.
    private boolean checkStructuralDifferences(ClientServerPair p, BookmarkValidationResults results) {
        boolean sawDiff = false;
        if (!p.client.parentID.equals(p.server.parentID)) {
            results.structuralDifferenceParentIDs.add(p.client.guid);
            // Just record we saw it, since we still want to check the childGUIDs
            sawDiff = true;
        }

        if (p.client.children != null && p.server.children != null) {
            if (p.client.children.size() == p.server.children.size()) {
                for (int i = 0; i < p.client.children.size(); ++i) {
                    String clientChildGUID = (String) p.client.children.get(i);
                    String serverChildGUID = (String) p.server.children.get(i);
                    if (!clientChildGUID.equals(serverChildGUID)) {
                        results.structuralDifferenceChildGUIDs.add(p.client.guid);
                        sawDiff = true;
                        break;
                    }
                }
            } else {
                // They have different sizes, so the contents must be different.
                sawDiff = true;
                results.structuralDifferenceChildGUIDs.add(p.client.guid);
            }
        }

        return sawDiff;
    }

    // Avoid false positive entries in "differences" caused by our use of congruentWith
    private BookmarkRecord normalizeRecord(BookmarkRecord r) {
        if (r.collection == null) {
            r.collection = "bookmarks";
        }
        if (r.tags == null) {
            // Desktop considers these the same and android doesn't. It's unclear if this is a bug.
            r.tags = new JSONArray();
        }
        return r;
    }

    private void compareClientWithServer(BookmarkValidationResults results) {
        Map<String, ClientServerPair> pairsById = new HashMap<>();
        for (BookmarkRecord r : remoteRecords) {
            pairsById.put(r.guid, new ClientServerPair(null, normalizeRecord(r)));
        }
        for (BookmarkRecord r : localRecords) {
            if (r.deleted) {
                continue;
            }
            ClientServerPair p = pairsById.get(r.guid);
            if (p != null) {
                p.client = normalizeRecord(r);
            } else {
                pairsById.put(r.guid, new ClientServerPair(normalizeRecord(r), null));
            }
        }
        for (Entry<String, ClientServerPair> e : pairsById.entrySet()) {
            ClientServerPair p = e.getValue();
            if (!checkMissing(p, results)) {
                continue;
            }

            // Skip checking for differences if we see a structural difference, since congruentWith
            // checks structural differences as well.
            boolean sawStructuralDifference = checkStructuralDifferences(p, results);

            if (!sawStructuralDifference &&
                    (!p.client.congruentWith(p.server) || !p.server.congruentWith(p.client))) {
                results.differences.add(p.client.guid);
            }
        }
    }

    // Remove anything in the validation results that shouldn't be reported as an error,
    // e.g. "multipleParents" with only one item in the list
    private void cleanupValidationResults(BookmarkValidationResults results) {
        Map<String, List<String>> filteredMultipleParents = new HashMap<>();
        for (Entry<String, List<String>> entry : results.multipleParents.entrySet()) {
            if (entry.getValue().size() >= 2) {
                filteredMultipleParents.put(entry.getKey(), entry.getValue());
            }
        }
        results.multipleParents = filteredMultipleParents;
    }

    private BookmarkValidationResults validate() {
        BookmarkValidationResults results = new BookmarkValidationResults();
        inspectServerRecords(results);
        compareClientWithServer(results);
        cleanupValidationResults(results);
        return results;
    }

    /**
     * Instantiate a validator from client and server records, and perform validation.
     */
    public static BookmarkValidationResults validateClientAgainstServer(List<BookmarkRecord> client, List<BookmarkRecord> server) {
        BookmarkValidator v = new BookmarkValidator(client, server);
        return v.validate();
    }

    /**
     * Perform the server-side portion of the validation only.
     */
    public static BookmarkValidationResults validateServer(List<BookmarkRecord> server) {
        BookmarkValidator v = new BookmarkValidator(new ArrayList<BookmarkRecord>(), server);
        BookmarkValidationResults results = new BookmarkValidationResults();
        v.inspectServerRecords(results);
        v.compareClientWithServer(results);
        return v.validate();
    }
}
