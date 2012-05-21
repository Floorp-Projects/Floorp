/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 
#ifndef _PAPERPS_H_
#define _PAPERPS_H_

#include "prtypes.h"
#include "nsDebug.h"

struct nsPaperSizePS_ {
    const char *name;
    float width_mm;
    float height_mm;
    bool isMetric;        // Present to the user in metric, if possible
};

class nsPaperSizePS {
    public:
        /** ---------------------------------------------------
         * Constructor
         */
        nsPaperSizePS() { mCurrent = 0; }

        /** ---------------------------------------------------
         * @return true if the cursor points past the last item.
         */
        bool AtEnd() { return mCurrent >= mCount; }

        /** ---------------------------------------------------
         * Position the cursor at the beginning of the paper size list.
         * @return VOID
         */
        void First() { mCurrent = 0; }

        /** ---------------------------------------------------
         * Advance the cursor to the next item.
         * @return VOID
         */
        void Next() {
            NS_ASSERTION(!AtEnd(), "Invalid current item");
            mCurrent++;
        }

        /** ---------------------------------------------------
         * Point the cursor to the entry with the given paper name.
         * @return true if pointing to a valid entry.
         */
        bool Find(const char *aName);

        /** ---------------------------------------------------
         * @return a pointer to the name of the current paper size
         */
        const char *Name() {
            NS_PRECONDITION(!AtEnd(), "Invalid current item");
            return mList[mCurrent].name;
        }

        /** ---------------------------------------------------
         * @return the width of the page in millimeters
         */
        float Width_mm() {
            NS_PRECONDITION(!AtEnd(), "Invalid current item");
            return mList[mCurrent].width_mm;
        }

        /** ---------------------------------------------------
         * @return the height of the page in millimeters
         */
        float Height_mm() {
            NS_PRECONDITION(!AtEnd(), "Invalid current item");
            return mList[mCurrent].height_mm;
        }

        /** ---------------------------------------------------
         * @return true if the paper should be presented to
         *                 the user in metric units.
         */
        bool IsMetric() {
            NS_PRECONDITION(!AtEnd(), "Invalid current item");
            return mList[mCurrent].isMetric;
        }

    private:
        unsigned int mCurrent;
        static const nsPaperSizePS_ mList[];
        static const unsigned int mCount;
};

#endif

