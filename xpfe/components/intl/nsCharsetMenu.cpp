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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#include "nsStringEnumerator.h"
#include "nsVoidArray.h"
#include "nsIObserverService.h"
#include "nsIRequestObserver.h"
#include "nsITimelineService.h"
#include "nsCRT.h"
#include "prmem.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kRDFInMemoryDataSourceCID, NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID, NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFContainerCID, NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

static const char kURINC_BrowserAutodetMenuRoot[] = "NC:BrowserAutodetMenuRoot";
static const char kURINC_BrowserCharsetMenuRoot[] = "NC:BrowserCharsetMenuRoot";
static const char kURINC_BrowserMoreCharsetMenuRoot[] = "NC:BrowserMoreCharsetMenuRoot";
static const char kURINC_BrowserMore1CharsetMenuRoot[] = "NC:BrowserMore1CharsetMenuRoot";
static const char kURINC_BrowserMore2CharsetMenuRoot[] = "NC:BrowserMore2CharsetMenuRoot";
static const char kURINC_BrowserMore3CharsetMenuRoot[] = "NC:BrowserMore3CharsetMenuRoot";
static const char kURINC_BrowserMore4CharsetMenuRoot[] = "NC:BrowserMore4CharsetMenuRoot";
static const char kURINC_BrowserMore5CharsetMenuRoot[] = "NC:BrowserMore5CharsetMenuRoot";
static const char kURINC_BrowserUnicodeCharsetMenuRoot[] = "NC:BrowserUnicodeCharsetMenuRoot";
static const char kURINC_MaileditCharsetMenuRoot[] = "NC:MaileditCharsetMenuRoot";
static const char kURINC_MailviewCharsetMenuRoot[] = "NC:MailviewCharsetMenuRoot";
static const char kURINC_ComposerCharsetMenuRoot[] = "NC:ComposerCharsetMenuRoot";
static const char kURINC_DecodersRoot[] = "NC:DecodersRoot";
static const char kURINC_EncodersRoot[] = "NC:EncodersRoot";
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

