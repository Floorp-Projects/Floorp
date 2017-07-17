/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.validation;


import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Stores data that describes the set of problems found during bookmark validation.
 * It stores enough info so that meaningful action to repair or visualize the problems could
 * be taken in the future, although at the moment the primary use is to generate data for
 * telemetry.
 *
 * Currently it only holds a subset of what is stored by desktop's implementation.
 */
public class BookmarkValidationResults extends ValidationResults {
    /**
     * Simple class used to store a pair of guids that refer to a parent/child relationship.
     */
    public static class ParentChildPair {
        public String parent;
        public String child;

        public ParentChildPair(String parentID, String childID) {
            this.parent = parentID;
            this.child = childID;
        }

        // There are a few cases where it's difficult for the validator to guarantee uniqueness of
        // it's complaints, and so it helps to be able to store these in a Map/Set by value.
        // The implementations were automatically generated.
        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || getClass() != o.getClass()) {
                return false;
            }

            ParentChildPair that = (ParentChildPair) o;

            if (!parent.equals(that.parent)) {
                return false;
            }
            return child.equals(that.child);

        }

        @Override
        public int hashCode() {
            int result = parent.hashCode();
            result = 31 * result + child.hashCode();
            return result;
        }
    }

    /**
     * True if we saw the root record (the record with a guid of "places") on the server.
     */
    public boolean rootOnServer = false;

    /**
     * List of records where the parent refers to a child that does not exist.
     */
    public List<ParentChildPair> missingChildren = new ArrayList<>();

    /**
     * List of records where the parent refers to a child that was deleted.
     */
    public List<ParentChildPair> deletedChildren = new ArrayList<>();

    /**
     * List of records where the parent was deleted but the child was not.
     */
    public List<ParentChildPair> deletedParents = new ArrayList<>();

    /**
     * List of records with either no parent, or where the parent could not be found.
     */
    public List<ParentChildPair> orphans = new ArrayList<>();

    /**
     * List of records who have the same child listed multiple times in their children array.
     */
    public List<ParentChildPair> duplicateChildren = new ArrayList<>();

    /**
     * List of records who refer to parents that are not folders.
     */
    public List<ParentChildPair> parentNotFolder = new ArrayList<>();

    /**
     * List of server-side records where the child's parentid and parent's children array disagree
     * with each-other.
     */
    public Set<ParentChildPair> parentChildMismatches = new HashSet<>();

    /**
     * Map of child guid to list of parent guids for records that show up in multiple parents.
     */
    public Map<String, List<String>> multipleParents = new HashMap<>();

    /**
     * Set of ids of records present on the server but missing from the client.
     */
    public Set<String> clientMissing = new HashSet<>();

    /**
     * Set of ids of records present on the client but missing from the server.
     */
    public Set<String> serverMissing = new HashSet<>();

    /**
     * List of ids of records present on the client, with tombstones on the server.
     */
    public Set<String> serverDeleted = new HashSet<>();

    /**
     * List of ids of items where the child guids differ between client and server.
     * Represents sdiff:childGUIDs, and part of structuralDifferences in the summary.
     */
    public Set<String> structuralDifferenceChildGUIDs = new HashSet<>();

    /**
     * List of ids of items where parentIDs differ between client and server.
     * Represents sdiff:parentid, and part of structuralDifferences in the summary.
     */
    public Set<String> structuralDifferenceParentIDs = new HashSet<>();

    /**
     * List of ids where the client and server disagree about something not covered by a structural
     * difference.
     */
    public Set<String> differences = new HashSet<>();


    private static void addProp(Map<String, Integer> m, String propName, int count) {
        if (count != 0) {
            m.put(propName, count);
        }
    }

    public Map<String, Integer> summarizeResults() {
        Map<String, Integer> m = new HashMap<>();
        if (rootOnServer) {
            addProp(m, "rootOnServer", 1);
        }
        addProp(m, "parentChildMismatches", parentChildMismatches.size());
        addProp(m, "missingChildren", missingChildren.size());
        addProp(m, "deletedChildren", deletedChildren.size());
        addProp(m, "deletedParents", deletedParents.size());
        addProp(m, "multipleParents", multipleParents.size());
        addProp(m, "orphans", orphans.size());
        addProp(m, "duplicateChildren", duplicateChildren.size());
        addProp(m, "parentNotFolder", parentNotFolder.size());
        addProp(m, "clientMissing", clientMissing.size());
        addProp(m, "serverMissing", serverMissing.size());
        addProp(m, "serverDeleted", serverDeleted.size());
        addProp(m, "sdiff:childGUIDs", structuralDifferenceChildGUIDs.size());
        addProp(m, "sdiff:parentid", structuralDifferenceParentIDs.size());
        addProp(m, "structuralDifferences", structuralDifferenceParentIDs.size() + structuralDifferenceChildGUIDs.size());
        addProp(m, "differences", differences.size());
        return m;
    }
}
