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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (Original Author)
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

#define NS_ALERTSERVICE_CONTRACTID \
  "@mozilla.org/alerts-service;1"

// This separate service uses the same nsIAlertsService interface,
// but instead sends a notification to a platform alerts API
// if available. Using a separate CID allows us to overwrite the XUL
// alerts service at runtime.
#define NS_SYSTEMALERTSERVICE_CONTRACTID \
  "@mozilla.org/system-alerts-service;1"

#define NS_AUTOCOMPLETECONTROLLER_CONTRACTID \
  "@mozilla.org/autocomplete/controller;1"

#define NS_AUTOCOMPLETECONTROLLER_CONTRACTID \
  "@mozilla.org/autocomplete/controller;1"

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

#define NS_AUTOCOMPLETEMDBRESULT_CONTRACTID \
  "@mozilla.org/autocomplete/mdb-result;1"

#define NS_DOWNLOADMANAGER_CONTRACTID \
  "@mozilla.org/download-manager;1"

#define NS_FORMHISTORY_CONTRACTID \
  "@mozilla.org/satchel/form-history;1"

#define NS_FORMFILLCONTROLLER_CONTRACTID \
  "@mozilla.org/satchel/form-fill-controller;1"

#define NS_FORMHISTORYAUTOCOMPLETE_CONTRACTID \
  "@mozilla.org/autocomplete/search;1?name=form-history"

#define NS_GLOBALHISTORY_DATASOURCE_CONTRACTID \
  "@mozilla.org/rdf/datasource;1?name=history"

#define NS_GLOBALHISTORY_AUTOCOMPLETE_CONTRACTID \
    "@mozilla.org/autocomplete/search;1?name=history"

#define NS_TYPEAHEADFIND_CONTRACTID \
    "@mozilla.org/typeaheadfind;1"

#define NS_PARENTALCONTROLSSERVICE_CONTRACTID \
    "@mozilla.org/parental-controls-service;1"

#define NS_URLCLASSIFIERDBSERVICE_CONTRACTID \
    "@mozilla.org/url-classifier/dbservice;1"

#define NS_URLCLASSIFIERSTREAMUPDATER_CONTRACTID \
    "@mozilla.org/url-classifier/streamupdater;1"

#define NS_URLCLASSIFIERUTILS_CONTRACTID \
    "@mozilla.org/url-classifier/utils;1"

#define NS_URLCLASSIFIERHASHCOMPLETER_CONTRACTID \
    "@mozilla.org/url-classifier/hashcompleter;1"

#define NS_SCRIPTABLEUNESCAPEHTML_CONTRACTID "@mozilla.org/feed-unescapehtml;1"

#define NS_NAVHISTORYSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-history-service;1"

#define NS_ANNOTATIONSERVICE_CONTRACTID \
  "@mozilla.org/browser/annotation-service;1"

#define NS_NAVBOOKMARKSSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-bookmarks-service;1"

#define NS_LIVEMARKSERVICE_CONTRACTID \
  "@mozilla.org/browser/livemark-service;2"

#define NS_FAVICONSERVICE_CONTRACTID \
  "@mozilla.org/browser/favicon-service;1"

#define NS_PLACESIMPORTEXPORTSERVICE_CONTRACTID \
  "@mozilla.org/browser/places/import-export-service;1"

#define NS_APPSTARTUP_CONTRACTID \
  "@mozilla.org/toolkit/app-startup;1"

/////////////////////////////////////////////////////////////////////////////

// {A0CCAAF8-09DA-44D8-B250-9AC3E93C8117}
#define NS_ALERTSSERVICE_CID \
{ 0xa0ccaaf8, 0x9da, 0x44d8, { 0xb2, 0x50, 0x9a, 0xc3, 0xe9, 0x3c, 0x81, 0x17 } }

// {84E11F80-CA55-11DD-AD8B-0800200C9A66}
#define NS_SYSTEMALERTSSERVICE_CID \
{ 0x84e11f80, 0xca55, 0x11dd, { 0xad, 0x8b, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 } }

// {F6D5EBBD-34F4-487d-9D10-3D34123E3EB9}
#define NS_AUTOCOMPLETECONTROLLER_CID \
{ 0xf6d5ebbd, 0x34f4, 0x487d, { 0x9d, 0x10, 0x3d, 0x34, 0x12, 0x3e, 0x3e, 0xb9 } }

// {2ee3039b-2de4-43d9-93b0-649beacff39a}
#define NS_AUTOCOMPLETESIMPLERESULT_CID \
{ 0x2ee3039b, 0x2de4, 0x43d9, { 0x93, 0xb0, 0x64, 0x9b, 0xea, 0xcf, 0xf3, 0x9a } }

