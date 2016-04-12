/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jar.h"

/* These are old DS_* routines renamed to ZZ_* */
ZZList *
ZZ_NewList(void)
{
    ZZList *list = (ZZList *)PORT_ZAlloc(sizeof(ZZList));
    if (list)
        ZZ_InitList(list);
    return list;
}

ZZLink *
ZZ_NewLink(JAR_Item *thing)
{
    ZZLink *link = (ZZLink *)PORT_ZAlloc(sizeof(ZZLink));
    if (link)
        link->thing = thing;
    return link;
}

void
ZZ_DestroyLink(ZZLink *link)
{
    PORT_Free(link);
}

void
ZZ_DestroyList(ZZList *list)
{
    PORT_Free(list);
}
