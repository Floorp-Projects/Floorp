/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_UPDATER)
# if !defined(MOZ_WIDGET_ANDROID)
#  define USE_MOZ_UPDATER
# endif
#endif

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

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

#define NS_AUTOCOMPLETEMDBRESULT_CONTRACTID \
  "@mozilla.org/autocomplete/mdb-result;1"

#define NS_DOWNLOADMANAGER_CONTRACTID \
  "@mozilla.org/download-manager;1"

#define NS_DOWNLOADPLATFORM_CONTRACTID \
  "@mozilla.org/toolkit/download-platform;1"

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

#define NS_URLCLASSIFIERPREFIXSET_CONTRACTID \
  "@mozilla.org/url-classifier/prefixset;1"

#define NS_URLCLASSIFIERDBSERVICE_CONTRACTID \
    "@mozilla.org/url-classifier/dbservice;1"

#define NS_URLCLASSIFIERSTREAMUPDATER_CONTRACTID \
    "@mozilla.org/url-classifier/streamupdater;1"

#define NS_URLCLASSIFIERUTILS_CONTRACTID \
    "@mozilla.org/url-classifier/utils;1"

#define NS_URLCLASSIFIERHASHCOMPLETER_CONTRACTID \
    "@mozilla.org/url-classifier/hashcompleter;1"

#define NS_NAVHISTORYSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-history-service;1"

#define NS_ANNOTATIONSERVICE_CONTRACTID \
  "@mozilla.org/browser/annotation-service;1"

#define NS_NAVBOOKMARKSSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-bookmarks-service;1"

#define NS_LIVEMARKSERVICE_CONTRACTID \
  "@mozilla.org/browser/livemark-service;2"

#define NS_TAGGINGSERVICE_CONTRACTID \
"@mozilla.org/browser/tagging-service;1"

#define NS_FAVICONSERVICE_CONTRACTID \
  "@mozilla.org/browser/favicon-service;1"

#define NS_APPSTARTUP_CONTRACTID \
  "@mozilla.org/toolkit/app-startup;1"

#if defined(USE_MOZ_UPDATER)
#define NS_UPDATEPROCESSOR_CONTRACTID \
  "@mozilla.org/updates/update-processor;1"
#endif

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

#define NS_DOWNLOADPLATFORM_CID \
    { 0x649a14c9, 0xfe5c, 0x48ec, { 0x9c, 0x85, 0x00, 0xca, 0xd9, 0xcc, 0xf3, 0x2e } }

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

// {3d8579f0-75fa-4e00-ba41-38661d5b5d17}
 #define NS_URLCLASSIFIERPREFIXSET_CID \
{ 0x3d8579f0, 0x75fa, 0x4e00, { 0xba, 0x41, 0x38, 0x66, 0x1d, 0x5b, 0x5d, 0x17} }

// {8a389f21-f821-4e29-9c6b-3de6f33cd7cf}
#define NS_URLCLASSIFIERDBSERVICE_CID \
{ 0x8a389f21, 0xf821, 0x4e29, { 0x9c, 0x6b, 0x3d, 0xe6, 0xf3, 0x3c, 0xd7, 0xcf} }

// {79e6b710-ce68-4639-ac6b-7d293af424a1}
#define NS_URLCLASSIFIERSTREAMUPDATER_CID \
{ 0x79e6b710, 0xce68, 0x4639, { 0xac, 0x6b, 0x7d, 0x29, 0x3a, 0xf4, 0x24, 0xa1} }

// {b7b2ccec-7912-4ea6-a548-b038447004bd}
#define NS_URLCLASSIFIERUTILS_CID \
{ 0xb7b2ccec, 0x7912, 0x4ea6, { 0xa5, 0x48, 0xb0, 0x38, 0x44, 0x70, 0x04, 0xbd} }

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

#if defined(USE_MOZ_UPDATER)
#define NS_UPDATEPROCESSOR_CID \
{ 0xf3dcf644, 0x79e8, 0x4f59, { 0xa1, 0xbb, 0x87, 0x84, 0x54, 0x48, 0x8e, 0xf9 } }
#endif

#define NS_APPLICATION_REPUTATION_SERVICE_CONTRACTID \
  "@mozilla.org/downloads/application-reputation-service;1"

#define NS_APPLICATION_REPUTATION_SERVICE_CID \
{ 0x8576c950, 0xf4a2, 0x11e2, { 0xb7, 0x78, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 } }
