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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef nsNetSegmentUtils_h__
#define nsNetSegmentUtils_h__

#include "necko-config.h"
#include "nsIOService.h"

#ifdef NECKO_SMALL_BUFFERS
#define NET_DEFAULT_SEGMENT_SIZE  2048
#define NET_DEFAULT_SEGMENT_COUNT 4
#else
#define NET_DEFAULT_SEGMENT_SIZE  4096
#define NET_DEFAULT_SEGMENT_COUNT 16
#endif

/**
 * returns preferred allocator for given segment size.  NULL implies
 * system allocator.  this result can be used when allocating a pipe.
 */
static inline nsIMemory *
net_GetSegmentAlloc(PRUint32 segsize)
{
    return (segsize == NET_DEFAULT_SEGMENT_SIZE)
                     ? nsIOService::gBufferCache
                     : nsnull;
}

/**
 * applies defaults to segment params in a consistent way.
 */
static inline void
net_ResolveSegmentParams(PRUint32 &segsize, PRUint32 &segcount)
{
    if (!segsize)
        segsize = NET_DEFAULT_SEGMENT_SIZE;
    if (!segcount)
        segcount = NET_DEFAULT_SEGMENT_COUNT;
}

#endif // !nsNetSegmentUtils_h__
