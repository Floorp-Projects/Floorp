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
 * RegistryErrors
 *
 * This interface defines the error return codes used by the Registry
 * and VersionRegistry classes and related helper classes.  Many of these
 * errors should never be returned by the Java interface to the registry,
 * but the complete list is included just in case...
 */
package netscape.softupdate;

public interface RegistryErrors {

    public static final int REGERR_OK            = 0;
    public static final int REGERR_FAIL          = 1;
    public static final int REGERR_NOMORE        = 2;
    public static final int REGERR_NOFIND        = 3;
    public static final int REGERR_BADREAD       = 4;
    public static final int REGERR_BADLOCN       = 5;
    public static final int REGERR_PARAM         = 6;
    public static final int REGERR_BADMAGIC      = 7;
    public static final int REGERR_BADCHECK      = 8;
    public static final int REGERR_NOFILE        = 9;
    public static final int REGERR_MEMORY        = 10;
    public static final int REGERR_BUFTOOSMALL   = 11;
    public static final int REGERR_NAMETOOLONG   = 12;
    public static final int REGERR_REGVERSION    = 13;
    public static final int REGERR_DELETED       = 14;
    public static final int REGERR_BADTYPE       = 15;
    public static final int REGERR_NOPATH        = 16;
    public static final int REGERR_BADNAME       = 17;
    public static final int REGERR_READONLY      = 18;

    public static final int REGERR_SECURITY      = 99;
}