static void CloneCStringArray(const nsCStringArray& src, nsCStringArray& dest)
{
  PRUint32 count = src.Count();
  PRUint32 i;
  for (i=0; i<count; i++) {
    dest.AppendCString(*src.CStringAt(i));
  }
}

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

  nsCAutoString mCharset;
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
  static nsIRDFResource * kNC_BrowserUnicodeCharsetMenuRoot;
  static nsIRDFResource * kNC_MaileditCharsetMenuRoot;
  static nsIRDFResource * kNC_MailviewCharsetMenuRoot;
  static nsIRDFResource * kNC_ComposerCharsetMenuRoot;
  static nsIRDFResource * kNC_DecodersRoot;
  static nsIRDFResource * kNC_EncodersRoot;
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
  nsCOMPtr<nsICharsetConverterManager> mCCManager;
  nsCOMPtr<nsIPrefBranch>               mPrefs;
  nsCOMPtr<nsIObserver>                 mCharsetMenuObserver;
  nsCStringArray                        mDecoderList;

  nsresult Done();
  nsresult SetCharsetCheckmark(nsString * aCharset, PRBool aValue);

  nsresult FreeResources();

  nsresult InitStaticMenu(nsCStringArray& aDecs, 
                          nsIRDFResource * aResource,
                          const char * aKey,
                          nsVoidArray * aArray);
  nsresult InitCacheMenu(nsCStringArray& aDecs,
                         nsIRDFResource * aResource,
                         const char * aKey,
                         nsVoidArray * aArray);
  
  nsresult InitMoreMenu(nsCStringArray& aDecs,
                        nsIRDFResource * aResource, 
                        const char * aFlag);
  
  nsresult InitMoreSubmenus(nsCStringArray& aDecs);

  static nsresult SetArrayFromEnumerator(nsIUTF8StringEnumerator* aEnumerator,
                                         nsCStringArray& aArray);
  
  nsresult AddCharsetToItemArray(nsVoidArray* aArray,
                                 const nsAFlatCString& aCharset, 
                                 nsMenuEntry ** aResult,
                                 PRInt32 aPlace);
  nsresult AddCharsetArrayToItemArray(nsVoidArray &aArray, 
                                      const nsCStringArray& aCharsets);
  nsresult AddMenuItemToContainer(nsIRDFContainer * aContainer, 
    nsMenuEntry * aItem, nsIRDFResource * aType, const char * aIDPrefix, 
    PRInt32 aPlace);
  nsresult AddMenuItemArrayToContainer(nsIRDFContainer * aContainer, 
    nsVoidArray * aArray, nsIRDFResource * aType);
  nsresult AddCharsetToContainer(nsVoidArray * aArray, 
                                 nsIRDFContainer * aContainer,
                                 const nsAFlatCString& aCharset,
                                 const char * aIDPrefix, 
    PRInt32 aPlace, PRInt32 aRDFPlace);

  nsresult AddFromPrefsToMenu(nsVoidArray * aArray, 
                              nsIRDFContainer * aContainer,
                              const char * aKey,
                              nsCStringArray& aDecs,
                              const char * aIDPrefix);
  nsresult AddFromNolocPrefsToMenu(nsVoidArray * aArray, 
                                   nsIRDFContainer * aContainer,
                                   const char * aKey,
                                   nsCStringArray& aDecs,
                                   const char * aIDPrefix);
  nsresult AddFromStringToMenu(char * aCharsetList,
                               nsVoidArray * aArray,
                               nsIRDFContainer * aContainer,
                               nsCStringArray& aDecs,
                               const char * aIDPrefix);

  nsresult AddSeparatorToContainer(nsIRDFContainer * aContainer);
  nsresult AddCharsetToCache(const nsAFlatCString& aCharset,
                             nsVoidArray * aArray, 
                             nsIRDFResource * aRDFResource,
                             PRInt32 aCacheStart, PRInt32 aCacheSize, 
                             PRInt32 aRDFPlace);

  nsresult WriteCacheToPrefs(nsVoidArray * aArray, PRInt32 aCacheStart, 
    const char * aKey);
  nsresult UpdateCachePrefs(const char * aCacheKey, const char * aCacheSizeKey, 
    const char * aStaticKey, const PRUnichar * aCharset);

  nsresult ClearMenu(nsIRDFContainer * aContainer, nsVoidArray * aArray);
  nsresult RemoveLastMenuItem(nsIRDFContainer * aContainer, 
                              nsVoidArray * aArray);

  nsresult RemoveFlaggedCharsets(nsCStringArray& aList, nsString * aProp);
  nsresult NewRDFContainer(nsIRDFDataSource * aDataSource, 
    nsIRDFResource * aResource, nsIRDFContainer ** aResult);
  void FreeMenuItemArray(nsVoidArray * aArray);
  PRInt32 FindMenuItemInArray(const nsVoidArray* aArray,
                              const nsAFlatCString& aCharset, 
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
  nsresult RefreshBrowserMenu();
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

nsresult
nsCharsetMenu::SetArrayFromEnumerator(nsIUTF8StringEnumerator* aEnumerator,
                                      nsCStringArray& aArray)
{
  nsresult rv;
  
  PRBool hasMore;
  rv = aEnumerator->HasMore(&hasMore);
  
  nsCAutoString value;
  while (NS_SUCCEEDED(rv) && hasMore) {
    rv = aEnumerator->GetNext(value);
    if (NS_SUCCEEDED(rv))
      aArray.AppendCString(value);

    rv = aEnumerator->HasMore(&hasMore);
  }

  return rv;
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
  }

  virtual ~nsCharsetMenuObserver() {}

private:
  nsCharsetMenu* mCharsetMenu;
};

NS_IMPL_ISUPPORTS1(nsCharsetMenuObserver, nsIObserver)

