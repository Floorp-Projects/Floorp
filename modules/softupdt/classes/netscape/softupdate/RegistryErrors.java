/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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