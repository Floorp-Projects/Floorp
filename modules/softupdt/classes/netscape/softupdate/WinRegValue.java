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

package netscape.softupdate;


public final class WinRegValue {
    public WinRegValue(int datatype, byte[] regdata) {
        type = datatype;
        data = regdata;
    }

    public int type;
    public byte[] data;

    public static final int REG_SZ                          = 1;
    public static final int REG_EXPAND_SZ                   = 2;
    public static final int REG_BINARY                      = 3;
    public static final int REG_DWORD                       = 4;
    public static final int REG_DWORD_LITTLE_ENDIAN         = 4;
    public static final int REG_DWORD_BIG_ENDIAN            = 5;
    public static final int REG_LINK                        = 6;
    public static final int REG_MULTI_SZ                    = 7;
    public static final int REG_RESOURCE_LIST               = 8;
    public static final int REG_FULL_RESOURCE_DESCRIPTOR    = 9;
    public static final int REG_RESOURCE_REQUIREMENTS_LIST  = 10;
}