NS_IMETHODIMP nsCharsetMenuObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu:Observe");
  nsresult rv = NS_OK;
 
  //XUL event handler
  if (!nsCRT::strcmp(aTopic, "charsetmenu-selected")) {
    nsDependentString nodeName(someData);
    rv = mCharsetMenu->Init();
    if (nodeName.EqualsLiteral("browser")) {
      rv = mCharsetMenu->InitBrowserMenu();
    }
    if (nodeName.EqualsLiteral("composer")) {
      rv = mCharsetMenu->InitComposerMenu();
    }
    if (nodeName.EqualsLiteral("mailview")) {
      rv = mCharsetMenu->InitMailviewMenu();
    }
    if (nodeName.EqualsLiteral("mailedit")) {
      rv = mCharsetMenu->InitMaileditMenu();
      rv = mCharsetMenu->InitOthers();
    }
    if (nodeName.EqualsLiteral("more-menu")) {
      rv = mCharsetMenu->InitSecondaryTiers();
      rv = mCharsetMenu->InitAutodetMenu();
    }
    if (nodeName.EqualsLiteral("other")) {
      rv = mCharsetMenu->InitOthers();
      rv = mCharsetMenu->InitMaileditMenu();
    }
  }
   
   //pref event handler
  if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsDependentString prefName(someData);

    if (prefName.EqualsLiteral(kBrowserStaticPrefKey)) {
      // refresh menus which share this pref
      rv = mCharsetMenu->RefreshBrowserMenu();
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mCharsetMenu->RefreshMailviewMenu();
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mCharsetMenu->RefreshComposerMenu();
    }
    else if (prefName.EqualsLiteral(kMaileditPrefKey)) {
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
nsIRDFResource * nsCharsetMenu::kNC_BrowserUnicodeCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_MaileditCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_MailviewCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_ComposerCharsetMenuRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_DecodersRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_EncodersRoot = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Name = NULL;
nsIRDFResource * nsCharsetMenu::kNC_Checked = NULL;
nsIRDFResource * nsCharsetMenu::kNC_CharsetDetector = NULL;
nsIRDFResource * nsCharsetMenu::kNC_BookmarkSeparator = NULL;
nsIRDFResource * nsCharsetMenu::kRDF_type = NULL;

nsCharsetMenu::nsCharsetMenu() 
: mInitialized(PR_FALSE), 
  mBrowserMenuInitialized(PR_FALSE),
  mMailviewMenuInitialized(PR_FALSE),
  mComposerMenuInitialized(PR_FALSE),
  mMaileditMenuInitialized(PR_FALSE),
  mSecondaryTiersInitialized(PR_FALSE),
  mAutoDetectInitialized(PR_FALSE),
  mOthersInitialized(PR_FALSE)
{
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

    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserCharsetMenuRoot),
                             &kNC_BrowserCharsetMenuRoot);
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

