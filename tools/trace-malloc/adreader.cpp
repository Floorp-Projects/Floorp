/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "adreader.h"

#include <stdio.h>
#include <string.h>

ADLog::ADLog()
    : mEntryCount(0)
{
    mEntries.mNext = static_cast<EntryBlock*>(&mEntries);
    mEntries.mPrev = static_cast<EntryBlock*>(&mEntries);
}

ADLog::~ADLog()
{
    for (const_iterator entry = begin(), entry_end = end();
         entry != entry_end; ++entry) {
        free((void*) (*entry)->type);
        free((char*) (*entry)->data);
        free((char*) (*entry)->allocation_stack);
    }

    for (EntryBlock *b = mEntries.mNext, *next; b != &mEntries; b = next) {
        next = b->mNext;
        delete b;
    }
}

bool
ADLog::Read(const char* aFileName)
{
    FILE *in = fopen(aFileName, "r");
    if (!in) {
        return false;
    }

    while (!feof(in)) {
        unsigned int ptr;
        char typebuf[256];
        int datasize;
        int res = fscanf(in, "%x %s (%d)\n", &ptr, typebuf, &datasize);
        if (res == EOF)
            break;
        if (res != 3) {
            return false;
        }

        size_t data_mem_size = ((datasize + sizeof(unsigned long) - 1) /
                               sizeof(unsigned long)) * sizeof(unsigned long);
        char *data = (char*)malloc(data_mem_size);

        for (unsigned long *cur_data = (unsigned long*) data,
                       *cur_data_end = (unsigned long*) ((char*)data + data_mem_size);
             cur_data != cur_data_end; ++cur_data) {
            res = fscanf(in, " %lX\n", cur_data);
            if (res != 1) {
                return false;
            }
        }

        char stackbuf[100000];
        stackbuf[0] = '\0';

        char *stack = stackbuf;
        int len;
        do {
            fgets(stack, sizeof(stackbuf) - (stack - stackbuf), in);
            len = strlen(stack);
            stack += len;
        } while (len > 1);

        if (mEntryCount % ADLOG_ENTRY_BLOCK_SIZE == 0) {
            EntryBlock *new_block = new EntryBlock();
            new_block->mNext = static_cast<EntryBlock*>(&mEntries);
            new_block->mPrev = mEntries.mPrev;
            mEntries.mPrev->mNext = new_block;
            mEntries.mPrev = new_block;
        }

        size_t typelen = strlen(typebuf);
        char *type = (char*)malloc(typelen-1);
        strncpy(type, typebuf+1, typelen-2);
        type[typelen-2] = '\0';

        Entry *entry =
            &mEntries.mPrev->entries[mEntryCount % ADLOG_ENTRY_BLOCK_SIZE];
        entry->address = (Pointer)ptr;
        entry->type = type;
        entry->datasize = datasize;
        entry->data = data;
        entry->allocation_stack = strdup(stackbuf);

        ++mEntryCount;
    }

    return true;
}

ADLog::const_iterator::const_iterator(ADLog::EntryBlock *aBlock,
                                      size_t aOffset)
{
    SetBlock(aBlock);
    mCur = mBlockStart + aOffset;
}

ADLog::const_iterator&
ADLog::const_iterator::operator++()
{
    ++mCur;
    if (mCur == mBlockEnd) {
        SetBlock(mBlock->mNext);
        mCur = mBlockStart;
    }

    return *this;
}

ADLog::const_iterator&
ADLog::const_iterator::operator--()
{
    if (mCur == mBlockStart) {
        SetBlock(mBlock->mPrev);
        mCur = mBlockEnd;
    }
    --mCur;

    return *this;
}
