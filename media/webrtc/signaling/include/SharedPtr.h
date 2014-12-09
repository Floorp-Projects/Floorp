/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <vector>
#include "base/linked_ptr.h"
#include "nsAutoPtr.h"

#ifndef DECLARE_PTR
#define DECLARE_PTR(className)\
    class className;\
    typedef linked_ptr<className> className##Ptr;
#endif


#ifndef DECLARE_PTR_VECTOR
#define DECLARE_PTR_VECTOR(className)\
    DECLARE_PTR(className)\
    typedef std::vector<className##Ptr> className##Vtr;\
    typedef linked_ptr<className##Vtr> className##Vtr##Ptr;
#endif


#ifndef NULL_PTR
#define NULL_PTR(className) linked_ptr<className>()
#endif

// NSPR Variations of the above, to help with migration
// from linked_ptr to nsRefPtr
#ifndef DECLARE_NS_PTR
#define DECLARE_NS_PTR(className)\
    class className;\
    typedef nsRefPtr<className> className##Ptr;
#endif


#ifndef DECLARE_NS_PTR_VECTOR
#define DECLARE_NS_PTR_VECTOR(className)\
    DECLARE_NS_PTR(className)\
    typedef std::vector<className##Ptr> className##Vtr;\
    typedef linked_ptr<className##Vtr> className##Vtr##Ptr;
#endif