nsresult nsCharsetMenu::RefreshBrowserMenu()
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIRDFContainer> container;
  res = NewRDFContainer(mInner, kNC_BrowserCharsetMenuRoot, getter_AddRefs(container));
  if (NS_FAILED(res)) return res;

  // clean the menu
  res = ClearMenu(container, &mBrowserMenu);
  if (NS_FAILED(res)) return res;

  // rebuild the menu
  nsCOMPtr<nsIUTF8StringEnumerator> decoders;
  res = mCCManager->GetDecoderList(getter_AddRefs(decoders));
  if (NS_FAILED(res)) return res;

  nsCStringArray decs;
  SetArrayFromEnumerator(decoders, decs);
  
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

  nsCOMPtr<nsIUTF8StringEnumerator> decoders;
  res = mCCManager->GetDecoderList(getter_AddRefs(decoders));
  if (NS_FAILED(res)) return res;

  nsCStringArray decs;
  SetArrayFromEnumerator(decoders, decs);
  
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
  nsCOMPtr<nsIUTF8StringEnumerator> encoders;
  res = mCCManager->GetEncoderList(getter_AddRefs(encoders));
  NS_ENSURE_SUCCESS(res, res);

  nsCStringArray encs;
  SetArrayFromEnumerator(encoders, encs);
  
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

  nsCOMPtr<nsIUTF8StringEnumerator> decoders;
  res = mCCManager->GetDecoderList(getter_AddRefs(decoders));
  if (NS_FAILED(res)) return res;

  nsCStringArray decs;
  SetArrayFromEnumerator(decoders, decs);
  
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
    nsCOMPtr<nsIUTF8StringEnumerator> decoders;
    res = mCCManager->GetDecoderList(getter_AddRefs(decoders));
    if (NS_FAILED(res)) return res;

    SetArrayFromEnumerator(decoders, mDecoderList);

    //initialize all remaining RDF template nodes
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserAutodetMenuRoot),
                             &kNC_BrowserAutodetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserMoreCharsetMenuRoot),
                             &kNC_BrowserMoreCharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserMore1CharsetMenuRoot),
                             &kNC_BrowserMore1CharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserMore2CharsetMenuRoot),
                             &kNC_BrowserMore2CharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserMore3CharsetMenuRoot),
                             &kNC_BrowserMore3CharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserMore4CharsetMenuRoot),
                             &kNC_BrowserMore4CharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserMore5CharsetMenuRoot),
                             &kNC_BrowserMore5CharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BrowserUnicodeCharsetMenuRoot),
                             &kNC_BrowserUnicodeCharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_MaileditCharsetMenuRoot),
                             &kNC_MaileditCharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_MailviewCharsetMenuRoot),
                             &kNC_MailviewCharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_ComposerCharsetMenuRoot),
                             &kNC_ComposerCharsetMenuRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_DecodersRoot),
                             &kNC_DecodersRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_EncodersRoot),
                             &kNC_EncodersRoot);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_Name),
                             &kNC_Name);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_Checked),
                             &kNC_Checked);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_CharsetDetector),
                             &kNC_CharsetDetector);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_BookmarkSeparator),
                             &kNC_BookmarkSeparator);
    mRDFService->GetResource(NS_LITERAL_CSTRING(kURINC_type), &kRDF_type);

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
    res = rdfUtil->MakeSeq(mInner, kNC_BrowserUnicodeCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_MaileditCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_MailviewCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_ComposerCharsetMenuRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_DecodersRoot, NULL);
    if (NS_FAILED(res)) goto done;
    res = rdfUtil->MakeSeq(mInner, kNC_EncodersRoot, NULL);
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
  NS_IF_RELEASE(kNC_BrowserUnicodeCharsetMenuRoot);
  NS_IF_RELEASE(kNC_MaileditCharsetMenuRoot);
  NS_IF_RELEASE(kNC_MailviewCharsetMenuRoot);
  NS_IF_RELEASE(kNC_ComposerCharsetMenuRoot);
  NS_IF_RELEASE(kNC_DecodersRoot);
  NS_IF_RELEASE(kNC_EncodersRoot);
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
  res = mRDFService->GetUnicodeResource(*aCharset, getter_AddRefs(node));
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
    /* nsIObserverService has to have released nsCharsetMenu already */
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


    // how to clone mDecoderList??
    nsCStringArray browserDecoderList;
    CloneCStringArray(mDecoderList, browserDecoderList);

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
    // this would bring in a whole bunch of 'font encoders' as well as genuine 
    // charset encoders, but it's safe because we rely on prefs to filter
    // them out. Moreover, 'customize' menu lists only genuine charset 
    // encoders further guarding  against  'font encoders' sneaking in. 
    nsCOMPtr<nsIUTF8StringEnumerator> encoders;
    res = mCCManager->GetEncoderList(getter_AddRefs(encoders));
    if (NS_FAILED(res))  return res;

    nsCStringArray maileditEncoderList;
    SetArrayFromEnumerator(encoders, maileditEncoderList);
  
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

    nsCStringArray mailviewDecoderList;
    CloneCStringArray(mDecoderList, mailviewDecoderList);

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

    nsCStringArray composerDecoderList;
    CloneCStringArray(mDecoderList, composerDecoderList);

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
    nsCStringArray othersDecoderList;
    CloneCStringArray(mDecoderList, othersDecoderList);

    res = InitMoreMenu(othersDecoderList, kNC_DecodersRoot, ".notForBrowser");                 
    if (NS_FAILED(res))  return res;

    // Using mDecoderList instead of GetEncoderList(), we can avoid having to
    // tag a whole bunch of 'font encoders' with '.notForOutgoing' in 
    // charsetData.properties file. 
    nsCStringArray othersEncoderList;
    CloneCStringArray(mDecoderList, othersEncoderList);

    res = InitMoreMenu(othersEncoderList, kNC_EncodersRoot, ".notForOutgoing");                 
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
    nsCStringArray secondaryTiersDecoderList;
    CloneCStringArray(mDecoderList, secondaryTiersDecoderList);

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

