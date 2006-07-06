/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 *   Dan Rosen <dr@netscape.com>
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

/* 2d96b3d0-c051-11d1-a827-0040959a28c9 */
#define NS_WINDOW_CID \
{ 0x2d96b3d0, 0xc051, 0x11d1, \
    {0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9}}

/* 2d96b3d1-c051-11d1-a827-0040959a28c9 */
#define NS_CHILD_CID \
{ 0x2d96b3d1, 0xc051, 0x11d1, \
    {0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9} }


/* BA7DE611-6088-11d3-A83E-00105A183419 */
#define NS_POPUP_CID \
{ 0xba7de611, 0x6088, 0x11d3,  \
    { 0xa8, 0x3e, 0x0, 0x10, 0x5a, 0x18, 0x34, 0x19 } }

/* bd57cee8-1dd1-11b2-9fe7-95cf4709aea3 */
#define NS_FILEPICKER_CID \
{ 0xbd57cee8, 0x1dd1, 0x11b2, \
    {0x9f, 0xe7, 0x95, 0xcf, 0x47, 0x09, 0xae, 0xa3} }

// {c2281100-3b4b-11d6-a384-f705fd0766fc}
#define NS_NATIVESCROLLBAR_CID \
{ 0xc2281100, 0x3b4b, 0x11d6, { 0xa3, 0x84, 0xf7, 0x05, 0xfd, 0x07, 0x66, 0xfc } }

/* 2d96b3df-c051-11d1-a827-0040959a28c9 */
#define NS_APPSHELL_CID \
{ 0x2d96b3df, 0xc051, 0x11d1, \
    {0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9} }

/* 2d96b3e0-c051-11d1-a827-0040959a28c9 */
#define NS_TOOLKIT_CID \
 { 0x2d96b3e0, 0xc051, 0x11d1, \
    {0xa8, 0x27, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9} }

/* XXX the following CID's are not in order. This needs
   to be fixed. */

/* A61E6398-2057-40fd-9C81-873B908D24E7 */
#define NS_LOOKANDFEEL_CID \
{ 0xa61e6398, 0x2057, 0x40fd,  \
    { 0x9c, 0x81, 0x87, 0x3b, 0x90, 0x8d, 0x24, 0xe7 } }

//-----------------------------------------------------------
// Menus 
//-----------------------------------------------------------

// {BC658C81-4BEB-11d2-8DBB-00609703C14E}
#define NS_MENUBAR_CID      \
{ 0xbc658c81, 0x4beb, 0x11d2, \
  { 0x8d, 0xbb, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

// {35A3DEC1-4992-11d2-8DBA-00609703C14E}
#define NS_MENU_CID      \
{ 0x35a3dec1, 0x4992, 0x11d2, \
  { 0x8d, 0xba, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

// {7F045771-4BEB-11d2-8DBB-00609703C14E}
#define NS_MENUITEM_CID      \
{ 0x7f045771, 0x4beb, 0x11d2, \
  { 0x8d, 0xbb, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

// {F6CD4F21-53AF-11d2-8DC4-00609703C14E}
#define NS_POPUPMENU_CID      \
{ 0xf6cd4f21, 0x53af, 0x11d2, \
  { 0x8d, 0xc4, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

//-----------------------------------------------------------
//Drag & Drop & Clipboard
//-----------------------------------------------------------
// {8B5314BB-DB01-11d2-96CE-0060B0FB9956}
#define NS_DRAGSERVICE_CID      \
{ 0x8b5314bb, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

// {8B5314BC-DB01-11d2-96CE-0060B0FB9956}
#define NS_TRANSFERABLE_CID      \
{ 0x8b5314bc, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

// {8B5314BA-DB01-11d2-96CE-0060B0FB9956}
#define NS_CLIPBOARD_CID      \
{ 0x8b5314ba, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

// {77221D5A-1DD2-11B2-8C69-C710F15D2ED5}
#define NS_CLIPBOARDHELPER_CID      \
{ 0x77221d5a, 0x1dd2, 0x11b2, { 0x8c, 0x69, 0xc7, 0x10, 0xf1, 0x5d, 0x2e, 0xd5 } }

// {8B5314BD-DB01-11d2-96CE-0060B0FB9956}
#define NS_DATAFLAVOR_CID      \
{ 0x8b5314bd, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

// {948A0023-E3A7-11d2-96CF-0060B0FB9956}
#define NS_HTMLFORMATCONVERTER_CID      \
{ 0x948a0023, 0xe3a7, 0x11d2, { 0x96, 0xcf, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

//-----------------------------------------------------------
//Other
//-----------------------------------------------------------
// {B148EED2-236D-11d3-B35C-00A0CC3C1CDE}
#define NS_SOUND_CID \
{ 0xb148eed2, 0x236d, 0x11d3, { 0xb3, 0x5c, 0x0, 0xa0, 0xcc, 0x3c, 0x1c, 0xde } }

// {9f1800ab-f428-4207-b40c-e832e77b01fc}
#define NS_BIDIKEYBOARD_CID \
{ 0x9f1800ab, 0xf428, 0x4207, { 0xb4, 0x0c, 0xe8, 0x32, 0xe7, 0x7b, 0x01, 0xfc } }

#define NS_SCREENMANAGER_CID \
{ 0xc401eb80, 0xf9ea, 0x11d3, { 0xbb, 0x6f, 0xe7, 0x32, 0xb7, 0x3e, 0xbe, 0x7c } }


//-----------------------------------------------------------
//Printing
//-----------------------------------------------------------
#define NS_DEVICE_CONTEXT_SPEC_CID \
{ 0xd7193600, 0x78e0, 0x11d2, \
{ 0xa8, 0x46, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_DEVICE_CONTEXT_SPEC_FACTORY_CID \
{ 0xec5bebb0, 0x7b51, 0x11d2, \
{ 0xa8, 0x48, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

#define NS_PRINTSETTINGSSERVICE_CID \
{ 0x841387c8, 0x72e6, 0x484b, \
{ 0x92, 0x96, 0xbf, 0x6e, 0xea, 0x80, 0xd5, 0x8a } }

// NOTE: This now has the same CID as NS_PRINTSETTINGSSERVICE_CID
// will go away when Bug 144114 is fixed
#define NS_PRINTOPTIONS_CID NS_PRINTSETTINGSSERVICE_CID

#define NS_PRINTER_ENUMERATOR_CID \
{ 0xa6cf9129, 0x15b3, 0x11d2, \
{ 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

#define NS_PRINTSESSION_CID \
{ 0x2f977d53, 0x5485, 0x11d4, \
{ 0x87, 0xe2, 0x00, 0x10, 0xa4, 0xe7, 0x5e, 0xf2 } }

