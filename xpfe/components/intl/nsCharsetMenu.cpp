/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIServiceManager.h"
#include "nsIComponentManager.h" 
#include "rdf.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFContainerUtils.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "nsCharsetMenu.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsICollation.h"
#include "nsCollationCID.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsICurrentCharsetListener.h"
#include "nsQuickSort.h"
#include "nsIObserver.h"
#include "nsVoidArray.h"
#include "nsIObserverService.h"
#include "nsIRequestObserver.h"
#include "nsITimelineService.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID, NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

static const char * kURINC_BrowserAutodetMenuRoot = "NC:BrowserAutodetMenuRoot";
static const char * kURINC_BrowserCharsetMenuRoot = "NC:BrowserCharsetMenuRoot";
static const char * kURINC_BrowserMoreCharsetMenuRoot = "NC:BrowserMoreCharsetMenuRoot";
static const char * kURINC_BrowserMore1CharsetMenuRoot = "NC:BrowserMore1CharsetMenuRoot";
static const char * kURINC_BrowserMore2CharsetMenuRoot = "NC:BrowserMore2CharsetMenuRoot";
static const char * kURINC_BrowserMore3CharsetMenuRoot = "NC:BrowserMore3CharsetMenuRoot";
static const char * kURINC_BrowserMore4CharsetMenuRoot = "NC:BrowserMore4CharsetMenuRoot";
static const char * kURINC_BrowserMore5CharsetMenuRoot = "NC:BrowserMore5CharsetMenuRoot";
static const char * kURINC_MaileditCharsetMenuRoot = "NC:MaileditCharsetMenuRoot";
static const char * kURINC_MailviewCharsetMenuRoot = "NC:MailviewCharsetMenuRoot";
static const char * kURINC_ComposerCharsetMenuRoot = "NC:ComposerCharsetMenuRoot";
static const char * kURINC_DecodersRoot = "NC:DecodersRoot";
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Name);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Checked);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, BookmarkSeparator);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, CharsetDetector);
DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, NC, type);

// Note here that browser and mailview have the same static area and cache 
// size but the cache itself is separate.

#define kBrowserStaticPrefKey       "intl.charsetmenu.browser.static"
#define kBrowserCachePrefKey        "intl.charsetmenu.browser.cache"
#define kBrowserCacheSizePrefKey    "intl.charsetmenu.browser.cache.size"

#define kMailviewStaticPrefKey      "intl.charsetmenu.browser.static"
#define kMailviewCachePrefKey       "intl.charsetmenu.mailview.cache"
#define kMailviewCacheSizePrefKey   "intl.charsetmenu.browser.cache.size"

#define kComposerStaticPrefKey      "intl.charsetmenu.browser.static"
#define kComposerCachePrefKey       "intl.charsetmenu.composer.cache"
#define kComposerCacheSizePrefKey   "intl.charsetmenu.browser.cache.size"

#define kMaileditPrefKey            "intl.charsetmenu.mailedit"

NS_NAMED_LITERAL_STRING(gCharsetMenuSelectedTopic, "charsetmenu-selected");
//----------------------------------------------------------------------------
// Class nsMenuEntry [declaration]

/**
 * A little class holding all data needed for a menu item.
 *
 * @created         18/Apr/2000
 * @author  Catalin Rotaru [CATA]
 */
class nsMenuEntry
{
public: 
  // memory & ref counting & leak prevention stuff
  nsMenuEntry() { MOZ_COUNT_CTOR(nsMenuEntry); }
  ~nsMenuEntry() { MOZ_COUNT_DTOR(nsMenuEntry); }

  nsCOMPtr<nsIAtom> mCharset;
  nsAutoString      mTitle;
};

MOZ_DECL_CTOR_COUNTER(nsMenuEntry)

//----------------------------------------------------------------------------
// Class nsCharsetMenu [declaration]

