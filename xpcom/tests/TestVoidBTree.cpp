/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Original Author:
 *   Chris Waterson <waterson@netscape.com>
 */

#include <stdlib.h>
#include <stdio.h>
#include "nsVoidBTree.h"
#include "nsVoidArray.h"

#define COUNT 1024
#define POINTER(i) NS_REINTERPRET_CAST(void*, 4 + 4 * (i))

static PRBool
Equal(const nsVoidArray& aControl, const nsVoidBTree& aTest)
{
    if (aControl.Count() != aTest.Count()) {
        printf("counts not equal; ");
        return PR_FALSE;
    }

    for (PRInt32 i = aControl.Count() - 1; i >= 0; --i) {
        if (aControl[i] != aTest[i]) {
            printf("element %d differs; ", i);
            return PR_FALSE;
        }
    }

    return PR_TRUE;
}


static void
CheckForwardEnumeration(const nsVoidArray& aControl, const nsVoidBTree& aTest)
{
    PRInt32 index = 0;

    nsVoidBTree::ConstIterator last = aTest.Last();
    for (nsVoidBTree::ConstIterator element = aTest.First(); element != last; ++element, ++index) {
        if (*element != aControl[index]) {
            printf("failed forward enumeration\n");
            exit(-1);
        }
    }

    if (index != aControl.Count()) {
        printf("erratic termination during forward enumeration\n");
        exit(-1);
    }
}

static void
CheckBackwardEnumeration(const nsVoidArray& aControl, const nsVoidBTree& aTest)
{
    PRInt32 index = aControl.Count();
    nsVoidBTree::ConstIterator first = aTest.First();
    nsVoidBTree::ConstIterator element = aTest.Last();

    if (first != element) {
        do {
            if (*--element != aControl[--index]) {
                printf("failed backward enumeration\n");
                exit(-1);
            }
        } while (element != first);
    }

    if (index != 0) {
        printf("erratic termination during backward enumeration\n");
        exit(-1);
    }
}

int main(int argc, char* argv[])
{
    nsVoidBTree btree;

    PRInt32 i;

    //----------------------------------------
    // Tail fill
    for (i = 0; i < COUNT; ++i)
        btree.InsertElementAt(NS_REINTERPRET_CAST(void*, POINTER(i)), i);

    for (i = 0; i < COUNT; ++i) {
        if (btree[i] != POINTER(i)) {
            printf("tail fill failed\n");
            return -1;
        }
    }

    printf("tail fill succeeded\n");

    //----------------------------------------
    // Tail empty
    for (i = COUNT - 1; i >= 0; --i)
        btree.RemoveElementAt(i);

    if (btree.Count() != 0) {
        printf("tail empty failed\n");
        return -1;
    }

    printf("tail empty succeeded\n");

    // N.B. no intervening Clear() to verify that we handle the re-use
    // case.

    //----------------------------------------
    // Front fill
    for (i = 0; i < COUNT; ++i)
        btree.InsertElementAt(POINTER(i), 0);

    for (i = 0; i < COUNT; ++i) {
        if (btree[COUNT - (i + 1)] != POINTER(i)) {
            printf("simple front fill failed\n");
            return -1;
        }
    }

    printf("front fill succeeded\n");

    //----------------------------------------
    // Front empty
    for (i = COUNT - 1; i >= 0; --i)
        btree.RemoveElementAt(0);

    if (btree.Count() != 0) {
        printf("front empty failed\n");
        return -1;
    }

    printf("front empty succeeded\n");
    fflush(stdout);

    //----------------------------------------
    // Test boundary conditions with small btree

    {
        printf("small btree boundary conditions ");
        fflush(stdout);

        nsVoidArray control;
        btree.Clear();

        CheckBackwardEnumeration(control, btree);
        CheckForwardEnumeration(control, btree);

        btree.AppendElement(POINTER(0));
        control.AppendElement(POINTER(0));

        CheckBackwardEnumeration(control, btree);
        CheckForwardEnumeration(control, btree);

        btree.AppendElement(POINTER(1));
        control.AppendElement(POINTER(1));

        CheckBackwardEnumeration(control, btree);
        CheckForwardEnumeration(control, btree);

        btree.RemoveElementAt(0);
        btree.RemoveElementAt(0);
        control.RemoveElementAt(0);
        control.RemoveElementAt(0);

        CheckBackwardEnumeration(control, btree);
        CheckForwardEnumeration(control, btree);

        printf("succeeded\n");
    }

    //----------------------------------------
    // Iterator
    {
        nsVoidArray control;
        btree.Clear();

        // Fill
        for (i = 0; i < COUNT; ++i) {
            PRInt32 slot = i ? rand() % i : 0;

            btree.InsertElementAt(POINTER(i), slot);
            control.InsertElementAt(POINTER(i), slot);

            if (! Equal(control, btree)) {
                printf("failed\n");
                return -1;
            }
        }

        for (nsVoidBTree::Iterator m = btree.First(); m != btree.Last(); ++m) {
            nsVoidBTree::Iterator n;
            for (n = m, ++n; n != btree.Last(); ++n) {
                if (*m > *n) {
                    void* tmp = *m;
                    *m = *n;
                    *n = tmp;
                }
            }
        }

        nsVoidBTree::Iterator el;
        for (el = btree.First(), i = 0; el != btree.Last(); ++el, ++i) {
            if (*el != POINTER(i)) {
                printf("bubble sort failed\n");
                return -1;
            }
        }

        printf("iteration succeeded\n");
    }

    //----------------------------------------
    // Random hammering

    printf("random fill/empty: ");

    for (PRInt32 iter = 10; iter >= 1; --iter) {
        printf("%d ", iter);
        fflush(stdout);

        nsVoidArray control;
        btree.Clear();

        // Fill
        for (i = 0; i < COUNT; ++i) {
            PRInt32 slot = i ? rand() % i : 0;

            btree.InsertElementAt(POINTER(i), slot);
            control.InsertElementAt(POINTER(i), slot);

            if (! Equal(control, btree)) {
                printf("failed\n");
                return -1;
            }
        }

        // IndexOf
        for (i = 0; i < COUNT; ++i) {
            void* element = control[i];
            if (btree.IndexOf(element) != i) {
                printf("failed IndexOf at %d\n", i);
                return -1;
            }
        }

        // Backward enumeration
        CheckBackwardEnumeration(control, btree);
               
        // Forward enumeration
        CheckForwardEnumeration(control, btree);

        // Empty
        for (i = COUNT - 1; i >= 0; --i) {
            PRInt32 slot = i ? rand() % i : 0;

            btree.RemoveElementAt(slot);
            control.RemoveElementAt(slot);

            if (! Equal(control, btree)) {
                printf("failed\n");
                return -1;
            }
        }
    }

    printf("succeeded\n");

    return 0;
}
