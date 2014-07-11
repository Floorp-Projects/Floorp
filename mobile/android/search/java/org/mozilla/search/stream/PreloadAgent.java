/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.stream;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A temporary agent for loading cards into the pre-load card stream.
 * <p/>
 * When we have more agents, we'll want to put an agent manager between the PreSearchFragment
 * and the set of all agents. See autocomplete.AutoCompleteFragmentManager.
 */
public class PreloadAgent {

    public static final List<TmpItem> ITEMS = new ArrayList<TmpItem>();

    private static final Map<String, TmpItem> ITEM_MAP = new HashMap<String, TmpItem>();

    static {
        addItem(new TmpItem("1", "Pre-load item1"));
        addItem(new TmpItem("2", "Pre-load item2"));
    }

    private static void addItem(TmpItem item) {
        ITEMS.add(item);
        ITEM_MAP.put(item.id, item);
    }

    public static class TmpItem {
        public final String id;
        public final String content;

        public TmpItem(String id, String content) {
            this.id = id;
            this.content = content;
        }

        @Override
        public String toString() {
            return content;
        }
    }
}