/**
 * The Charset Converter menu.
 *
 * God, our GUI programming disgusts me.
 *
 * @created         23/Nov/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsCharsetMenu : public nsIRDFDataSource, public nsICurrentCharsetListener
{
  NS_DECL_ISUPPORTS

private:
  static nsIRDFResource * kNC_BrowserAutodetMenuRoot;
  static nsIRDFResource * kNC_BrowserCharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMoreCharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore1CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore2CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore3CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore4CharsetMenuRoot;
  static nsIRDFResource * kNC_BrowserMore5CharsetMenuRoot;
  static nsIRDFResource * kNC_MaileditCharsetMenuRoot;
  static nsIRDFResource * kNC_MailviewCharsetMenuRoot;
  static nsIRDFResource * kNC_ComposerCharsetMenuRoot;
  static nsIRDFResource * kNC_DecodersRoot;
  static nsIRDFResource * kNC_Name;
  static nsIRDFResource * kNC_Checked;
  static nsIRDFResource * kNC_CharsetDetector;
  static nsIRDFResource * kNC_BookmarkSeparator;
  static nsIRDFResource * kRDF_type;

  static nsIRDFDataSource * mInner;

  PRPackedBool mInitialized;
  PRPackedBool mBrowserMenuInitialized;
  PRPackedBool mMailviewMenuInitialized;
  PRPackedBool mComposerMenuInitialized;
  PRPackedBool mMaileditMenuInitialized;
  PRPackedBool mSecondaryTiersInitialized;
  PRPackedBool mAutoDetectInitialized;
  PRPackedBool mOthersInitialized;

  nsVoidArray   mBrowserMenu;
  PRInt32       mBrowserCacheStart;
  PRInt32       mBrowserCacheSize;
  PRInt32       mBrowserMenuRDFPosition;

  nsVoidArray   mMailviewMenu;
  PRInt32       mMailviewCacheStart;
  PRInt32       mMailviewCacheSize;
  PRInt32       mMailviewMenuRDFPosition;

  nsVoidArray   mComposerMenu;
  PRInt32       mComposerCacheStart;
  PRInt32       mComposerCacheSize;
  PRInt32       mComposerMenuRDFPosition;

  nsCOMPtr<nsIRDFService>               mRDFService;
  nsCOMPtr<nsICharsetConverterManager2> mCCManager;
  nsCOMPtr<nsIPrefBranch>               mPrefs;
  nsCOMPtr<nsIObserver>                 mCharsetMenuObserver;
  nsCOMPtr<nsISupportsArray>            mDecoderList;

  nsresult Done();
  nsresult SetCharsetCheckmark(nsString * aCharset, PRBool aValue);

  nsresult FreeResources();

  nsresult InitStaticMenu(nsISupportsArray * aDecs, 
    nsIRDFResource * aResource, const char * aKey, nsVoidArray * aArray);
  nsresult InitCacheMenu(nsISupportsArray * aDecs, nsIRDFResource * aResource,
    const char * aKey, nsVoidArray * aArray);
  nsresult InitMoreMenu(nsISupportsArray * aDecs, nsIRDFResource * aResource, 
    char * aFlag);
  nsresult InitMoreSubmenus(nsISupportsArray * aDecs);

  nsresult AddCharsetToItemArray(nsVoidArray * aArray, nsIAtom * aCharset, 
    nsMenuEntry ** aResult, PRInt32 aPlace);
  nsresult AddCharsetArrayToItemArray(nsVoidArray * aArray, 
    nsISupportsArray * aCharsets);
  nsresult AddMenuItemToContainer(nsIRDFContainer * aContainer, 
    nsMenuEntry * aItem, nsIRDFResource * aType, char * aIDPrefix, 
    PRInt32 aPlace);
  nsresult AddMenuItemArrayToContainer(nsIRDFContainer * aContainer, 
    nsVoidArray * aArray, nsIRDFResource * aType);
  nsresult AddCharsetToContainer(nsVoidArray * aArray, 
    nsIRDFContainer * aContainer, nsIAtom * aCharset, char * aIDPrefix, 
    PRInt32 aPlace, PRInt32 aRDFPlace);

  nsresult AddFromPrefsToMenu(nsVoidArray * aArray, 
    nsIRDFContainer * aContainer, const char * aKey, nsISupportsArray * aDecs,
    char * aIDPrefix);
  nsresult AddFromNolocPrefsToMenu(nsVoidArray * aArray, 
    nsIRDFContainer * aContainer, const char * aKey, nsISupportsArray * aDecs,
    char * aIDPrefix);
  nsresult AddFromStringToMenu(char * aCharsetList, nsVoidArray * aArray, 
    nsIRDFContainer * aContainer, nsISupportsArray * aDecs, char * aIDPrefix);

  nsresult AddSeparatorToContainer(nsIRDFContainer * aContainer);
  nsresult AddCharsetToCache(nsIAtom * aCharset, nsVoidArray * aArray, 
    nsIRDFResource * aRDFResource, PRInt32 aCacheStart, PRInt32 aCacheSize, 
    PRInt32 aRDFPlace);

  nsresult WriteCacheToPrefs(nsVoidArray * aArray, PRInt32 aCacheStart, 
    const char * aKey);
  nsresult UpdateCachePrefs(const char * aCacheKey, const char * aCacheSizeKey, 
    const char * aStaticKey, const PRUnichar * aCharset);

  nsresult ClearMenu(nsIRDFContainer * aContainer, nsVoidArray * aArray);
  nsresult RemoveLastMenuItem(nsIRDFContainer * aContainer, 
    nsVoidArray * aArray);

  nsresult RemoveFlaggedCharsets(nsISupportsArray * aList, nsString * aProp);
  nsresult NewRDFContainer(nsIRDFDataSource * aDataSource, 
    nsIRDFResource * aResource, nsIRDFContainer ** aResult);
  void FreeMenuItemArray(nsVoidArray * aArray);
  PRInt32 FindMenuItemInArray(nsVoidArray * aArray, nsIAtom * aCharset, 
      nsMenuEntry ** aResult);
  nsresult ReorderMenuItemArray(nsVoidArray * aArray);
  nsresult GetCollation(nsICollation ** aCollation);

public:
  nsCharsetMenu();
  virtual ~nsCharsetMenu();

  nsresult Init();
  nsresult InitBrowserMenu();
  nsresult InitMaileditMenu();
  nsresult InitMailviewMenu();
  nsresult InitComposerMenu();
  nsresult InitOthers();
  nsresult InitSecondaryTiers();
  nsresult InitAutodetMenu();
  nsresult RefreshBroserMenu();
  nsresult RefreshMailviewMenu();
  nsresult RefreshMaileditMenu();
  nsresult RefreshComposerMenu();

  //--------------------------------------------------------------------------
  // Interface nsICurrentCharsetListener [declaration]

  NS_IMETHOD SetCurrentCharset(const PRUnichar * aCharset);
  NS_IMETHOD SetCurrentMailCharset(const PRUnichar * aCharset);
  NS_IMETHOD SetCurrentComposerCharset(const PRUnichar * aCharset);

  //--------------------------------------------------------------------------
  // Interface nsIRDFDataSource [declaration]

  NS_DECL_NSIRDFDATASOURCE
};

//----------------------------------------------------------------------------
// Global functions and data [implementation]

NS_IMETHODIMP NS_NewCharsetMenu(nsISupports * aOuter, const nsIID & aIID, 
                                void ** aResult)
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    *aResult = nsnull;
    return NS_ERROR_NO_AGGREGATION;
  }
  nsCharsetMenu* inst = new nsCharsetMenu();
  if (!inst) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) {
    *aResult = nsnull;
    delete inst;
  }
  return res;
}

struct charsetMenuSortRecord {
  nsMenuEntry* item;
  PRUint8*     key;
  PRUint32     len;

};

static int PR_CALLBACK CompareMenuItems(const void* aArg1, const void* aArg2, void *data)
{
  PRInt32 res; 
  nsICollation * collation = (nsICollation *) data;
  charsetMenuSortRecord *rec1 = (charsetMenuSortRecord *) aArg1;
  charsetMenuSortRecord *rec2 = (charsetMenuSortRecord *) aArg2;

  collation->CompareRawSortKey(rec1->key, rec1->len, rec2->key, rec2->len, &res);

  return res;
}

//----------------------------------------------------------------------------
// Class nsCharsetMenuObserver

class nsCharsetMenuObserver : public nsIObserver {

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsCharsetMenuObserver(nsCharsetMenu * menu)
    : mCharsetMenu(menu)
  {
    NS_INIT_ISUPPORTS();
  }

  virtual ~nsCharsetMenuObserver() {}

private:
  nsCharsetMenu* mCharsetMenu;
};

NS_IMPL_ISUPPORTS1(nsCharsetMenuObserver, nsIObserver);

NS_IMETHODIMP nsCharsetMenuObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu:Observe");
  nsresult rv;
 
  //XUL event handler
  if (!nsCRT::strcmp(aTopic, "charsetmenu-selected")) {
    nsDependentString nodeName(someData);
    rv = mCharsetMenu->Init();
    if (nodeName.Equals(NS_LITERAL_STRING("browser"))) {
      rv = mCharsetMenu->InitBrowserMenu();
    }
    if (nodeName.Equals(NS_LITERAL_STRING("composer"))) {
      rv = mCharsetMenu->InitComposerMenu();
    }
    if (nodeName.Equals(NS_LITERAL_STRING("mailview"))) {
      rv = mCharsetMenu->InitMailviewMenu();
    }
    if (nodeName.Equals(NS_LITERAL_STRING("mailedit"))) {
      rv = mCharsetMenu->InitMaileditMenu();
      rv = mCharsetMenu->InitOthers();
    }
    if (nodeName.Equals(NS_LITERAL_STRING("more-menu"))) {
      rv = mCharsetMenu->InitSecondaryTiers();
      rv = mCharsetMenu->InitAutodetMenu();
    }
    if (nodeName.Equals(NS_LITERAL_STRING("other"))) {
      rv = mCharsetMenu->InitOthers();
      rv = mCharsetMenu->InitMaileditMenu();
    }
  }
   
   //pref event handler
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString prefName(someData);

    if (prefName.Equals(NS_LITERAL_STRING(kBrowserStaticPrefKey))) {
      // refresh menus which share this pref
      rv = mCharsetMenu->RefreshBroserMenu();
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mCharsetMenu->RefreshMailviewMenu();
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mCharsetMenu->RefreshComposerMenu();
    }
    else if (prefName.Equals(NS_LITERAL_STRING(kMaileditPrefKey))) {
      rv = mCharsetMenu->RefreshMaileditMenu();
    }
  }

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu:Observe");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu:Observe");
  return rv;
}

//----------------------------------------------------------------------------
// Class nsCharsetMenu [implementation]

NS_IMPL_ISUPPORTS2(nsCharsetMenu, nsIRDFDataSource, nsICurrentCharsetListener)

nsIRDFDataSource * nsCharsetMenu::mInner = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserAutodetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMoreCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore1CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore2CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore3CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore4CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BrowserMore5CharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_MaileditCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_MailviewCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_ComposerCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_DecodersRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Name = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Checked = NULL;
nsIRDFResource * nsCharsetMenu::kNC_CharsetDetector = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BookmarkSeparator = NULL;
nsIRDFResource * nsCharsetMenu::kRDF_type = NULL;

nsCharsetMenu::nsCharsetMenu() 
: mInitialized(PR_FALSE), 
  mOthersInitialized(PR_FALSE),
  mBrowserMenuInitialized(PR_FALSE),
  mMailviewMenuInitialized(PR_FALSE),
  mComposerMenuInitialized(PR_FALSE),
  mMaileditMenuInitialized(PR_FALSE),
  mSecondaryTiersInitialized(PR_FALSE),
  mAutoDetectInitialized(PR_FALSE)
{
  NS_INIT_REFCNT();
  NS_TIMELINE_START_TIMER("nsCharsetMenu::nsCharsetMenu");
  nsresult res = NS_OK;

  //get charset manager
  mCCManager = do_GetService(kCharsetConverterManagerCID, &res);

  //initialize skeleton RDF source
  mRDFService = do_GetService(kRDFServiceCID, &res);

  if (NS_SUCCEEDED(res))  {
    res = mRDFService->RegisterDataSource(this, PR_FALSE);
  
    res = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID, nsnull, 
          NS_GET_IID(nsIRDFDataSource), (void**) &mInner);

    mRDFService->GetResource(kURINC_BrowserCharsetMenuRoot, &kNC_BrowserCharsetMenuRoot);
  }

  //get pref service
  nsCOMPtr<nsIPrefService> PrefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &res);
  if (NS_SUCCEEDED(res))
    res = PrefService->GetBranch(nsnull, getter_AddRefs(mPrefs));

  //register event listener
  mCharsetMenuObserver = new nsCharsetMenuObserver(this);

  if (mCharsetMenuObserver)  {
    nsCOMPtr<nsIObserverService> observerService = 
             do_GetService("@mozilla.org/observer-service;1", &res);

    if (NS_SUCCEEDED(res))
      res = observerService->AddObserver(mCharsetMenuObserver, 
                                         "charsetmenu-selected", 
                                         PR_FALSE);
  }

  NS_ASSERTION(NS_SUCCEEDED(res), "Failed to initialize nsCharsetMenu");
  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::nsCharsetMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::nsCharsetMenu");
}

nsCharsetMenu::~nsCharsetMenu() 
{
  Done();

  FreeMenuItemArray(&mBrowserMenu);
  FreeMenuItemArray(&mMailviewMenu);
  FreeMenuItemArray(&mComposerMenu);

  FreeResources();
}

// XXX collapse these 2 in one

nsresult nsCharsetMenu::RefreshBroserMenu()
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIRDFContainer> container;
  res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // clean the menu
  res = ClearMenu(container, &mBrowserMenu);
  if (NS_FAILED(res)) return res;

  // rebuild the menu
  nsCOMPtr<nsISupportsArray> decs;
  res = mCCManager->GetDecoderList(getter_AddRefs(decs));
  if (NS_FAILED(res)) return res;

  res = AddFromPrefsToMenu(&mBrowserMenu, container, kBrowserStaticPrefKey, 
    decs, "charset.");
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing static charset menu from prefs");

  // mark the end of the static area, the rest is cache
  mBrowserCacheStart = mBrowserMenu.Count();

  res = InitCacheMenu(decs, kNC_BrowserCharsetMenuRoot, kBrowserCachePrefKey, 
    &mBrowserMenu);
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing browser cache charset menu");

  return res;
}

nsresult nsCharsetMenu::RefreshMailviewMenu()
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIRDFContainer> container;
  res = NewRDFContainer(mInner, kNC_MailviewCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // clean the menu
  res = ClearMenu(container, &mMailviewMenu);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsISupportsArray> decs;
  res = mCCManager->GetDecoderList(getter_AddRefs(decs));
  if (NS_FAILED(res)) return res;

  res = AddFromPrefsToMenu(&mMailviewMenu, container, kMailviewStaticPrefKey, 
    decs, "charset.");
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing static charset menu from prefs");

  // mark the end of the static area, the rest is cache
  mMailviewCacheStart = mMailviewMenu.Count();

  res = InitCacheMenu(decs, kNC_MailviewCharsetMenuRoot, 
    kMailviewCachePrefKey, &mMailviewMenu);
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing mailview cache charset menu");

  return res;
}

nsresult nsCharsetMenu::RefreshMaileditMenu()
{
  nsresult res;

  nsCOMPtr<nsIRDFContainer> container;
  res = NewRDFContainer(mInner, kNC_MaileditCharsetMenuRoot, getter_AddRefs(container));
  NS_ENSURE_SUCCESS(res, res);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  res = container->GetElements(getter_AddRefs(enumerator));
  NS_ENSURE_SUCCESS(res, res);

  // clear the menu
  nsCOMPtr<nsIRDFNode> node;
  while (NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(node)))) {

    res = mInner->Unassert(kNC_MaileditCharsetMenuRoot, kNC_Name, node);
    NS_ENSURE_SUCCESS(res, res);

    res = container->RemoveElement(node, PR_FALSE);
    NS_ENSURE_SUCCESS(res, res);
  }

  // get a list of available encoders
  nsCOMPtr<nsISupportsArray> encs;
  res = mCCManager->GetEncoderList(getter_AddRefs(encs));
  NS_ENSURE_SUCCESS(res, res);

  // add menu items from pref
  res = AddFromPrefsToMenu(NULL, container, kMaileditPrefKey, encs, NULL);
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing mailedit charset menu from prefs");

  return res;
}

nsresult nsCharsetMenu::RefreshComposerMenu()
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIRDFContainer> container;
  res = NewRDFContainer(mInner, kNC_ComposerCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // clean the menu
  res = ClearMenu(container, &mComposerMenu);
  if (NS_FAILED(res)) return res;

  nsCOMPtr<nsISupportsArray> decs;
  res = mCCManager->GetDecoderList(getter_AddRefs(decs));
  if (NS_FAILED(res)) return res;

  res = AddFromPrefsToMenu(&mComposerMenu, container, kComposerStaticPrefKey, 
    decs, "charset.");
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing static charset menu from prefs");

  // mark the end of the static area, the rest is cache
  mComposerCacheStart = mComposerMenu.Count();

  res = InitCacheMenu(decs, kNC_ComposerCharsetMenuRoot, 
    kComposerCachePrefKey, &mComposerMenu);
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing composer cache charset menu");

  return res;
}

nsresult nsCharsetMenu::Init() 
{
  nsresult res = NS_OK;

  if (!mInitialized)  {

    //enumerate decoders
    res = mCCManager->GetDecoderList(getter_AddRefs(mDecoderList));
    if (NS_FAILED(res)) return res;

    //initialize all remaining RDF template nodes
    mRDFService->GetResource(kURINC_BrowserAutodetMenuRoot, &kNC_BrowserAutodetMenuRoot);
    mRDFService->GetResource(kURINC_BrowserMoreCharsetMenuRoot, &kNC_BrowserMoreCharsetMenuRoot);
    mRDFService->GetResource(kURINC_BrowserMore1CharsetMenuRoot, &kNC_BrowserMore1CharsetMenuRoot);
    mRDFService->GetResource(kURINC_BrowserMore2CharsetMenuRoot, &kNC_BrowserMore2CharsetMenuRoot);
    mRDFService->GetResource(kURINC_BrowserMore3CharsetMenuRoot, &kNC_BrowserMore3CharsetMenuRoot);
    mRDFService->GetResource(kURINC_BrowserMore4CharsetMenuRoot, &kNC_BrowserMore4CharsetMenuRoot);
    mRDFService->GetResource(kURINC_BrowserMore5CharsetMenuRoot, &kNC_BrowserMore5CharsetMenuRoot);
    mRDFService->GetResource(kURINC_MaileditCharsetMenuRoot, &kNC_MaileditCharsetMenuRoot);
    mRDFService->GetResource(kURINC_MailviewCharsetMenuRoot, &kNC_MailviewCharsetMenuRoot);
    mRDFService->GetResource(kURINC_ComposerCharsetMenuRoot, &kNC_ComposerCharsetMenuRoot);
    mRDFService->GetResource(kURINC_DecodersRoot, &kNC_DecodersRoot);
    mRDFService->GetResource(kURINC_Name, &kNC_Name);
    mRDFService->GetResource(kURINC_Checked, &kNC_Checked);
    mRDFService->GetResource(kURINC_CharsetDetector, &kNC_CharsetDetector);
    mRDFService->GetResource(kURINC_BookmarkSeparator, &kNC_BookmarkSeparator);
    mRDFService->GetResource(kURINC_type, &kRDF_type);

    nsIRDFContainerUtils * rdfUtil = NULL;
    res = nsServiceManager::GetService(kRDFContainerUtilsCID, 
      NS_GET_IID(nsIRDFContainerUtils), (nsISupports **)&rdfUtil);
    if (NS_FAILED(res)) goto done;

    res = rdfUtil->MakeSeq(mInner, kNC_BrowserAutodetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserMoreCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore1CharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore2CharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore3CharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore4CharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserMore5CharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_MaileditCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_MailviewCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_ComposerCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_DecodersRoot, NULL);
    if (NS_FAILED(res)) goto done;

  done:
    if (rdfUtil != NULL) nsServiceManager::ReleaseService(kRDFContainerUtilsCID,rdfUtil);
    if (NS_FAILED(res)) return res;
  }
  mInitialized = NS_SUCCEEDED(res);
  return res;
}

nsresult nsCharsetMenu::Done() 
{
  nsresult res = NS_OK;
  res = mRDFService->UnregisterDataSource(this);

  NS_IF_RELEASE(kNC_BrowserAutodetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserCharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMoreCharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore1CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore2CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore3CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore4CharsetMenuRoot);
  NS_IF_RELEASE(kNC_BrowserMore5CharsetMenuRoot);
  NS_IF_RELEASE(kNC_MaileditCharsetMenuRoot);
  NS_IF_RELEASE(kNC_MailviewCharsetMenuRoot);
  NS_IF_RELEASE(kNC_ComposerCharsetMenuRoot);
  NS_IF_RELEASE(kNC_DecodersRoot);
  NS_IF_RELEASE(kNC_Name);
  NS_IF_RELEASE(kNC_Checked);
  NS_IF_RELEASE(kNC_CharsetDetector);
  NS_IF_RELEASE(kNC_BookmarkSeparator);
  NS_IF_RELEASE(kRDF_type);
  NS_IF_RELEASE(mInner);

  return res;
}

nsresult nsCharsetMenu::SetCharsetCheckmark(nsString * aCharset, 
                                            PRBool aValue)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container;
  nsCOMPtr<nsIRDFResource> node;

  res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // find RDF node for given charset
  char csID[256];
  aCharset->ToCString(csID, sizeof(csID));
  res = mRDFService->GetResource(csID, getter_AddRefs(node));
  if (NS_FAILED(res)) return res;

  // set checkmark value
  nsCOMPtr<nsIRDFLiteral> checkedLiteral;
  nsAutoString checked; checked.AssignWithConversion((aValue == PR_TRUE) ? "true" : "false");
  res = mRDFService->GetLiteral(checked.get(), getter_AddRefs(checkedLiteral));
  if (NS_FAILED(res)) return res;
  res = Assert(node, kNC_Checked, checkedLiteral, PR_TRUE);
  if (NS_FAILED(res)) return res;

  return res;
}

/**
 * Free the resources no longer needed by the component.
 */
