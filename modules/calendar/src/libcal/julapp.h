/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * julapp.h
 * John Sun
 * 4/28/98 9:47:49 AM
 */
#ifndef __JULIANAPPLICATION_H_
#define __JULIANAPPLICATION_H_

/**
 *  This class is created to appialize the NLS_DATA_DIRECTORY correctly
 *  without setting the NLS_DATA_DIRECTORY environment variable.
 *  To do this, the app function should locate somehow the location of
 *  the nls folder in the tree.  Currently, the location is in
 *  $(MOZ_SRC)/ns/dist/nls.  I don't know how to get the $(MOZ_SRC) though.
 */
class JulianApplication
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    static void init();
public:
    JulianApplication();
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    /*-----------------------------
    ** ACCESSORS (GET AND SET)
    **---------------------------*/
    /*-----------------------------
    ** UTILITIES
    **---------------------------*/
    /*-----------------------------
    ** STATIC METHODS
    **---------------------------*/
    /*-----------------------------
    ** OVERLOADED OPERATORS
    **---------------------------*/
};

#endif /* __JULIANAPPLICATION_H_ */