// {7A6F70B6-2BBD-44b5-9304-501352D44AB5}
#define NS_AUTOCOMPLETEMDBRESULT_CID \
{ 0x7a6f70b6, 0x2bbd, 0x44b5, { 0x93, 0x4, 0x50, 0x13, 0x52, 0xd4, 0x4a, 0xb5 } }

#define NS_DOWNLOADMANAGER_CID \
    { 0xedb0490e, 0x1dd1, 0x11b2, { 0x83, 0xb8, 0xdb, 0xf8, 0xd8, 0x59, 0x06, 0xa6 } }

// {895DB6C7-DBDF-40ea-9F64-B175033243DC}
#define NS_FORMFILLCONTROLLER_CID \
{ 0x895db6c7, 0xdbdf, 0x40ea, { 0x9f, 0x64, 0xb1, 0x75, 0x3, 0x32, 0x43, 0xdc } }

// {59648a91-5a60-4122-8ff2-54b839c84aed}
#define NS_GLOBALHISTORY_CID \
{ 0x59648a91, 0x5a60, 0x4122, { 0x8f, 0xf2, 0x54, 0xb8, 0x39, 0xc8, 0x4a, 0xed} }

// {59648a91-5a60-4122-8ff2-54b839c84aed}
#define NS_PARENTALCONTROLSSERVICE_CID \
{ 0x580530e5, 0x118c, 0x4bc7, { 0xab, 0x88, 0xbc, 0x2c, 0xd2, 0xb9, 0x72, 0x23 } }

// {e7f70966-9a37-48d7-8aeb-35998f31090e}
#define NS_TYPEAHEADFIND_CID \
{ 0xe7f70966, 0x9a37, 0x48d7, { 0x8a, 0xeb, 0x35, 0x99, 0x8f, 0x31, 0x09, 0x0e} }
  
// {5eb7c3c1-ec1f-4007-87cc-eefb37d68ce6}
#define NS_URLCLASSIFIERDBSERVICE_CID \
{ 0x5eb7c3c1, 0xec1f, 0x4007, { 0x87, 0xcc, 0xee, 0xfb, 0x37, 0xd6, 0x8c, 0xe6} }

// {c2be6dc0-ef1e-4abd-86a2-4f864ddc57f6}
#define NS_URLCLASSIFIERSTREAMUPDATER_CID \
{ 0xc2be6dc0, 0xef1e, 0x4abd, { 0x86, 0xa2, 0x4f, 0x86, 0x4d, 0xdc, 0x57, 0xf6} }

// {b7b2ccec-7912-4ea6-a548-b038447004bd}
#define NS_URLCLASSIFIERUTILS_CID \
{ 0xb7b2ccec, 0x7912, 0x4ea6, { 0xa5, 0x48, 0xb0, 0x38, 0x44, 0x70, 0x04, 0xbd} }

// {10f2f5f0-f103-4901-980f-ba11bd70d60d}
#define NS_SCRIPTABLEUNESCAPEHTML_CID  \
{ 0x10f2f5f0, 0xf103, 0x4901, { 0x98, 0x0f, 0xba, 0x11, 0xbd, 0x70, 0xd6, 0x0d} }

#define NS_NAVHISTORYSERVICE_CID \
{ 0x88cecbb7, 0x6c63, 0x4b3b, { 0x8c, 0xd4, 0x84, 0xf3, 0xb8, 0x22, 0x8c, 0x69 } }

#define NS_NAVHISTORYRESULTTREEVIEWER_CID \
{ 0x2ea8966f, 0x0671, 0x4c02, { 0x9c, 0x70, 0x94, 0x59, 0x56, 0xd4, 0x54, 0x34 } }

#define NS_ANNOTATIONSERVICE_CID \
{ 0x5e8d4751, 0x1852, 0x434b, { 0xa9, 0x92, 0x2c, 0x6d, 0x2a, 0x25, 0xfa, 0x46 } }

#define NS_NAVBOOKMARKSSERVICE_CID \
{ 0x9de95a0c, 0x39a4, 0x4d64, {0x9a, 0x53, 0x17, 0x94, 0x0d, 0xd7, 0xca, 0xbb}}

#define NS_FAVICONSERVICE_CID \
{ 0x984e3259, 0x9266, 0x49cf, { 0xb6, 0x05, 0x60, 0xb0, 0x22, 0xa0, 0x07, 0x56 } }

// {6fb0c970-e1b1-11db-8314-0800200c9a66}
#define NS_PLACESIMPORTEXPORTSERVICE_CID \
{ 0x6fb0c970, 0xe1b1, 0x11db, { 0x83, 0x14, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 } }