nsresult nsCharsetMenu::FreeResources()
{
  nsresult res = NS_OK;

  if (mCharsetMenuObserver) {
    nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(mPrefs);
    if (pbi) {
      pbi->RemoveObserver(kBrowserStaticPrefKey, mCharsetMenuObserver);
      pbi->RemoveObserver(kMaileditPrefKey, mCharsetMenuObserver);
    }
    nsCOMPtr<nsIObserverService> observerService = 
         do_GetService("@mozilla.org/observer-service;1", &res);

    if (NS_SUCCEEDED(res))
      res = observerService->RemoveObserver(mCharsetMenuObserver, 
                                            "charsetmenu-selected");
  }

  mRDFService = NULL;
  mCCManager  = NULL;
  mPrefs      = NULL;

  return res;
}


nsresult nsCharsetMenu::InitBrowserMenu() 
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitBrowserMenu");

  nsresult res = NS_OK;

  if (!mBrowserMenuInitialized)  {
    nsCOMPtr<nsIRDFContainer> container;
    res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsISupportsArray> browserDecoderList;
    res = mDecoderList->Clone(getter_AddRefs(browserDecoderList));
    if (NS_FAILED(res))  return res;

    // even if we fail, the show must go on
    res = InitStaticMenu(browserDecoderList, kNC_BrowserCharsetMenuRoot, 
      kBrowserStaticPrefKey, &mBrowserMenu);
    NS_ASSERTION(NS_SUCCEEDED(res), "error initializing browser static charset menu");

    // mark the end of the static area, the rest is cache
    mBrowserCacheStart = mBrowserMenu.Count();
    mPrefs->GetIntPref(kBrowserCacheSizePrefKey, &mBrowserCacheSize);

    // compute the position of the menu in the RDF container
    res = container->GetCount(&mBrowserMenuRDFPosition);
    if (NS_FAILED(res)) return res;
    // this "1" here is a correction necessary because the RDF container 
    // elements are numbered from 1 (why god, WHY?!?!?!)
    mBrowserMenuRDFPosition -= mBrowserCacheStart - 1;

    res = InitCacheMenu(browserDecoderList, kNC_BrowserCharsetMenuRoot, kBrowserCachePrefKey, 
      &mBrowserMenu);
    NS_ASSERTION(NS_SUCCEEDED(res), "error initializing browser cache charset menu");

    // register prefs callback
    nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(mPrefs);
    if (pbi)
      res = pbi->AddObserver(kBrowserStaticPrefKey, mCharsetMenuObserver, PR_FALSE);
  }

  mBrowserMenuInitialized = NS_SUCCEEDED(res);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitBrowserMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitBrowserMenu");

  return res;
}

