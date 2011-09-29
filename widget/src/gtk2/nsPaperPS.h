/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
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
 * The Original Code is developed for mozilla.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron@newsguy.com>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
         * @return PR_TRUE if the cursor points past the last item.
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
         * @return PR_TRUE if pointing to a valid entry.
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
         * @return PR_TRUE if the paper should be presented to
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

