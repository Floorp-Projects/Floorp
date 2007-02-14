/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the trace-malloc Allocation Dump Reader (adreader).
 *
 * The Initial Developer of the Original Code is L. David Baron.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>

#define ADLOG_ENTRY_BLOCK_SIZE 4096

class ADLog {

public:

    // Use typedef in case somebody wants to process 64-bit output on a
    // 32-bit machine someday.
    typedef const char* Pointer;

    struct Entry {
        Pointer address;
        const char *type;

        const char *data;         // The contents of the memory.
        size_t datasize;

        const char *allocation_stack;
    };

    ADLog();
    ~ADLog();

    /*
     * Returns false on failure and true on success.
     */
    bool Read(const char *aFilename);

private:
    // Link structure for a circularly linked list.
    struct EntryBlock;
    struct EntryBlockLink {
        EntryBlock *mPrev;
        EntryBlock *mNext;
    };

    struct EntryBlock : public EntryBlockLink {
        Entry entries[ADLOG_ENTRY_BLOCK_SIZE];
    };

    size_t mEntryCount;
    EntryBlockLink mEntries;

public:

    class const_iterator {
        private:
            // Only |ADLog| member functions can construct iterators.
            friend class ADLog;
            const_iterator(EntryBlock *aBlock, size_t aOffset);

        public:
            const Entry* operator*() { return mCur; }
            const Entry* operator->() { return mCur; }

            const_iterator& operator++();
            const_iterator& operator--();

            bool operator==(const const_iterator& aOther) const {
                return mCur == aOther.mCur;
            }

            bool operator!=(const const_iterator& aOther) const {
                return mCur != aOther.mCur;
            }

        private:
            void SetBlock(EntryBlock *aBlock) {
                mBlock = aBlock;
                mBlockStart = aBlock->entries;
                mBlockEnd = aBlock->entries + ADLOG_ENTRY_BLOCK_SIZE;
            }

            EntryBlock *mBlock;
            Entry *mCur, *mBlockStart, *mBlockEnd;

            // Not to be implemented.
            const_iterator operator++(int);
            const_iterator operator--(int);
    };

    const_iterator begin() {
        return const_iterator(mEntries.mNext, 0);
    }
    const_iterator end() {
        return const_iterator(mEntries.mPrev,
                              mEntryCount % ADLOG_ENTRY_BLOCK_SIZE);
    }

    size_t count() { return mEntryCount; }
};