nsresult nsCharsetMenu::InitStaticMenu(nsCStringArray& aDecs,
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
                        nsCStringArray& aDecs,
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
    nsCStringArray detectorArray;

    res = NewRDFContainer(mInner, kNC_BrowserAutodetMenuRoot, getter_AddRefs(container));
    if (NS_FAILED(res)) return res;

    nsCOMPtr<nsIUTF8StringEnumerator> detectors;
    res = mCCManager->GetCharsetDetectorList(getter_AddRefs(detectors));
    if (NS_FAILED(res)) goto done;

    res = SetArrayFromEnumerator(detectors, detectorArray);
    if (NS_FAILED(res)) goto done;
    
    res = AddCharsetArrayToItemArray(chardetArray, detectorArray);
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

nsresult nsCharsetMenu::InitMoreMenu(nsCStringArray& aDecs, 
                                     nsIRDFResource * aResource, 
                                     const char * aFlag)
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

  res = AddCharsetArrayToItemArray(moreMenu, aDecs);
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
nsresult nsCharsetMenu::InitMoreSubmenus(nsCStringArray& aDecs)
{
  NS_TIMELINE_START_TIMER("nsCharsetMenu::InitMoreSubmenus");

  nsresult res = NS_OK;

  nsCOMPtr<nsIRDFContainer> container1;
  nsCOMPtr<nsIRDFContainer> container2;
  nsCOMPtr<nsIRDFContainer> container3;
  nsCOMPtr<nsIRDFContainer> container4;
  nsCOMPtr<nsIRDFContainer> container5;
  nsCOMPtr<nsIRDFContainer> containerU;
  const char key1[] = "intl.charsetmenu.browser.more1";
  const char key2[] = "intl.charsetmenu.browser.more2";
  const char key3[] = "intl.charsetmenu.browser.more3";
  const char key4[] = "intl.charsetmenu.browser.more4";
  const char key5[] = "intl.charsetmenu.browser.more5";
  const char keyU[] = "intl.charsetmenu.browser.unicode";

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

  res = NewRDFContainer(mInner, kNC_BrowserUnicodeCharsetMenuRoot, 
    getter_AddRefs(containerU));
  if (NS_FAILED(res)) return res;
  AddFromPrefsToMenu(NULL, containerU, keyU, aDecs, NULL);

  NS_TIMELINE_STOP_TIMER("nsCharsetMenu::InitMoreSubmenus");
  NS_TIMELINE_MARK_TIMER("nsCharsetMenu::InitMoreSubmenus");

  return res;
}

nsresult nsCharsetMenu::AddCharsetToItemArray(nsVoidArray *aArray, 
                                              const nsAFlatCString& aCharset, 
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

  res = mCCManager->GetCharsetTitle(aCharset.get(), item->mTitle);
  if (NS_FAILED(res)) {
    item->mTitle.AssignWithConversion(aCharset.get());
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

nsresult
nsCharsetMenu::AddCharsetArrayToItemArray(nsVoidArray& aArray, 
                                          const nsCStringArray& aCharsets) 
{
  PRUint32 count = aCharsets.Count();

  for (PRUint32 i = 0; i < count; i++) {

    nsCString* str = aCharsets.CStringAt(i);
    if (str) {
      nsresult res = AddCharsetToItemArray(&aArray, *str, NULL, -1);
    
      if (NS_FAILED(res)) return res;
    }
  }

  return NS_OK;
}

// aPlace < -1 for Remove
// aPlace < 0 for Append
nsresult nsCharsetMenu::AddMenuItemToContainer(
                        nsIRDFContainer * aContainer,
                        nsMenuEntry * aItem,
                        nsIRDFResource * aType,
                        const char * aIDPrefix,
                        PRInt32 aPlace) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIRDFResource> node;

  nsCAutoString id;
  if (aIDPrefix != NULL) id.Assign(aIDPrefix);
  id.Append(aItem->mCharset);

  // Make up a unique ID and create the RDF NODE
  res = mRDFService->GetResource(id, getter_AddRefs(node));
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

nsresult nsCharsetMenu::AddCharsetToContainer(nsVoidArray *aArray, 
                                              nsIRDFContainer * aContainer, 
                                              const nsAFlatCString& aCharset, 
                                              const char * aIDPrefix,
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
                        nsCStringArray& aDecs, 
                        const char * aIDPrefix)
{
  nsresult res = NS_OK;

  nsCOMPtr<nsIPrefLocalizedString> pls;
  res = mPrefs->GetComplexValue(aKey, NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(pls));
  if (NS_FAILED(res)) return res;

  if (pls) {
    nsXPIDLString ucsval;
    pls->ToString(getter_Copies(ucsval));
    NS_ConvertUCS2toUTF8 utf8val(ucsval);
    if (ucsval)
      res = AddFromStringToMenu(utf8val.BeginWriting(), aArray,
                                aContainer, aDecs, aIDPrefix);
  }

  return res;
}

nsresult
nsCharsetMenu::AddFromNolocPrefsToMenu(nsVoidArray * aArray, 
                                       nsIRDFContainer * aContainer, 
                                       const char * aKey, 
                                       nsCStringArray& aDecs, 
                                       const char * aIDPrefix)
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
                        nsCStringArray& aDecs, 
                        const char * aIDPrefix)
{
  nsresult res = NS_OK;
  char * p = aCharsetList;
  char * q = p;
  while (*p != 0) {
	  for (; (*q != ',') && (*q != ' ') && (*q != 0); q++) {;}
    char temp = *q;
    *q = 0;

    // if this charset is not on the accepted list of charsets, ignore it
    PRInt32 index;
    index = aDecs.IndexOfIgnoreCase(nsCAutoString(p));
    if (index >= 0) {

      // else, add it to the menu
      res = AddCharsetToContainer(aArray, aContainer, nsDependentCString(p),
                                  aIDPrefix, -1, 0);
      NS_ASSERTION(NS_SUCCEEDED(res), "cannot add charset to menu");
      if (NS_FAILED(res)) break;

      res = aDecs.RemoveCStringAt(index);
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
  nsCAutoString str;
  str.AssignLiteral("----");

  // hack to generate unique id's for separators
  static PRInt32 u = 0;
  u++;
  str.AppendInt(u);

  nsMenuEntry item;
  item.mCharset = str;
  item.mTitle.AssignWithConversion(str.get());

  return AddMenuItemToContainer(aContainer, &item, kNC_BookmarkSeparator, 
    NULL, -1);
}

nsresult
nsCharsetMenu::AddCharsetToCache(const nsAFlatCString& aCharset,
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
  nsCAutoString cache;
  nsCAutoString sep(NS_LITERAL_CSTRING(", "));
  PRInt32 count = aArray->Count();

  for (PRInt32 i = aCacheStart; i < count; i++) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(i);
    if (item != NULL) {    
      cache.Append(item->mCharset);
      if (i < count - 1) {
        cache.Append(sep);
      }
    }
  }

  // write the pref
  res = mPrefs->SetCharPref(aKey, cache.get());

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
  NS_ConvertUCS2toUTF8 currentCharset(aCharset);
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
  if (last >= 0) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(last);
    if (item != NULL) {    
      res = AddMenuItemToContainer(aContainer, item, NULL, "charset.", -2);
      if (NS_FAILED(res)) return res;

      res = aArray->RemoveElementAt(last);
      if (NS_FAILED(res)) return res;
    }
  }

  return res;
}

nsresult nsCharsetMenu::RemoveFlaggedCharsets(nsCStringArray& aList, 
                                              nsString * aProp)
{
  nsresult res = NS_OK;
  PRUint32 count;

  count = aList.Count();
  if (NS_FAILED(res)) return res;

  nsCString* charset;
  nsAutoString str;
  for (PRUint32 i = 0; i < count; i++) {

    charset = aList.CStringAt(i);
    if (!charset) continue;

    res = mCCManager->GetCharsetData(charset->get(), aProp->get(), str);
    if (NS_FAILED(res)) continue;

    aList.RemoveCStringAt(i);

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

PRInt32 nsCharsetMenu::FindMenuItemInArray(const nsVoidArray* aArray, 
                                           const nsAFlatCString& aCharset, 
                                           nsMenuEntry ** aResult)
{
  PRUint32 count = aArray->Count();

  for (PRUint32 i=0; i < count; i++) {
    nsMenuEntry * item = (nsMenuEntry *) aArray->ElementAt(i);
    if (item->mCharset == aCharset) {
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

    res = collation->AllocateRawSortKey(kCollationCaseInSensitive, 
                                       (array[i].item)->mTitle, &array[i].key, &array[i].len);
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
    PR_FREEIF(array[i].key);
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
  nsresult res = NS_OK;

  if (mBrowserMenuInitialized) {
    res = AddCharsetToCache(NS_LossyConvertUCS2toASCII(aCharset),
                            &mBrowserMenu, kNC_BrowserCharsetMenuRoot, 
                            mBrowserCacheStart, mBrowserCacheSize,
                            mBrowserMenuRDFPosition);
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
  nsresult res = NS_OK;

  if (mMailviewMenuInitialized) {
    res = AddCharsetToCache(NS_LossyConvertUCS2toASCII(aCharset),
                            &mMailviewMenu, kNC_MailviewCharsetMenuRoot, 
                            mMailviewCacheStart, mMailviewCacheSize,
                            mMailviewMenuRDFPosition);
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
  nsresult res = NS_OK;

  if (mComposerMenuInitialized) {

    res = AddCharsetToCache(NS_LossyConvertUCS2toASCII(aCharset),
                            &mComposerMenu, kNC_ComposerCharsetMenuRoot, 
                            mComposerCacheStart, mComposerCacheSize,
                            mComposerMenuRDFPosition);
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

NS_IMETHODIMP nsCharsetMenu::BeginUpdateBatch()
{
  return mInner->BeginUpdateBatch();
}

NS_IMETHODIMP nsCharsetMenu::EndUpdateBatch()
{
  return mInner->EndUpdateBatch();
}
