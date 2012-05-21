/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
