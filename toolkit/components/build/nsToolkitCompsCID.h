/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This separate service uses the same nsIAlertsService interface,
// but instead sends a notification to a platform alerts API
// if available. Using a separate CID allows us to overwrite the XUL
// alerts service at runtime.
#define NS_SYSTEMALERTSERVICE_CONTRACTID "@mozilla.org/system-alerts-service;1"

#define NS_AUTOCOMPLETECONTROLLER_CONTRACTID \
  "@mozilla.org/autocomplete/controller;1"

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

#define NS_AUTOCOMPLETEMDBRESULT_CONTRACTID \
  "@mozilla.org/autocomplete/mdb-result;1"

#define NS_FORMHISTORY_CONTRACTID "@mozilla.org/satchel/form-history;1"

#define NS_FORMFILLCONTROLLER_CONTRACTID \
  "@mozilla.org/satchel/form-fill-controller;1"

#define NS_FORMHISTORYAUTOCOMPLETE_CONTRACTID \
  "@mozilla.org/autocomplete/search;1?name=form-history"

#define NS_URLCLASSIFIERHASHCOMPLETER_CONTRACTID \
  "@mozilla.org/url-classifier/hashcompleter;1"

#define NS_NAVHISTORYSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-history-service;1"

#define NS_ANNOTATIONSERVICE_CONTRACTID \
  "@mozilla.org/browser/annotation-service;1"

#define NS_NAVBOOKMARKSSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-bookmarks-service;1"

#define NS_TAGGINGSERVICE_CONTRACTID "@mozilla.org/browser/tagging-service;1"

#define NS_FAVICONSERVICE_CONTRACTID "@mozilla.org/browser/favicon-service;1"

#define NS_KEY_VALUE_SERVICE_CONTRACTID "@mozilla.org/key-value-service;1"

/////////////////////////////////////////////////////////////////////////////

// {84E11F80-CA55-11DD-AD8B-0800200C9A66}
#define NS_SYSTEMALERTSSERVICE_CID                   \
  {                                                  \
    0x84e11f80, 0xca55, 0x11dd, {                    \
      0xad, 0x8b, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 \
    }                                                \
  }

// {7A6F70B6-2BBD-44b5-9304-501352D44AB5}
#define NS_AUTOCOMPLETEMDBRESULT_CID                \
  {                                                 \
    0x7a6f70b6, 0x2bbd, 0x44b5, {                   \
      0x93, 0x4, 0x50, 0x13, 0x52, 0xd4, 0x4a, 0xb5 \
    }                                               \
  }

// {59648a91-5a60-4122-8ff2-54b839c84aed}
#define NS_GLOBALHISTORY_CID                         \
  {                                                  \
    0x59648a91, 0x5a60, 0x4122, {                    \
      0x8f, 0xf2, 0x54, 0xb8, 0x39, 0xc8, 0x4a, 0xed \
    }                                                \
  }

// {7a258022-6765-11e5-b379-b37b1f2354be}
#define NS_URLCLASSIFIERDBSERVICE_CID                \
  {                                                  \
    0x7a258022, 0x6765, 0x11e5, {                    \
      0xb3, 0x79, 0xb3, 0x7b, 0x1f, 0x23, 0x54, 0xbe \
    }                                                \
  }

#define NS_NAVHISTORYSERVICE_CID                     \
  {                                                  \
    0x88cecbb7, 0x6c63, 0x4b3b, {                    \
      0x8c, 0xd4, 0x84, 0xf3, 0xb8, 0x22, 0x8c, 0x69 \
    }                                                \
  }

#define NS_NAVHISTORYRESULTTREEVIEWER_CID            \
  {                                                  \
    0x2ea8966f, 0x0671, 0x4c02, {                    \
      0x9c, 0x70, 0x94, 0x59, 0x56, 0xd4, 0x54, 0x34 \
    }                                                \
  }

#define NS_ANNOTATIONSERVICE_CID                     \
  {                                                  \
    0x5e8d4751, 0x1852, 0x434b, {                    \
      0xa9, 0x92, 0x2c, 0x6d, 0x2a, 0x25, 0xfa, 0x46 \
    }                                                \
  }

#define NS_NAVBOOKMARKSSERVICE_CID                   \
  {                                                  \
    0x9de95a0c, 0x39a4, 0x4d64, {                    \
      0x9a, 0x53, 0x17, 0x94, 0x0d, 0xd7, 0xca, 0xbb \
    }                                                \
  }

#define NS_FAVICONSERVICE_CID                        \
  {                                                  \
    0x984e3259, 0x9266, 0x49cf, {                    \
      0xb6, 0x05, 0x60, 0xb0, 0x22, 0xa0, 0x07, 0x56 \
    }                                                \
  }

// 6cc1a0a8-af97-4d41-9b4a-58dcec46ebce
#define NS_KEY_VALUE_SERVICE_CID                     \
  {                                                  \
    0x6cc1a0a8, 0xaf97, 0x4d41, {                    \
      0x9b, 0x4a, 0x58, 0xdc, 0xec, 0x46, 0xeb, 0xce \
    }                                                \
  }