nsresult nsCharsetMenu::InitMaileditMenu() 
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitMaileditMenu");

  nsresult res = NS_OK;

  if (!mMaileditMenuInitialized)  {
    nsCOMPtr<nsIRDFContainer> container;
    res = NewRDFContainer(mInner, kNC_MaileditCharsetMenuRoot, getter_AddRefs(container));
    if (NS_FAILED(res)) return res;

    //enumerate encoders
    nsCOMPtr<nsISupportsArray> maileditEncoderList;
    res = mCCManager->GetEncoderList(getter_AddRefs(maileditEncoderList));
    if (NS_FAILED(res))  return res;

    res = AddFromPrefsToMenu(NULL, container, kMaileditPrefKey, maileditEncoderList, NULL);
    NS_ASSERTION(NS_SUCCEEDED(res), "error initializing mailedit charset menu from prefs");

    // register prefs callback
    nsCOMPtr<nsIPrefBranchInternal> pbi = do_QueryInterface(mPrefs);
    if (pbi)
      res = pbi->AddObserver(kMaileditPrefKey, mCharsetMenuObserver, PR_FALSE);
  }

  mMaileditMenuInitialized = NS_SUCCEEDED(res);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitMaileditMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitMaileditMenu");

  return res;
}

nsresult nsCharsetMenu::InitMailviewMenu() 
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitMailviewMenu");
  
  nsresult res = NS_OK;

  if (!mMailviewMenuInitialized)  {
    nsCOMPtr<nsIRDFContainer> container;
    res = NewRDFContainer(mInner, kNC_MailviewCharsetMenuRoot, getter_AddRefs(container));
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsISupportsArray> mailviewDecoderList;
    res = mDecoderList->Clone(getter_AddRefs(mailviewDecoderList));
    if (NS_FAILED(res))  return res;

    // even if we fail, the show must go on
    res = InitStaticMenu(mailviewDecoderList, kNC_MailviewCharsetMenuRoot, 
      kMailviewStaticPrefKey, &mMailviewMenu);
    NS_ASSERTION(NS_SUCCEEDED(res), "error initializing mailview static charset menu");

    // mark the end of the static area, the rest is cache
    mMailviewCacheStart = mMailviewMenu.Count();
    mPrefs->GetIntPref(kMailviewCacheSizePrefKey, &mMailviewCacheSize);

    // compute the position of the menu in the RDF container
    res = container->GetCount(&mMailviewMenuRDFPosition);
    if (NS_FAILED(res)) return res;
    // this "1" here is a correction necessary because the RDF container 
    // elements are numbered from 1 (why god, WHY?!?!?!)
    mMailviewMenuRDFPosition -= mMailviewCacheStart - 1;

    res = InitCacheMenu(mailviewDecoderList, kNC_MailviewCharsetMenuRoot, 
      kMailviewCachePrefKey, &mMailviewMenu);
    NS_ASSERTION(NS_SUCCEEDED(res), "error initializing mailview cache charset menu");
  }

  mMailviewMenuInitialized = NS_SUCCEEDED(res);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitMailviewMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitMailviewMenu");

  return res;
}

nsresult nsCharsetMenu::InitComposerMenu() 
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitComposerMenu");
 
  nsresult res = NS_OK;

  if (!mComposerMenuInitialized)  {
    nsCOMPtr<nsIRDFContainer> container;
    res = NewRDFContainer(mInner, kNC_ComposerCharsetMenuRoot, getter_AddRefs(container));
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsISupportsArray> composerDecoderList;
    res = mDecoderList->Clone(getter_AddRefs(composerDecoderList));
    if (NS_FAILED(res))  return res;

    // even if we fail, the show must go on
    res = InitStaticMenu(composerDecoderList, kNC_ComposerCharsetMenuRoot, 
      kComposerStaticPrefKey, &mComposerMenu);
    NS_ASSERTION(NS_SUCCEEDED(res), "error initializing composer static charset menu");

    // mark the end of the static area, the rest is cache
    mComposerCacheStart = mComposerMenu.Count();
    mPrefs->GetIntPref(kComposerCacheSizePrefKey, &mComposerCacheSize);

    // compute the position of the menu in the RDF container
    res = container->GetCount(&mComposerMenuRDFPosition);
    if (NS_FAILED(res)) return res;
    // this "1" here is a correction necessary because the RDF container 
    // elements are numbered from 1 (why god, WHY?!?!?!)
    mComposerMenuRDFPosition -= mComposerCacheStart - 1;

    res = InitCacheMenu(composerDecoderList, kNC_ComposerCharsetMenuRoot, 
      kComposerCachePrefKey, &mComposerMenu);
    NS_ASSERTION(NS_SUCCEEDED(res), "error initializing composer cache charset menu");
  }

  mComposerMenuInitialized = NS_SUCCEEDED(res);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitComposerMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitComposerMenu");
  
  return res;
}

