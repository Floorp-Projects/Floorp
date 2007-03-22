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
