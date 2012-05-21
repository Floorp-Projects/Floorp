/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRegion.h"

void xxxGFXNeverCalled()
{
    nsRegion a;
    nsRect r;
    nsRegion b(r);
    nsRegion c(a);
    c.And(a,b);
    c.And(a,r);
    c.And(r,a);
    c.Or(a,b);
    c.Or(a,r);
    c.Sub(a,b);
    c.Sub(r,a);
    c.IsEmpty();
    c.GetBounds();
    c.GetNumRects();
    c.MoveBy(0,0);
    c.MoveBy(nsPoint());
    c.SetEmpty();
    c.SimplifyInward(0);
    c.SimplifyOutward(0);
    
    
    nsRegionRectIterator i(a);
    i.Next();
    i.Prev();
    i.Reset();
}