nsresult nsCharsetMenu::InitOthers() 
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitOthers");

  nsresult res = NS_OK;

  if (!mOthersInitialized) {
    nsCOMPtr<nsISupportsArray> othersDecoderList;
    res = mDecoderList->Clone(getter_AddRefs(othersDecoderList));
    if (NS_FAILED(res))  return res;

    res = InitMoreMenu(othersDecoderList, kNC_DecodersRoot, ".notForBrowser");                 
    if (NS_FAILED(res)) return res;
  }

  mOthersInitialized = NS_SUCCEEDED(res);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitOthers");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitOthers");

  return res;
}

/**
 * Inits the secondary tiers of the charset menu. Because currently all the CS 
 * menus are sharing the secondary tiers, one should call this method only 
 * once for all of them.
 */
nsresult nsCharsetMenu::InitSecondaryTiers()
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitSecondaryTiers");

  nsresult res = NS_OK;

  if (!mSecondaryTiersInitialized)  {
    nsCOMPtr<nsISupportsArray> secondaryTiersDecoderList;
    res = mDecoderList->Clone(getter_AddRefs(secondaryTiersDecoderList));
    if (NS_FAILED(res))  return res;

    res = InitMoreSubmenus(secondaryTiersDecoderList);
    NS_ASSERTION(NS_SUCCEEDED(res), "err init browser charset more submenus");

    res = InitMoreMenu(secondaryTiersDecoderList, kNC_BrowserMoreCharsetMenuRoot, ".notForBrowser");
    NS_ASSERTION(NS_SUCCEEDED(res), "err init browser charset more menu");
  }

  mSecondaryTiersInitialized = NS_SUCCEEDED(res);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitSecondaryTiers");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitSecondaryTiers");

  return res;
}

nsresult nsCharsetMenu::InitStaticMenu(
                        nsISupportsArray * aDecs,
                        nsIRDFResource * aResource, 
                        const char * aKey, 
                        nsVoidArray * aArray)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitStaticMenu");

  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container;

  res = NewRDFContainer(mInner, aResource, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // XXX work around bug that causes the submenus to be first instead of last
  res = AddSeparatorToContainer(container);
  NS_ASSERTION(NS_SUCCEEDED(res), "error adding separator to container");

  res = AddFromPrefsToMenu(aArray, container, aKey, aDecs, "charset.");
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing static charset menu from prefs");

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitStaticMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitStaticMenu");

  return res;
}

nsresult nsCharsetMenu::InitCacheMenu(
                        nsISupportsArray * aDecs,
                        nsIRDFResource * aResource, 
                        const char * aKey, 
                        nsVoidArray * aArray)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitCacheMenu");

  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container;

  res = NewRDFContainer(mInner, aResource, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  res = AddFromNolocPrefsToMenu(aArray, container, aKey, aDecs, "charset.");
  NS_ASSERTION(NS_SUCCEEDED(res), "error initializing cache charset menu from prefs");

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitCacheMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitCacheMenu");

  return res;
}

nsresult nsCharsetMenu::InitAutodetMenu()
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitAutodetMenu");

  nsresult res = NS_OK;

  if (!mAutoDetectInitialized) {
    nsVoidArray chardetArray;
    nsCOMPtr<nsIRDFContainer> container;

    res = NewRDFContainer(mInner, kNC_BrowserAutodetMenuRoot, getter_AddRefs(container));
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsISupportsArray> array;
    res = mCCManager->GetCharsetDetectorList(getter_AddRefs(array));
    if (NS_FAILED(res)) goto done;

    res = AddCharsetArrayToItemArray(&chardetArray, array);
    if (NS_FAILED(res)) goto done;

    // reorder the array
    res = ReorderMenuItemArray(&chardetArray);
    if (NS_FAILED(res)) goto done;

    res = AddMenuItemArrayToContainer(container, &chardetArray, 
      kNC_CharsetDetector);
    if (NS_FAILED(res)) goto done;

  done:
    // free the elements in the VoidArray
    FreeMenuItemArray(&chardetArray);
  }

  mAutoDetectInitialized = NS_SUCCEEDED(res);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitAutodetMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitAutodetMenu");

  return res;
}

nsresult nsCharsetMenu::InitMoreMenu(
                        nsISupportsArray * aDecs, 
                        nsIRDFResource * aResource, 
                        char * aFlag)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitMoreMenu");

  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFContainer> container;
  nsVoidArray moreMenu;
  nsAutoString prop; prop.AssignWithConversion(aFlag);

  res = NewRDFContainer(mInner, aResource, getter_AddRefs(container));
  if (NS_FAILED(res)) goto done;

  // remove charsets "not for browser"
  res = RemoveFlaggedCharsets(aDecs, &prop);
  if (NS_FAILED(res)) goto done;

  res = AddCharsetArrayToItemArray(&moreMenu, aDecs);
  if (NS_FAILED(res)) goto done;

  // reorder the array
  res = ReorderMenuItemArray(&moreMenu);
  if (NS_FAILED(res)) goto done;

  res = AddMenuItemArrayToContainer(container, &moreMenu, NULL);
  if (NS_FAILED(res)) goto done;

done:
  // free the elements in the VoidArray
  FreeMenuItemArray(&moreMenu);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitMoreMenu");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitMoreMenu");

  return res;
}

// XXX please make this method more general; the cut&pasted code is laughable
nsresult nsCharsetMenu::InitMoreSubmenus(nsISupportsArray * aDecs)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitMoreSubmenus");

  nsresult res = NS_OK;

  nsCOMPtr<nsIRDFContainer> container1;
  nsCOMPtr<nsIRDFContainer> container2;
  nsCOMPtr<nsIRDFContainer> container3;
  nsCOMPtr<nsIRDFContainer> container4;
  nsCOMPtr<nsIRDFContainer> container5;
  char * key1 = "intl.charsetmenu.browser.more1";
  char * key2 = "intl.charsetmenu.browser.more2";
  char * key3 = "intl.charsetmenu.browser.more3";
  char * key4 = "intl.charsetmenu.browser.more4";
  char * key5 = "intl.charsetmenu.browser.more5";

  res = NewRDFContainer(mInner, kNC_BrowserMore1CharsetMenuRoot, 
    getter_AddRefs(container1));
  if (NS_FAILED(res)) return res;
  AddFromPrefsToMenu(NULL, container1, key1, aDecs, NULL);

  res = NewRDFContainer(mInner, kNC_BrowserMore2CharsetMenuRoot, 
    getter_AddRefs(container2));
  if (NS_FAILED(res)) return res;
  AddFromPrefsToMenu(NULL, container2, key2, aDecs, NULL);

  res = NewRDFContainer(mInner, kNC_BrowserMore3CharsetMenuRoot, 
    getter_AddRefs(container3));
  if (NS_FAILED(res)) return res;
  AddFromPrefsToMenu(NULL, container3, key3, aDecs, NULL);

  res = NewRDFContainer(mInner, kNC_BrowserMore4CharsetMenuRoot, 
    getter_AddRefs(container4));
  if (NS_FAILED(res)) return res;
  AddFromPrefsToMenu(NULL, container4, key4, aDecs, NULL);

  res = NewRDFContainer(mInner, kNC_BrowserMore5CharsetMenuRoot, 
    getter_AddRefs(container5));
  if (NS_FAILED(res)) return res;
  AddFromPrefsToMenu(NULL, container5, key5, aDecs, NULL);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitMoreSubmenus");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitMoreSubmenus");

  return res;
}

