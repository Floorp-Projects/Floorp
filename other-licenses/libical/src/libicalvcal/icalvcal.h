/* -*- Mode: C -*-*/
/*======================================================================
 FILE: icalvcal.h
 CREATOR: eric 25 May 00

 

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvcal.h


======================================================================*/

#ifndef ICALVCAL_H
#define ICALVCAL_H

#include "ical.h"
#include "vcc.h"

/* Convert a vObject into an icalcomponent */

icalcomponent* icalvcal_convert(VObject *object);

#endif /* !ICALVCAL_H */



