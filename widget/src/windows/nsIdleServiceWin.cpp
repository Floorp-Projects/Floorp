/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Gijs Kruitbosch <gijskruitbosch@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Gijs Kruitbosch <gijskruitbosch@gmail.com> 
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

#include "nsIdleServiceWin.h"
#include <windows.h>


#ifdef WINCE
// The last user input event time in microseconds. If there are any pending
// native toolkit input events it returns the current time. The value is
// compatible with PR_IntervalToMicroseconds(PR_IntervalNow()).
// DEFINED IN widget/src/windows/nsWindow.cpp
extern PRUint32 gLastInputEventTime;
#endif


NS_IMPL_ISUPPORTS1(nsIdleServiceWin, nsIIdleService)

NS_IMETHODIMP
nsIdleServiceWin::GetIdleTime(PRUint32 *aTimeDiff)
{
#ifndef WINCE
    LASTINPUTINFO inputInfo;
    inputInfo.cbSize = sizeof(inputInfo);
    if (!::GetLastInputInfo(&inputInfo))
        return NS_ERROR_FAILURE;

    *aTimeDiff = SAFE_COMPARE_EVEN_WITH_WRAPPING(GetTickCount(), inputInfo.dwTime);
#else
    // NOTE: nowTime is not necessarily equivalent to GetTickCount() return value
    //       we need to compare apples to apples - hence the nowTime variable
    PRUint32 nowTime = PR_IntervalToMicroseconds(PR_IntervalNow());

    *aTimeDiff = SAFE_COMPARE_EVEN_WITH_WRAPPING(nowTime, gLastInputEventTime) / 1000;
#endif

    return NS_OK;
}