nsresult nsCharsetMenu::AddCharsetToItemArray(
                        nsVoidArray * aArray, 
                        nsIAtom * aCharset, 
                        nsMenuEntry ** aResult,
                        PRInt32 aPlace) 
{
  nsresult res = NS_OK;
  nsMenuEntry * item = NULL; 

  if (aResult != NULL) *aResult = NULL;
  
  item = new nsMenuEntry();
  if (item == NULL) {
    res = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  item->mCharset = aCharset;

  res = mCCManager->GetCharsetTitle2(aCharset, &item->mTitle);
  if (NS_FAILED(res)) {
    res = aCharset->ToString(item->mTitle);
    if (NS_FAILED(res)) goto done;
  }

  if (aArray != NULL) {
    if (aPlace < 0) {
      res = aArray->AppendElement(item);
      if (NS_FAILED(res)) goto done;
    } else {
      res = aArray->InsertElementAt(item, aPlace);
      if (NS_FAILED(res)) goto done;
    }
  }

  if (aResult != NULL) *aResult = item;

  // if we have made another reference to "item", do not delete it 
  if ((aArray != NULL) || (aResult != NULL)) item = NULL; 

done:
  if (item != NULL) delete item;

  return res;
}

nsresult nsCharsetMenu::AddCharsetArrayToItemArray(
                        nsVoidArray * aArray, 
                        nsISupportsArray * aCharsets) 
{
  PRUint32 count;
  nsresult res = aCharsets->Count(&count);
  if (NS_FAILED(res)) return res;

  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIAtom> cs;
    res = aCharsets->GetElementAt(i, getter_AddRefs(cs));
    if (NS_FAILED(res)) return res;

    res = AddCharsetToItemArray(aArray, cs, NULL, -1);
    if (NS_FAILED(res)) return res;
  }

  return NS_OK;
}

// aPlace < -1 for Remove
// aPlace < 0 for Append
nsresult nsCharsetMenu::AddMenuItemToContainer(
                        nsIRDFContainer * aContainer,
                        nsMenuEntry * aItem,
                        nsIRDFResource * aType,
                        char * aIDPrefix,
                        PRInt32 aPlace) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFResource> node;

  nsAutoString cs;
  res = aItem->mCharset->ToString(cs);
  if (NS_FAILED(res)) return res;

  nsAutoString id;
  if (aIDPrefix != NULL) id.AssignWithConversion(aIDPrefix);
  id.Append(cs);

  // Make up a unique ID and create the RDF NODE
  char csID[256];
  id.ToCString(csID, sizeof(csID));
  res = mRDFService->GetResource(csID, getter_AddRefs(node));
  if (NS_FAILED(res)) return res;

  const PRUnichar * title = aItem->mTitle.get();

  // set node's title
  nsCOMPtr<nsIRDFLiteral> titleLiteral;
  res = mRDFService->GetLiteral(title, getter_AddRefs(titleLiteral));
  if (NS_FAILED(res)) return res;

  if (aPlace < -1) {
    res = Unassert(node, kNC_Name, titleLiteral);
    if (NS_FAILED(res)) return res;
  } else {
    res = Assert(node, kNC_Name, titleLiteral, PR_TRUE);
    if (NS_FAILED(res)) return res;
  }

  if (aType != NULL) {
    if (aPlace < -1) {
      res = Unassert(node, kRDF_type, aType);
      if (NS_FAILED(res)) return res;
    } else {
      res = Assert(node, kRDF_type, aType, PR_TRUE);
      if (NS_FAILED(res)) return res;
    }
  }

  // Add the element to the container
  if (aPlace < -1) {
    res = aContainer->RemoveElement(node, PR_TRUE);
    if (NS_FAILED(res)) return res;
  } else if (aPlace < 0) {
    res = aContainer->AppendElement(node);
    if (NS_FAILED(res)) return res;
  } else {
    res = aContainer->InsertElementAt(node, aPlace, PR_TRUE);
    if (NS_FAILED(res)) return res;
  } 

  return res;
}

nsresult nsCharsetMenu::AddMenuItemArrayToContainer(
                        nsIRDFContainer * aContainer,
                        nsVoidArray * aArray,
                        nsIRDFResource * aType) 
{
  PRUint32 count = aArray->Count();
  nsresult res = NS_OK;

  for (PRUint32 i = 0; i < count; i++) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(i);
    if (item == NULL) return NS_ERROR_UNEXPECTED;

    res = AddMenuItemToContainer(aContainer, item, aType, NULL, -1);
    if (NS_FAILED(res)) return res;
  }

  return NS_OK;
}

nsresult nsCharsetMenu::AddCharsetToContainer(
                        nsVoidArray * aArray, 
                        nsIRDFContainer * aContainer, 
                        nsIAtom * aCharset, 
                        char * aIDPrefix,
                        PRInt32 aPlace,
						PRInt32 aRDFPlace)
{
  nsresult res = NS_OK;
  nsMenuEntry * item = NULL; 
  
  res = AddCharsetToItemArray(aArray, aCharset, &item, aPlace);
  if (NS_FAILED(res)) goto done;

  res = AddMenuItemToContainer(aContainer, item, NULL, aIDPrefix, 
    aPlace + aRDFPlace);
  if (NS_FAILED(res)) goto done;

  // if we have made another reference to "item", do not delete it 
  if (aArray != NULL) item = NULL; 

done:
  if (item != NULL) delete item;

  return res;
}

nsresult nsCharsetMenu::AddFromPrefsToMenu(
                        nsVoidArray * aArray, 
                        nsIRDFContainer * aContainer, 
                        const char * aKey, 
                        nsISupportsArray * aDecs, 
                        char * aIDPrefix)
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIPrefLocalizedString> pls;
  res = mPrefs->GetComplexValue(aKey, NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(pls));
  if (NS_FAILED(res)) return res;

  if (pls) {
    nsXPIDLString ucsval;
    pls->ToString(getter_Copies(ucsval));
    if (ucsval)
      res = AddFromStringToMenu(NS_CONST_CAST(char *, NS_ConvertUCS2toUTF8(ucsval).get()), aArray,
        aContainer, aDecs, aIDPrefix);
  }

  return res;
}

nsresult nsCharsetMenu::AddFromNolocPrefsToMenu(
                        nsVoidArray * aArray, 
                        nsIRDFContainer * aContainer, 
                        const char * aKey, 
                        nsISupportsArray * aDecs, 
                        char * aIDPrefix)
{
  nsresult res = NS_OK;

  char * value = NULL;
  res = mPrefs->GetCharPref(aKey, &value);
  if (NS_FAILED(res)) return res;

  if (value != NULL) {
    res = AddFromStringToMenu(value, aArray, aContainer, aDecs, aIDPrefix);
    nsMemory::Free(value);
  }

  return res;
}

nsresult nsCharsetMenu::AddFromStringToMenu(
                        char * aCharsetList, 
                        nsVoidArray * aArray, 
                        nsIRDFContainer * aContainer, 
                        nsISupportsArray * aDecs, 
                        char * aIDPrefix)
{
  nsresult res = NS_OK;
  char * p = aCharsetList;
  char * q = p;
  while (*p != 0) {
	  for (; (*q != ',') && (*q != ' ') && (*q != 0); q++) {;}
    char temp = *q;
    *q = 0;

    nsCOMPtr<nsIAtom> atom;
    res = mCCManager->GetCharsetAtom2(p, getter_AddRefs(atom));
    NS_ASSERTION(NS_SUCCEEDED(res), "cannot get charset atom");
    if (NS_FAILED(res)) break;

    // if this charset is not on the accepted list of charsets, ignore it
    PRInt32 index;
    res = aDecs->GetIndexOf(atom, &index);
    if (NS_SUCCEEDED(res) && (index >= 0)) {

      // else, add it to the menu
      res = AddCharsetToContainer(aArray, aContainer, atom, aIDPrefix, -1, 0);
      NS_ASSERTION(NS_SUCCEEDED(res), "cannot add charset to menu");
      if (NS_FAILED(res)) break;

      res = aDecs->RemoveElement(atom);
      NS_ASSERTION(NS_SUCCEEDED(res), "cannot remove atom from array");
    }

    *q = temp;
    for (; (*q == ',') || (*q == ' '); q++) {;}
    p=q;
  }

  return NS_OK;
}

nsresult nsCharsetMenu::AddSeparatorToContainer(nsIRDFContainer * aContainer)
{
  nsAutoString str;
  str.AssignWithConversion("----");

  // hack to generate unique id's for separators
  static PRInt32 u = 0;
  u++;
  str.AppendInt(u);

  nsMenuEntry item;
  item.mCharset = getter_AddRefs(NS_NewAtom(str));
  item.mTitle.Assign(str);

  return AddMenuItemToContainer(aContainer, &item, kNC_BookmarkSeparator, 
    NULL, -1);
}

