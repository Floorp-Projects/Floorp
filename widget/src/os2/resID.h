/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _resid_h
#define _resid_h

// Resource IDs for the widget DLL.

// Directory-picker dialog
#define DID_DIRPICKER  100

#define IDD_TREECNR    101
#define IDD_CBDRIVES   102
#define IDD_EFPATH     103
#define IDD_HELPBUTTON 104

// Icons
#define ID_ICO_FRAME         500
#define ID_ICO_FOLDER        501
#define ID_ICO_DRAGITEM      502

#define ID_PTR_SELECTURL    2000
#define ID_PTR_ARROWNORTH   2001
#define ID_PTR_ARROWNORTHP  2002
#define ID_PTR_ARROWSOUTH   2003
#define ID_PTR_ARROWSOUTHP  2004
#define ID_PTR_ARROWWEST    2005
#define ID_PTR_ARROWWESTP   2006
#define ID_PTR_ARROWEAST    2007
#define ID_PTR_ARROWEASTP   2008
#define ID_PTR_COPY         2009
#define ID_PTR_ALIAS        2010
#define ID_PTR_CELL         2011
#define ID_PTR_GRAB         2012
#define ID_PTR_GRABBING     2013
#define ID_PTR_ARROWWAIT    2014

#define ID_STR_FONT        10000
#define ID_STR_HMMDIR      10001
#define ID_STR_NOCDIR      10002

// OS2TODO HCT temporary bug fix
#ifndef FCF_CLOSEBUTTON // defined in the Merlin toolkit
#define FCF_CLOSEBUTTON 0x04000000L
#endif



#endif
