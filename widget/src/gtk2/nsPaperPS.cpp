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
 * The Original Code is developed for mozilla. The tables of paper sizes
 * and paper orientations are based on tables from nsPostScriptObj.h.
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

#include "mozilla/Util.h"
 
#include "nsPaperPS.h"
#include "plstr.h"
#include "nsCoord.h"
#include "nsMemory.h"

using namespace mozilla;

const nsPaperSizePS_ nsPaperSizePS::mList[] =
{
#define SIZE_MM(x)      (x)
#define SIZE_INCH(x)    ((x) * MM_PER_INCH_FLOAT)
    { "A5",             SIZE_MM(148),   SIZE_MM(210),   true },
    { "A4",             SIZE_MM(210),   SIZE_MM(297),   true },
    { "A3",             SIZE_MM(297),   SIZE_MM(420),   true },
    { "Letter",         SIZE_INCH(8.5), SIZE_INCH(11),  false },
    { "Legal",          SIZE_INCH(8.5), SIZE_INCH(14),  false },
    { "Tabloid",        SIZE_INCH(11),  SIZE_INCH(17),  false },
    { "Executive",      SIZE_INCH(7.5), SIZE_INCH(10),  false },
#undef SIZE_INCH
#undef SIZE_MM
};

const unsigned int nsPaperSizePS::mCount = ArrayLength(mList);

bool
nsPaperSizePS::Find(const char *aName)
{
    for (int i = mCount; i--; ) {
        if (!PL_strcasecmp(aName, mList[i].name)) {
            mCurrent = i;
            return true;
        }
    }
    return false;
}