nsresult nsCharsetMenu::AddCharsetToCache(
                        nsIAtom * aCharset,
                        nsVoidArray * aArray,
                        nsIRDFResource * aRDFResource, 
                        PRInt32 aCacheStart, 
						PRInt32 aCacheSize,
						PRInt32 aRDFPlace)
{
  PRInt32 i;
  nsresult res = NS_OK;

  i = FindMenuItemInArray(aArray, aCharset, NULL);
  if (i >= 0) return res;

  nsCOMPtr<nsIRDFContainer> container;
  res = NewRDFContainer(mInner, aRDFResource, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // iff too many items, remove last one
  if (aArray->Count() - aCacheStart >= aCacheSize){
    res = RemoveLastMenuItem(container, aArray);
    if (NS_FAILED(res)) return res;
  }

  res = AddCharsetToContainer(aArray, container, aCharset, "charset.", 
    aCacheStart, aRDFPlace);

  return res;
}

nsresult nsCharsetMenu::WriteCacheToPrefs(nsVoidArray * aArray, 
                                          PRInt32 aCacheStart, 
                                          const char * aKey)
{
  nsresult res = NS_OK;

  // create together the cache string
  nsAutoString cache;
  nsAutoString sep; sep.AppendWithConversion(", ");
  PRInt32 count = aArray->Count();

  for (PRInt32 i = aCacheStart; i < count; i++) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(i);
    if (item != NULL) {    
      nsAutoString cs;
      res = item->mCharset->ToString(cs);
      if (NS_SUCCEEDED(res)) {
        cache.Append(cs);
        if (i < count - 1) {
          cache.Append(sep);
        }
      }
    }
  }

  // write the pref
  res = mPrefs->SetCharPref(aKey, NS_ConvertUCS2toUTF8(cache).get());

  return res;
}

nsresult nsCharsetMenu::UpdateCachePrefs(const char * aCacheKey,
                                         const char * aCacheSizeKey,
                                         const char * aStaticKey,
                                         const PRUnichar * aCharset)
{
  nsresult res = NS_OK;
  char * cachePrefValue = NULL;
  char * staticPrefValue = NULL;
  const char * currentCharset = NS_ConvertUCS2toUTF8(aCharset).get();
  PRInt32 cacheSize = 0;

  res = mPrefs->GetCharPref(aCacheKey, &cachePrefValue);
  res = mPrefs->GetCharPref(aStaticKey, &staticPrefValue);
  res = mPrefs->GetIntPref(aCacheSizeKey, &cacheSize);

  nsCAutoString strCachePrefValue(cachePrefValue);
  nsCAutoString strStaticPrefValue(staticPrefValue);

  if ((strCachePrefValue.Find(currentCharset) == -1) && 
      (strStaticPrefValue.Find(currentCharset) == -1)) {

    if (!strCachePrefValue.IsEmpty())
      strCachePrefValue.Insert(", ", 0);

    strCachePrefValue.Insert(currentCharset, 0);
    if ((cacheSize - 1) < (PRInt32) strCachePrefValue.CountChar(','))
      strCachePrefValue.Truncate(strCachePrefValue.RFindChar(','));

    res = mPrefs->SetCharPref(aCacheKey, PromiseFlatCString(strCachePrefValue).get());
  }

  nsMemory::Free(cachePrefValue);
  nsMemory::Free(staticPrefValue);
  return res;
}

nsresult nsCharsetMenu::ClearMenu(nsIRDFContainer * aContainer,  
                                  nsVoidArray * aArray)
{
  nsresult res = NS_OK;

  // clean the RDF data source
  PRInt32 count = aArray->Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(i);
    if (item != NULL) {    
      res = AddMenuItemToContainer(aContainer, item, NULL, "charset.", -2);
      if (NS_FAILED(res)) return res;
    }
  }

  // clean the internal data structures
  FreeMenuItemArray(aArray);

  return res;
}

nsresult nsCharsetMenu::RemoveLastMenuItem(nsIRDFContainer * aContainer,
                                           nsVoidArray * aArray)
{
  nsresult res = NS_OK;

  PRInt32 last = aArray->Count() - 1;
  nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(last);
  if (item != NULL) {    
    res = AddMenuItemToContainer(aContainer, item, NULL, "charset.", -2);
    if (NS_FAILED(res)) return res;

    res = aArray->RemoveElementAt(last);
    if (NS_FAILED(res)) return res;
  }

  return res;
}

nsresult nsCharsetMenu::RemoveFlaggedCharsets(
                        nsISupportsArray * aList, 
                        nsString * aProp)
{
  nsresult res = NS_OK;
  PRUint32 count;

  res = aList->Count(&count);
  if (NS_FAILED(res)) return res;

  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIAtom> atom;
    res = aList->GetElementAt(i, getter_AddRefs(atom));
    if (NS_FAILED(res)) continue;

    nsAutoString str;
    res = mCCManager->GetCharsetData2(atom, aProp->get(), &str);
    if (NS_FAILED(res)) continue;

    res = aList->RemoveElement(atom);
    if (NS_FAILED(res)) continue;

    i--; 
    count--;
  }

  return NS_OK;
}

nsresult nsCharsetMenu::NewRDFContainer(nsIRDFDataSource * aDataSource, 
                                        nsIRDFResource * aResource, 
                                        nsIRDFContainer ** aResult)
{
  nsresult res;

  res = nsComponentManager::CreateInstance(kRDFContainerCID, NULL, 
    NS_GET_IID(nsIRDFContainer), (void**)aResult);
  if (NS_FAILED(res)) return res;

  res = (*aResult)->Init(aDataSource, aResource);
  if (NS_FAILED(res)) NS_RELEASE(*aResult);

  return res;
}

void nsCharsetMenu::FreeMenuItemArray(nsVoidArray * aArray)
{
  PRUint32 count = aArray->Count();
  for (PRUint32 i = 0; i < count; i++) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(i);
    if (item != NULL) {
      delete item;
    }
  }
  aArray->Clear();
}

PRInt32 nsCharsetMenu::FindMenuItemInArray(nsVoidArray * aArray, 
                                           nsIAtom * aCharset, 
                                           nsMenuEntry ** aResult)
{
  PRUint32 count = aArray->Count();

  for (PRUint32 i=0; i < count; i++) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(i);
    if ((item->mCharset).get() == aCharset) {
      if (aResult != NULL) *aResult = item;
      return i;
    }
  }

  if (aResult != NULL) *aResult = NULL;
  return -1;
}

nsresult nsCharsetMenu::ReorderMenuItemArray(nsVoidArray * aArray)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsICollation> collation;
  PRUint32 count = aArray->Count();
  PRUint32 i;

  // we need to use a temporary array
  charsetMenuSortRecord *array = new charsetMenuSortRecord [count];
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
  for (i = 0; i < count; i++)
    array[i].key = nsnull;

  res = GetCollation(getter_AddRefs(collation));
  if (NS_FAILED(res))
    goto done;

  for (i = 0; i < count && NS_SUCCEEDED(res); i++) {
    array[i].item = (nsMenuEntry *)aArray->ElementAt(i);

    res = collation->GetSortKeyLen(kCollationCaseInSensitive, 
                                   (array[i].item)->mTitle, &array[i].len);

    if (NS_SUCCEEDED(res)) {
      array[i].key = new PRUint8 [array[i].len];
      if (!(array[i].key)) {
        res = NS_ERROR_OUT_OF_MEMORY;
        goto done;
      }
      res = collation->CreateRawSortKey(kCollationCaseInSensitive, 
                                       (array[i].item)->mTitle, array[i].key, &array[i].len);
    }
  }

  // reorder the array
  if (NS_SUCCEEDED(res)) {
    NS_QuickSort(array, count, sizeof(*array), CompareMenuItems, collation);

    // move the elements from the temporary array into the the real one
    aArray->Clear();
    for (i = 0; i < count; i++) {
      aArray->AppendElement(array[i].item);
    }
  }

done:
  for (i = 0; i < count; i++) {
    if (array[i].key)
      delete [] array[i].key;
  }
  delete [] array;
  return res;
}

nsresult nsCharsetMenu::GetCollation(nsICollation ** aCollation)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsILocale> locale = nsnull;
  nsICollationFactory * collationFactory = nsnull;
  
  nsCOMPtr<nsILocaleService> localeServ = 
           do_GetService(kLocaleServiceCID, &res);
  if (NS_FAILED(res)) return res;

  res = localeServ->GetApplicationLocale(getter_AddRefs(locale));
  if (NS_FAILED(res)) return res;

  res = nsComponentManager::CreateInstance(kCollationFactoryCID, NULL, 
      NS_GET_IID(nsICollationFactory), (void**) &collationFactory);
  if (NS_FAILED(res)) return res;

  res = collationFactory->CreateCollation(locale, aCollation);
  NS_RELEASE(collationFactory);
  return res;
}

//----------------------------------------------------------------------------
// Interface nsICurrentCharsetListener [implementation]

NS_IMETHODIMP nsCharsetMenu::SetCurrentCharset(const PRUnichar * aCharset)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu:SetCurrentCharset");
  nsresult res;

  if (mBrowserMenuInitialized) {
    nsCOMPtr<nsIAtom> atom;
    res = mCCManager->GetCharsetAtom(aCharset, getter_AddRefs(atom));
    if (NS_FAILED(res)) {
        NS_TIMELINE_LEAVE("nsCharsetMenu:SetCurrentCharset");
        return res;
    }
    res = AddCharsetToCache(atom, &mBrowserMenu, kNC_BrowserCharsetMenuRoot, 
      mBrowserCacheStart, mBrowserCacheSize, mBrowserMenuRDFPosition);
    if (NS_FAILED(res)) {
        NS_TIMELINE_LEAVE("nsCharsetMenu:SetCurrentCharset");
        return res;
    }

    res = WriteCacheToPrefs(&mBrowserMenu, mBrowserCacheStart, 
      kBrowserCachePrefKey);
  } else {
    UpdateCachePrefs(kBrowserCachePrefKey, kBrowserCacheSizePrefKey, 
                     kBrowserStaticPrefKey, aCharset);
  }
  NS_TIMELINE_STOP_TIMER("nsCharsetMenu:SetCurrentCharset");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu:SetCurrentCharset");
  return res;
}

NS_IMETHODIMP nsCharsetMenu::SetCurrentMailCharset(const PRUnichar * aCharset)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu:SetCurrentMailCharset");
  nsresult res;

  if (mMailviewMenuInitialized) {
    nsCOMPtr<nsIAtom> atom;
    res = mCCManager->GetCharsetAtom(aCharset, getter_AddRefs(atom));
    if (NS_FAILED(res)) return res;

    res = AddCharsetToCache(atom, &mMailviewMenu, kNC_MailviewCharsetMenuRoot, 
      mMailviewCacheStart, mMailviewCacheSize, mMailviewMenuRDFPosition);
    if (NS_FAILED(res)) return res;

    res = WriteCacheToPrefs(&mMailviewMenu, mMailviewCacheStart, 
      kMailviewCachePrefKey);
  } else {
    UpdateCachePrefs(kMailviewCachePrefKey, kMailviewCacheSizePrefKey, 
                     kMailviewStaticPrefKey, aCharset);
  }
  NS_TIMELINE_STOP_TIMER("nsCharsetMenu:SetCurrentMailCharset");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu:SetCurrentMailCharset");
  return res;
}

NS_IMETHODIMP nsCharsetMenu::SetCurrentComposerCharset(const PRUnichar * aCharset)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu:SetCurrentComposerCharset");
  nsresult res;

  if (mComposerMenuInitialized) {
    nsCOMPtr<nsIAtom> atom;
    res = mCCManager->GetCharsetAtom(aCharset, getter_AddRefs(atom));
    if (NS_FAILED(res)) return res;

    res = AddCharsetToCache(atom, &mComposerMenu, kNC_ComposerCharsetMenuRoot, 
      mComposerCacheStart, mComposerCacheSize, mComposerMenuRDFPosition);
    if (NS_FAILED(res)) return res;

    res = WriteCacheToPrefs(&mComposerMenu, mComposerCacheStart, 
      kComposerCachePrefKey);
  } else {
    UpdateCachePrefs(kComposerCachePrefKey, kComposerCacheSizePrefKey, 
                     kComposerStaticPrefKey, aCharset);
  }
  NS_TIMELINE_STOP_TIMER("nsCharsetMenu:SetCurrentComposerCharset");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu:SetCurrentComposerCharset");
  return res;
}

//----------------------------------------------------------------------------
// Interface nsIRDFDataSource [implementation]

NS_IMETHODIMP nsCharsetMenu::GetURI(char ** uri)
{
  if (!uri) return NS_ERROR_NULL_POINTER;

  *uri = nsCRT::strdup("rdf:charset-menu");
  if (!(*uri)) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsCharsetMenu::GetSource(nsIRDFResource* property,
                                       nsIRDFNode* target,
                                       PRBool tv,
                                       nsIRDFResource** source)
{
  return mInner->GetSource(property, target, tv, source);
}

NS_IMETHODIMP nsCharsetMenu::GetSources(nsIRDFResource* property,
                                        nsIRDFNode* target,
                                        PRBool tv,
                                        nsISimpleEnumerator** sources)
{
  return mInner->GetSources(property, target, tv, sources);
}

NS_IMETHODIMP nsCharsetMenu::GetTarget(nsIRDFResource* source,
                                       nsIRDFResource* property,
                                       PRBool tv,
                                       nsIRDFNode** target)
{
  return mInner->GetTarget(source, property, tv, target);
}

NS_IMETHODIMP nsCharsetMenu::GetTargets(nsIRDFResource* source,
                                        nsIRDFResource* property,
                                        PRBool tv,
                                        nsISimpleEnumerator** targets)
{
  return mInner->GetTargets(source, property, tv, targets);
}

NS_IMETHODIMP nsCharsetMenu::Assert(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue)
{
  // TODO: filter out asserts we don't care about
  return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP nsCharsetMenu::Unassert(nsIRDFResource* aSource,
                                      nsIRDFResource* aProperty,
                                      nsIRDFNode* aTarget)
{
  // TODO: filter out unasserts we don't care about
  return mInner->Unassert(aSource, aProperty, aTarget);
}


NS_IMETHODIMP nsCharsetMenu::Change(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aOldTarget,
                                    nsIRDFNode* aNewTarget)
{
  // TODO: filter out changes we don't care about
  return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP nsCharsetMenu::Move(nsIRDFResource* aOldSource,
                                  nsIRDFResource* aNewSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
  // TODO: filter out changes we don't care about
  return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}


NS_IMETHODIMP nsCharsetMenu::HasAssertion(nsIRDFResource* source, 
                                          nsIRDFResource* property, 
                                          nsIRDFNode* target, PRBool tv, 
                                          PRBool* hasAssertion)
{
  return mInner->HasAssertion(source, property, target, tv, hasAssertion);
}

NS_IMETHODIMP nsCharsetMenu::AddObserver(nsIRDFObserver* n)
{
  return mInner->AddObserver(n);
}

NS_IMETHODIMP nsCharsetMenu::RemoveObserver(nsIRDFObserver* n)
{
  return mInner->RemoveObserver(n);
}

NS_IMETHODIMP 
nsCharsetMenu::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  return mInner->HasArcIn(aNode, aArc, result);
}

NS_IMETHODIMP 
nsCharsetMenu::HasArcOut(nsIRDFResource *source, nsIRDFResource *aArc, PRBool *result)
{
  return mInner->HasArcOut(source, aArc, result);
}

NS_IMETHODIMP nsCharsetMenu::ArcLabelsIn(nsIRDFNode* node, 
                                         nsISimpleEnumerator** labels)
{
  return mInner->ArcLabelsIn(node, labels);
}

NS_IMETHODIMP nsCharsetMenu::ArcLabelsOut(nsIRDFResource* source, 
                                          nsISimpleEnumerator** labels)
{
  return mInner->ArcLabelsOut(source, labels);
}

NS_IMETHODIMP nsCharsetMenu::GetAllResources(nsISimpleEnumerator** aCursor)
{
  return mInner->GetAllResources(aCursor);
}

NS_IMETHODIMP nsCharsetMenu::GetAllCommands(
                             nsIRDFResource* source,
                             nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::GetAllCmds(
                             nsIRDFResource* source,
                             nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::IsCommandEnabled(
                             nsISupportsArray/*<nsIRDFResource>*/* aSources,
                             nsIRDFResource*   aCommand,
                             nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                             PRBool* aResult)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsCharsetMenu::DoCommand(nsISupportsArray* aSources,
                                       nsIRDFResource*   aCommand,
                                       nsISupportsArray* aArguments)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

