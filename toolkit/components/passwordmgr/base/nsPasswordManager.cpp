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
 * The Original Code is Mozilla Password Manager.
 *
 * The Initial Developer of the Original Code is
 * Brian Ryner.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsPasswordManager.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsILineInputStream.h"
#include "plbase64.h"
#include "nsISecretDecoderRing.h"
#include "nsIPassword.h"
#include "nsIPrompt.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "prmem.h"
#include "nsIStringBundle.h"
#include "nsArray.h"
#include "nsICategoryManager.h"
#include "nsIObserverService.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIForm.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIContent.h"
#include "nsIFormControl.h"
#include "nsIDOMWindowInternal.h"
#include "nsCURILoader.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIAutoCompleteResult.h"
#include "nsIPK11TokenDB.h"
#include "nsIPK11Token.h"

static const char kPMPropertiesURL[] = "chrome://passwordmgr/locale/passwordmgr.properties";
static PRBool sRememberPasswords = PR_FALSE;
static PRBool sPrefsInitialized = PR_FALSE;

static nsIStringBundle* sPMBundle;
static nsISecretDecoderRing* sDecoderRing;
static nsPasswordManager* sPasswordManager;

class nsPasswordManager::SignonDataEntry
{
public:
  nsString userField;
  nsString userValue;
  nsString passField;
  nsString passValue;
  SignonDataEntry* next;

  SignonDataEntry() : next(nsnull) { }
  ~SignonDataEntry() { delete next; }
};

class nsPasswordManager::SignonHashEntry
{
  // Wraps a pointer to the linked list of SignonDataEntry objects.
  // This allows us to adjust the head of the linked list without a
  // hashtable operation.

public:
  SignonDataEntry* head;

  SignonHashEntry(SignonDataEntry* aEntry) : head(aEntry) { }
  ~SignonHashEntry() { delete head; }
};

class nsPasswordManager::PasswordEntry : public nsIPassword
{
public:
  PasswordEntry(const nsACString& aKey, SignonDataEntry* aData);
  virtual ~PasswordEntry() { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPASSWORD

protected:

  nsCString mHost;
  nsString  mUser;
  nsString  mPassword;
  PRBool    mDecrypted[2];
};

NS_IMPL_ISUPPORTS1(nsPasswordManager::PasswordEntry, nsIPassword)

nsPasswordManager::PasswordEntry::PasswordEntry(const nsACString& aKey,
                                                SignonDataEntry* aData)
  : mHost(aKey)
{
    mDecrypted[0] = mDecrypted[1] = PR_FALSE;

  if (aData) {
    mUser.Assign(aData->userValue);
    mPassword.Assign(aData->passValue);
  }
}

NS_IMETHODIMP
nsPasswordManager::PasswordEntry::GetHost(nsACString& aHost)
{
  aHost.Assign(mHost);
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::PasswordEntry::GetUser(nsAString& aUser)
{
  if (!mUser.IsEmpty() && !mDecrypted[0]) {
    if (NS_SUCCEEDED(DecryptData(mUser, mUser)))
      mDecrypted[0] = PR_TRUE;
    else
      return NS_ERROR_FAILURE;
  }

  aUser.Assign(mUser);
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::PasswordEntry::GetPassword(nsAString& aPassword)
{
  if (!mPassword.IsEmpty() && !mDecrypted[1]) {
    if (NS_SUCCEEDED(DecryptData(mPassword, mPassword)))
      mDecrypted[1] = PR_TRUE;
    else
      return NS_ERROR_FAILURE;
  }

  aPassword.Assign(mPassword);
  return NS_OK;
}





NS_IMPL_ADDREF(nsPasswordManager)
NS_IMPL_RELEASE(nsPasswordManager)

NS_INTERFACE_MAP_BEGIN(nsPasswordManager)
  NS_INTERFACE_MAP_ENTRY(nsIPasswordManager)
  NS_INTERFACE_MAP_ENTRY(nsIPasswordManagerInternal)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIFormSubmitObserver)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPasswordManager)
NS_INTERFACE_MAP_END

nsPasswordManager::nsPasswordManager()
  : mAutoCompletingField(nsnull)
{
}

nsPasswordManager::~nsPasswordManager()
{
}


/* static */ nsPasswordManager*
nsPasswordManager::GetInstance()
{
  if (!sPasswordManager) {
    sPasswordManager = new nsPasswordManager();
    if (!sPasswordManager)
      return nsnull;

    NS_ADDREF(sPasswordManager);   // addref the global

    if (NS_FAILED(sPasswordManager->Init())) {
      NS_RELEASE(sPasswordManager);
      return nsnull;
    }
  }

  NS_ADDREF(sPasswordManager);   // addref the return result
  return sPasswordManager;
}

nsresult
nsPasswordManager::Init()
{
  mSignonTable.Init();
  mRejectTable.Init();
  mAutoCompleteInputs.Init();

  sPrefsInitialized = PR_TRUE;

  nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ASSERTION(prefService, "No pref service");

  prefService->GetBranch("signon.", getter_AddRefs(mPrefBranch));
  NS_ASSERTION(mPrefBranch, "No pref branch");

  mPrefBranch->GetBoolPref("rememberSignons", &sRememberPasswords);

  nsCOMPtr<nsIPrefBranchInternal> branchInternal = do_QueryInterface(mPrefBranch);

  // Have the pref service hold a weak reference; the service manager
  // will be holding a strong reference.

  branchInternal->AddObserver("rememberSignons", this, PR_TRUE);


  // Be a form submit and web progress observer so that we can save and
  // prefill passwords.

  nsCOMPtr<nsIObserverService> obsService = do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(obsService, "No observer service");

  obsService->AddObserver(this, NS_FORMSUBMIT_SUBJECT, PR_TRUE);

  nsCOMPtr<nsIDocumentLoader> docLoaderService = do_GetService(NS_DOCUMENTLOADER_SERVICE_CONTRACTID);
  NS_ASSERTION(docLoaderService, "No document loader service");

  nsCOMPtr<nsIWebProgress> progress = do_QueryInterface(docLoaderService);
  NS_ASSERTION(progress, "docloader service does not implement nsIWebProgress");

  progress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);

  // Now read in the signon file
  nsXPIDLCString signonFile;
  mPrefBranch->GetCharPref("SignonFileName", getter_Copies(signonFile));
  NS_ASSERTION(!signonFile.IsEmpty(), "Fallback for signon filename not present");

  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mSignonFile));
  mSignonFile->AppendNative(signonFile);

  nsCAutoString path;
  mSignonFile->GetNativePath(path);

  ReadPasswords(mSignonFile);

  return NS_OK;
}

/* static */ PRBool
nsPasswordManager::SingleSignonEnabled()
{
  if (!sPrefsInitialized) {
    // Create the PasswordManager service to initialize the prefs and callback
    nsCOMPtr<nsIPasswordManager> manager = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
  }

  return sRememberPasswords;
}

/* static */ NS_METHOD
nsPasswordManager::Register(nsIComponentManager* aCompMgr,
                            nsIFile* aPath,
                            const char* aRegistryLocation,
                            const char* aComponentType,
                            const nsModuleComponentInfo* aInfo)
{
  // By registering in NS_PASSWORDMANAGER_CATEGORY, an instance of the password
  // manager will be created when a password input is added to a form.  We
  // can then register that singleton instance as a form submission observer.

  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXPIDLCString prevEntry;
  catman->AddCategoryEntry(NS_PASSWORDMANAGER_CATEGORY,
                           "Password Manager",
                           NS_PASSWORDMANAGER_CONTRACTID,
                           PR_TRUE,
                           PR_TRUE,
                           getter_Copies(prevEntry));

  return NS_OK;
}

/* static */ NS_METHOD
nsPasswordManager::Unregister(nsIComponentManager* aCompMgr,
                              nsIFile* aPath,
                              const char* aRegistryLocation,
                              const nsModuleComponentInfo* aInfo)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  catman->DeleteCategoryEntry(NS_PASSWORDMANAGER_CATEGORY,
                              NS_PASSWORDMANAGER_CONTRACTID,
                              PR_TRUE);

  return NS_OK;
}

/* static */ void
nsPasswordManager::Shutdown()
{
  NS_IF_RELEASE(sDecoderRing);
  NS_IF_RELEASE(sPMBundle);
  NS_IF_RELEASE(sPasswordManager);
}

// nsIPasswordManager implementation

NS_IMETHODIMP
nsPasswordManager::AddUser(const nsACString& aHost,
                           const nsAString& aUser,
                           const nsAString& aPassword)
{
  // Silently ignore an empty username/password entry.
  // There's no point in taking up space in the signon file with this.
  if (aUser.IsEmpty() && aPassword.IsEmpty())
    return NS_OK;

  // Check for an existing entry for this host + user
  if (!aHost.IsEmpty()) {
    SignonHashEntry *hashEnt;
    if (mSignonTable.Get(aHost, &hashEnt)) {
      nsString empty;
      SignonDataEntry *entry = nsnull;
      FindPasswordEntryInternal(hashEnt->head, aUser, empty, empty, &entry);
      if (entry) {
        // Just change the password
        return EncryptDataUCS2(aPassword, entry->passValue);
      }
    }
  }

  SignonDataEntry* entry = new SignonDataEntry();
  if (NS_FAILED(EncryptDataUCS2(aUser, entry->userValue)) ||
      NS_FAILED(EncryptDataUCS2(aPassword, entry->passValue))) {
    delete entry;
    return NS_ERROR_FAILURE;
  }

  AddSignonData(aHost, entry);
  WritePasswords(mSignonFile);

  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::RemoveUser(const nsACString& aHost, const nsAString& aUser)
{
  SignonDataEntry* entry, *prevEntry = nsnull;
  SignonHashEntry* hashEnt;

  if (!mSignonTable.Get(aHost, &hashEnt))
    return NS_ERROR_FAILURE;

  for (entry = hashEnt->head; entry; prevEntry = entry, entry = entry->next) {

    nsAutoString ptUser;
    if (!entry->userValue.IsEmpty() &&
        NS_FAILED(DecryptData(entry->userValue, ptUser)))
      break;

    if (ptUser.Equals(aUser)) {
      if (prevEntry)
        prevEntry->next = entry->next;
      else
        hashEnt->head = entry->next;

      entry->next = nsnull;
      delete entry;

      if (!hashEnt->head)
        mSignonTable.Remove(aHost);  // deletes hashEnt

      WritePasswords(mSignonFile);

      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPasswordManager::AddReject(const nsACString& aHost)
{
  mRejectTable.Put(aHost, 1);
  WritePasswords(mSignonFile);
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::RemoveReject(const nsACString& aHost)
{
  mRejectTable.Remove(aHost);
  WritePasswords(mSignonFile);
  return NS_OK;
}

/* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::BuildArrayEnumerator(const nsACString& aKey,
                                        SignonHashEntry* aEntry,
                                        void* aUserData)
{
  nsIMutableArray* array = NS_STATIC_CAST(nsIMutableArray*, aUserData);

  for (SignonDataEntry* e = aEntry->head; e; e = e->next)
    array->AppendElement(new PasswordEntry(aKey, e), PR_FALSE);

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsPasswordManager::GetEnumerator(nsISimpleEnumerator** aEnumerator)
{
  // Build an array out of the hashtable
  nsCOMPtr<nsIMutableArray> signonArray;
  NS_NewArray(getter_AddRefs(signonArray));

  mSignonTable.EnumerateRead(BuildArrayEnumerator, signonArray);

  return signonArray->Enumerate(aEnumerator);
}

/* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::BuildRejectArrayEnumerator(const nsACString& aKey,
                                              PRInt32 aEntry,
                                              void* aUserData)
{
  nsIMutableArray* array = NS_STATIC_CAST(nsIMutableArray*, aUserData);

  nsCOMPtr<nsIPassword> passwordEntry = new PasswordEntry(aKey, nsnull);
  array->AppendElement(passwordEntry, PR_FALSE);

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsPasswordManager::GetRejectEnumerator(nsISimpleEnumerator** aEnumerator)
{
  // Build an array out of the hashtable
  nsCOMPtr<nsIMutableArray> rejectArray;
  NS_NewArray(getter_AddRefs(rejectArray));

  mRejectTable.EnumerateRead(BuildRejectArrayEnumerator, rejectArray);

  return rejectArray->Enumerate(aEnumerator);
}

// nsIPasswordManagerInternal implementation

struct findEntryContext {
  nsPasswordManager* manager;
  const nsACString& hostURI;
  const nsAString&  username;
  const nsAString&  password;
  nsACString& hostURIFound;
  nsAString&  usernameFound;
  nsAString&  passwordFound;
  PRBool matched;

  findEntryContext(nsPasswordManager* aManager,
                   const nsACString& aHostURI,
                   const nsAString&  aUsername,
                   const nsAString&  aPassword,
                   nsACString& aHostURIFound,
                   nsAString&  aUsernameFound,
                   nsAString&  aPasswordFound)
    : manager(aManager), hostURI(aHostURI), username(aUsername),
      password(aPassword), hostURIFound(aHostURIFound),
      usernameFound(aUsernameFound), passwordFound(aPasswordFound),
      matched(PR_FALSE) { }
};

/* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::FindEntryEnumerator(const nsACString& aKey,
                                       SignonHashEntry* aEntry,
                                       void* aUserData)
{
  findEntryContext* context = NS_STATIC_CAST(findEntryContext*, aUserData);
  nsPasswordManager* manager = context->manager;
  nsresult rv;

  SignonDataEntry* entry = nsnull;
  rv = manager->FindPasswordEntryInternal(aEntry->head,
                                          context->username,
                                          context->password,
                                          EmptyString(),
                                          &entry);

  if (NS_SUCCEEDED(rv) && entry) {
    if (NS_SUCCEEDED(DecryptData(entry->userValue, context->usernameFound)) &&
        NS_SUCCEEDED(DecryptData(entry->passValue, context->passwordFound))) {
      context->matched = PR_TRUE;
      context->hostURIFound.Assign(context->hostURI);
    }

    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsPasswordManager::FindPasswordEntry(const nsACString& aHostURI,
                                     const nsAString&  aUsername,
                                     const nsAString&  aPassword,
                                     nsACString& aHostURIFound,
                                     nsAString&  aUsernameFound,
                                     nsAString&  aPasswordFound)
{
  if (!aHostURI.IsEmpty()) {
    SignonHashEntry* hashEnt;
    if (mSignonTable.Get(aHostURI, &hashEnt)) {
      SignonDataEntry* entry;
      nsresult rv = FindPasswordEntryInternal(hashEnt->head,
                                              aUsername,
                                              aPassword,
                                              EmptyString(),
                                              &entry);

      if (NS_SUCCEEDED(rv) && entry) {
        if (NS_SUCCEEDED(DecryptData(entry->userValue, aUsernameFound)) &&
            NS_SUCCEEDED(DecryptData(entry->passValue, aPasswordFound))) {
          aHostURIFound.Assign(aHostURI);
        } else {
          return NS_ERROR_FAILURE;
        }
      }

      return rv;
    }

    return NS_ERROR_FAILURE;
  }

  // No host given, so enumerate all entries in the hashtable
  findEntryContext context(this, aHostURI, aUsername, aPassword,
                           aHostURIFound, aUsernameFound, aPasswordFound);

  mSignonTable.EnumerateRead(FindEntryEnumerator, &context);

  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::AddUserFull(const nsACString& aKey,
                               const nsAString& aUser,
                               const nsAString& aPassword,
                               const nsAString& aUserFieldName,
                               const nsAString& aPassFieldName)
{
  // Silently ignore an empty username/password entry.
  // There's no point in taking up space in the signon file with this.
  if (aUser.IsEmpty() && aPassword.IsEmpty())
    return NS_OK;

  // Check for an existing entry for this host + user
  if (!aKey.IsEmpty()) {
    SignonHashEntry *hashEnt;
    if (mSignonTable.Get(aKey, &hashEnt)) {
      nsString empty;
      SignonDataEntry *entry = nsnull;
      FindPasswordEntryInternal(hashEnt->head, aUser, empty, empty, &entry);
      if (entry) {
        // Just change the password
        EncryptDataUCS2(aPassword, entry->passValue);
        // ... and update the field names...s
        entry->userField.Assign(aUserFieldName);
        entry->passField.Assign(aPassFieldName);
        return NS_OK;
      }
    }
  }

  SignonDataEntry* entry = new SignonDataEntry();
  entry->userField.Assign(aUserFieldName);
  entry->passField.Assign(aPassFieldName);
  EncryptDataUCS2(aUser, entry->userValue);
  EncryptDataUCS2(aPassword, entry->passValue);

  AddSignonData(aKey, entry);
  WritePasswords(mSignonFile);

  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::ReadPasswords(nsIFile* aPasswordFile)
{
  nsCOMPtr<nsIInputStream> fileStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileStream), aPasswordFile);
  if (!fileStream)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsILineInputStream> lineStream = do_QueryInterface(fileStream);
  NS_ASSERTION(lineStream, "File stream is not an nsILineInputStream");

  // Read the header
  nsCAutoString utf8Buffer;
  PRBool moreData = PR_FALSE;
  nsresult rv = lineStream->ReadLine(utf8Buffer, &moreData);
  if (NS_FAILED(rv))
    return NS_OK;

  if (!utf8Buffer.Equals(NS_LITERAL_CSTRING("#2c"))) {
    NS_ERROR("Unexpected version header in signon file");
    return NS_OK;
  }

  enum { STATE_REJECT, STATE_REALM, STATE_USERFIELD, STATE_USERVALUE,
         STATE_PASSFIELD, STATE_PASSVALUE } state = STATE_REJECT;

  nsCAutoString realm;
  SignonDataEntry* entry = nsnull;
  PRBool writeOnFinish = PR_FALSE;

  do {
    rv = lineStream->ReadLine(utf8Buffer, &moreData);
    if (NS_FAILED(rv))
      return NS_OK;

    switch (state) {
    case STATE_REJECT:
      if (utf8Buffer.Equals(NS_LITERAL_CSTRING(".")))
        state = STATE_REALM;
      else
        mRejectTable.Put(utf8Buffer, 1);

      break;

    case STATE_REALM:
      realm.Assign(utf8Buffer);
      state = STATE_USERFIELD;
      break;

    case STATE_USERFIELD:

      // Commit any completed entry
      if (entry) {
        // Weed out empty username+password entries from corrupted signon files
        if (entry->userValue.IsEmpty() && entry->passValue.IsEmpty()) {
          NS_WARNING("Discarding empty password entry");
          writeOnFinish = PR_TRUE; // so we won't get this on the next startup
          delete entry;
        } else {
          AddSignonData(realm, entry);
        }
      }

      // If the line is a ., we've reached the end of this realm's entries.
      if (utf8Buffer.Equals(NS_LITERAL_CSTRING("."))) {
        entry = nsnull;
        state = STATE_REALM;
      } else {
        entry = new SignonDataEntry();
        CopyUTF8toUTF16(utf8Buffer, entry->userField);
        state = STATE_USERVALUE;
      }

      break;

    case STATE_USERVALUE:
      NS_ASSERTION(entry, "bad state");

      CopyUTF8toUTF16(utf8Buffer, entry->userValue);

      state = STATE_PASSFIELD;
      break;

    case STATE_PASSFIELD:
      NS_ASSERTION(entry, "bad state");

      // Strip off the leading "*" character
      CopyUTF8toUTF16(Substring(utf8Buffer, 1, utf8Buffer.Length() - 1),
                      entry->passField);

      state = STATE_PASSVALUE;
      break;

    case STATE_PASSVALUE:
      NS_ASSERTION(entry, "bad state");

      CopyUTF8toUTF16(utf8Buffer, entry->passValue);

      state = STATE_USERFIELD;
      break;
    }
  } while (moreData);

  // Don't leak if the file ended unexpectedly
  delete entry;

  if (writeOnFinish) {
    fileStream->Close();
    WritePasswords(mSignonFile);
  }

  return NS_OK;
}


// nsIObserver implementation
NS_IMETHODIMP
nsPasswordManager::Observe(nsISupports* aSubject,
                           const char* aTopic,
                           const PRUnichar* aData)
{
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(aSubject);
    NS_ASSERTION(branch == mPrefBranch, "unexpected pref change notification");

    branch->GetBoolPref("rememberSignons", &sRememberPasswords);
  }

  return NS_OK;
}

// nsIWebProgressListener implementation
NS_IMETHODIMP
nsPasswordManager::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 PRUint32 aStateFlags,
                                 nsresult aStatus)
{
  // We're only interested in successful document loads.
  if (NS_FAILED(aStatus) ||
      !(aStateFlags & nsIWebProgressListener::STATE_IS_DOCUMENT) ||
      !(aStateFlags & nsIWebProgressListener::STATE_STOP))
    return NS_OK;

  // Don't do anything if the global signon pref is disabled
  if (!SingleSignonEnabled())
    return NS_OK;

  nsCOMPtr<nsIDOMWindow> domWin;
  nsresult rv = aWebProgress->GetDOMWindow(getter_AddRefs(domWin));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> domDoc;
  domWin->GetDocument(getter_AddRefs(domDoc));
  NS_ASSERTION(domDoc, "DOM window should always have a document!");

  // For now, only prefill forms in HTML documents.
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(domDoc);
  if (!htmlDoc)
    return NS_OK;

  nsCOMPtr<nsIDOMHTMLCollection> forms;
  htmlDoc->GetForms(getter_AddRefs(forms));
  
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

  nsCAutoString realm;
  if (!GetPasswordRealm(doc->GetDocumentURI(), realm))
    return NS_OK;

  SignonHashEntry* hashEnt;
  if (!mSignonTable.Get(realm, &hashEnt))
    return NS_OK;

  PRUint32 formCount;
  forms->GetLength(&formCount);

  // We can auto-prefill the username and password if there is only
  // one stored login that matches the username and password field names
  // on the form in question.  Note that we only need to worry about a
  // single login per form.

  for (PRUint32 i = 0; i < formCount; ++i) {
    nsCOMPtr<nsIDOMNode> formNode;
    forms->Item(i, getter_AddRefs(formNode));

    nsCOMPtr<nsIForm> form = do_QueryInterface(formNode);
    SignonDataEntry* firstMatch = nsnull;
    PRBool attachedToInput = PR_FALSE;
    nsCOMPtr<nsIDOMHTMLInputElement> userField, passField;
    nsCOMPtr<nsIDOMHTMLInputElement> temp;
    nsAutoString fieldType;

    for (SignonDataEntry* e = hashEnt->head; e; e = e->next) {
      
      nsCOMPtr<nsISupports> foundNode;
      form->ResolveName(e->userField, getter_AddRefs(foundNode));
      temp = do_QueryInterface(foundNode);

      nsAutoString oldUserValue;

      if (temp) {
        temp->GetType(fieldType);
        if (!fieldType.Equals(NS_LITERAL_STRING("text")))
          continue;

        temp->GetValue(oldUserValue);
        userField = temp;
      } else {
        continue;
      }

      if (!(e->passField).IsEmpty()) {
        form->ResolveName(e->passField, getter_AddRefs(foundNode));
        temp = do_QueryInterface(foundNode);
      }
      else {
        // No password field name was supplied, try to locate one in the form.
        nsCOMPtr<nsIFormControl> fc(do_QueryInterface(foundNode));
        PRInt32 index = -1;
        form->IndexOfControl(fc, &index);
        if (index >= 0) {
          PRUint32 count;
          form->GetElementCount(&count);

          PRUint32 i;
          temp = nsnull;

          // Search forwards
          nsCOMPtr<nsIFormControl> passField;
          for (i = index + 1; i < count; ++i) {
            form->GetElementAt(i, getter_AddRefs(passField));

            if (passField && passField->GetType() == NS_FORM_INPUT_PASSWORD) {
              foundNode = passField;
              temp = do_QueryInterface(foundNode);
            }
          }

          if (!temp && index != 0) {
            // Search backwards
            i = index;
            do {
              form->GetElementAt(i, getter_AddRefs(passField));

              if (passField && passField->GetType() == NS_FORM_INPUT_PASSWORD) {
                foundNode = passField;
                temp = do_QueryInterface(foundNode);
              }
            } while (i-- != 0);
          }
        }
      }

      nsAutoString oldPassValue;

      if (temp) {
        temp->GetType(fieldType);
        if (!fieldType.Equals(NS_LITERAL_STRING("password")))
          continue;

        temp->GetValue(oldPassValue);
        passField = temp;
      } else {
        continue;
      }

      if (!oldUserValue.IsEmpty()) {
        // The page has prefilled a username.
        // If it matches any of our saved usernames, prefill the password
        // for that username.  If there are multiple saved usernames,
        // we will also attach the autocomplete listener.

        nsAutoString userValue;
        if (NS_FAILED(DecryptData(e->userValue, userValue)))
          goto done;

        if (userValue.Equals(oldUserValue)) {
          nsAutoString passValue;
          if (NS_FAILED(DecryptData(e->passValue, passValue)))
            goto done;

          passField->SetValue(passValue);
        }
      }

      if (firstMatch && !attachedToInput) {
        // We've found more than one possible signon for this form.

        // Listen for blur and autocomplete events on the username field so
        // that we can attempt to prefill the password after the user has
        // entered the username.

        AttachToInput(userField);
        attachedToInput = PR_TRUE;
      } else {
        firstMatch = e;
      }
    }

    if (firstMatch && !attachedToInput) {
      nsAutoString buffer;

      if (NS_FAILED(DecryptData(firstMatch->userValue, buffer)))
        goto done;

      userField->SetValue(buffer);

      if (NS_FAILED(DecryptData(firstMatch->passValue, buffer)))
        goto done;

      passField->SetValue(buffer);
      AttachToInput(userField);
    }
  }

 done:
  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(domDoc);
  targ->AddEventListener(NS_LITERAL_STRING("unload"),
                         NS_STATIC_CAST(nsIDOMLoadListener*, this), PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRInt32 aCurSelfProgress,
                                    PRInt32 aMaxSelfProgress,
                                    PRInt32 aCurTotalProgress,
                                    PRInt32 aMaxTotalProgress)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::OnLocationChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    nsIURI* aLocation)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::OnSecurityChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRUint32 aState)
{
  return NS_OK;
}


// nsIFormSubmitObserver implementation
NS_IMETHODIMP
nsPasswordManager::Notify(nsIContent* aFormNode,
                          nsIDOMWindowInternal* aWindow,
                          nsIURI* aActionURL,
                          PRBool* aCancelSubmit)
{
  // This function must never return a failure code or the form submit
  // will be cancelled.

  // Don't do anything if the global signon pref is disabled
  if (!SingleSignonEnabled())
    return NS_OK;

  // Check the reject list
  nsCAutoString realm;
  if (!GetPasswordRealm(aFormNode->GetDocument()->GetDocumentURI(), realm))
    return NS_OK;

  PRInt32 rejectValue;
  if (mRejectTable.Get(realm, &rejectValue)) {
    // The user has opted to never save passwords for this site.
    return NS_OK;
  }

  nsCOMPtr<nsIForm> formElement = do_QueryInterface(aFormNode);

  PRUint32 numControls;
  formElement->GetElementCount(&numControls);

  // Count the number of password fields in the form.

  nsCOMPtr<nsIDOMHTMLInputElement> userField;
  nsCOMArray<nsIDOMHTMLInputElement> passFields;

  PRUint32 i, firstPasswordIndex = numControls;

  for (i = 0; i < numControls; ++i) {

    nsCOMPtr<nsIFormControl> control;
    formElement->GetElementAt(i, getter_AddRefs(control));

    if (control->GetType() == NS_FORM_INPUT_PASSWORD) {
      nsCOMPtr<nsIDOMHTMLInputElement> elem = do_QueryInterface(control);
      passFields.AppendObject(elem);
      if (firstPasswordIndex == numControls)
        firstPasswordIndex = i;
    }
  }

  nsCOMPtr<nsIPrompt> prompt;
  aWindow->GetPrompter(getter_AddRefs(prompt));

  switch (passFields.Count()) {
  case 1:  // normal login
    {
      // Search backwards from the password field to find a username field.
      for (PRInt32 j = (PRInt32) firstPasswordIndex - 1; j >= 0; --j) {
        nsCOMPtr<nsIFormControl> control;
        formElement->GetElementAt(j, getter_AddRefs(control));

        if (control->GetType() == NS_FORM_INPUT_TEXT) {
          userField = do_QueryInterface(control);
          break;
        }
      }

      // If the username field or the form has autocomplete=off,
      // we don't store the login

      nsAutoString autocomplete;

      if (userField) {
        nsCOMPtr<nsIDOMElement> userFieldElement = do_QueryInterface(userField);
        userFieldElement->GetAttribute(NS_LITERAL_STRING("autocomplete"),
                                       autocomplete);

        if (autocomplete.LowerCaseEqualsLiteral("off"))
          return NS_OK;
      }

      nsCOMPtr<nsIDOMElement> formDOMEl = do_QueryInterface(aFormNode);
      formDOMEl->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
      if (autocomplete.LowerCaseEqualsLiteral("off"))
        return NS_OK;

      nsCOMPtr<nsIDOMElement> passFieldElement = do_QueryInterface(passFields.ObjectAt(0));
      passFieldElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
      if (autocomplete.EqualsIgnoreCase("off"))
        return NS_OK;


      // Check whether this signon is already stored.
      // Note that we don't prompt the user if only the password doesn't match;
      // we instead just silently change the stored password.

      nsAutoString userValue, passValue, userFieldName, passFieldName;

      if (userField) {
        userField->GetValue(userValue);
        userField->GetName(userFieldName);
      }

      passFields.ObjectAt(0)->GetValue(passValue);
      passFields.ObjectAt(0)->GetName(passFieldName);

      // If username and password are both empty, there is no reason
      // to store this login.

      if (userValue.IsEmpty() && passValue.IsEmpty())
        return NS_OK;

      SignonHashEntry* hashEnt;

      if (mSignonTable.Get(realm, &hashEnt)) {

        SignonDataEntry* entry;
        nsAutoString buffer;

        for (entry = hashEnt->head; entry; entry = entry->next) {
          if (entry->userField.Equals(userFieldName) &&
              entry->passField.Equals(passFieldName)) {

            if (NS_FAILED(DecryptData(entry->userValue, buffer)))
              return NS_OK;

            if (buffer.Equals(userValue)) {

              if (NS_FAILED(DecryptData(entry->passValue, buffer)))
                return NS_OK;

              if (!buffer.Equals(passValue)) {
                if (NS_FAILED(EncryptDataUCS2(passValue, entry->passValue)))
                  return NS_OK;

                WritePasswords(mSignonFile);
              }

              return NS_OK;
            }
          }
        }
      }

      nsAutoString dialogTitle, dialogText, neverText;
      GetLocalizedString(NS_LITERAL_STRING("savePasswordTitle"), dialogTitle);
      GetLocalizedString(NS_LITERAL_STRING("savePasswordText"),
                         dialogText,
                         PR_TRUE);
      GetLocalizedString(NS_LITERAL_STRING("neverForSite"), neverText);

      PRInt32 selection;
      prompt->ConfirmEx(dialogTitle.get(),
                        dialogText.get(),
                        (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                        (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1) +
                        (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2),
                        nsnull, nsnull,
                        neverText.get(),
                        nsnull, nsnull,
                        &selection);

      if (selection == 0) {
        SignonDataEntry* entry = new SignonDataEntry();
        entry->userField.Assign(userFieldName);
        entry->passField.Assign(passFieldName);
        if (NS_FAILED(EncryptDataUCS2(userValue, entry->userValue)) ||
            NS_FAILED(EncryptDataUCS2(passValue, entry->passValue))) {
          delete entry;
          return NS_OK;
        }

        AddSignonData(realm, entry);
        WritePasswords(mSignonFile);
      } else if (selection == 2) {
        AddReject(realm);
      }
    }
    break;

  case 2:
  case 3:
    {
      // If the following conditions are true, we guess that this is a
      // password change page:
      //   - there are 2 or 3 password fields on the page
      //   - the fields do not all have the same value
      //   - there is already a stored login for this realm
      //
      // In this situation, prompt the user to confirm that this is a password
      // change.

      SignonDataEntry* changeEntry = nsnull;
      nsAutoString value0, valueN;
      passFields.ObjectAt(0)->GetValue(value0);

      for (PRInt32 k = 1; k < passFields.Count(); ++k) {
        passFields.ObjectAt(k)->GetValue(valueN);
        if (!value0.Equals(valueN)) {

          SignonHashEntry* hashEnt;

          if (mSignonTable.Get(realm, &hashEnt)) {

            SignonDataEntry* entry = hashEnt->head;

            if (entry->next) {

              // Multiple stored logons, prompt for which username is
              // being changed.

              PRUint32 entryCount = 2;
              SignonDataEntry* temp = entry->next;
              while (temp->next) {
                ++entryCount;
                temp = temp->next;
              }

              nsAutoString* ptUsernames = new nsAutoString[entryCount];
              const PRUnichar** formatArgs = new const PRUnichar*[entryCount];
              temp = entry;

              for (PRUint32 arg = 0; arg < entryCount; ++arg) {
                if (NS_FAILED(DecryptData(temp->userValue, ptUsernames[arg])))
                  return NS_OK;

                formatArgs[arg] = ptUsernames[arg].get();
                temp = temp->next;
              }

              nsAutoString dialogTitle, dialogText;
              GetLocalizedString(NS_LITERAL_STRING("passwordChangeTitle"),
                                 dialogTitle);
              GetLocalizedString(NS_LITERAL_STRING("userSelectText"),
                                 dialogText);

              PRInt32 selection;
              PRBool confirm;
              prompt->Select(dialogTitle.get(),
                             dialogText.get(),
                             entryCount,
                             formatArgs,
                             &selection,
                             &confirm);

              delete[] formatArgs;
              delete[] ptUsernames;

              if (confirm && selection >= 0) {
                changeEntry = entry;
                for (PRInt32 m = 0; m < selection; ++m)
                  changeEntry = changeEntry->next;
              }

            } else {
              nsAutoString dialogTitle, dialogText, ptUser;

              if (NS_FAILED(DecryptData(entry->userValue, ptUser)))
                return NS_OK;

              const PRUnichar* formatArgs[1] = { ptUser.get() };

              GetLocalizedString(NS_LITERAL_STRING("passwordChangeTitle"),
                                 dialogTitle);
              GetLocalizedString(NS_LITERAL_STRING("passwordChangeText"),
                                 dialogText,
                                 PR_TRUE,
                                 formatArgs,
                                 1);

              PRInt32 selection;
              prompt->ConfirmEx(dialogTitle.get(),
                                dialogText.get(),
                                (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                                (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1),
                                nsnull, nsnull, nsnull, nsnull, nsnull,
                                &selection);

              if (selection == 0)
                changeEntry = entry;
            }
          }
          break;
        }
      }

      if (changeEntry) {
        nsAutoString newValue;
        passFields.ObjectAt(1)->GetValue(newValue);
        if (NS_FAILED(EncryptDataUCS2(newValue, changeEntry->passValue)))
          return NS_OK;

        WritePasswords(mSignonFile);
      }
    }
    break;

  default:  // no passwords or something odd; be safe and just don't store anything
    break;
  }


  return NS_OK;
}

// nsIDOMFocusListener implementation

NS_IMETHODIMP
nsPasswordManager::Focus(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::Blur(nsIDOMEvent* aEvent)
{
  return FillPassword(aEvent);
}

NS_IMETHODIMP
nsPasswordManager::HandleEvent(nsIDOMEvent* aEvent)
{
  return FillPassword(aEvent);
}

// Autocomplete implementation

class UserAutoComplete : public nsIAutoCompleteResult
{
public:
  UserAutoComplete(const nsACString& aHost, const nsAString& aSearchString);
  virtual ~UserAutoComplete();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULT

  nsVoidArray mArray;
  nsCString   mHost;
  nsString    mSearchString;
  PRInt32     mDefaultIndex;
  PRUint16    mResult;
};
  
UserAutoComplete::UserAutoComplete(const nsACString& aHost,
                                   const nsAString& aSearchString)
  : mHost(aHost),
    mSearchString(aSearchString),
    mDefaultIndex(-1),
    mResult(RESULT_FAILURE)
{
}

UserAutoComplete::~UserAutoComplete()
{
  for (PRInt32 i  = 0; i < mArray.Count(); ++i)
    nsMemory::Free(mArray.ElementAt(i));
}

NS_IMPL_ISUPPORTS1(UserAutoComplete, nsIAutoCompleteResult)

NS_IMETHODIMP
UserAutoComplete::GetSearchString(nsAString& aString)
{
  aString.Assign(mSearchString);
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::GetSearchResult(PRUint16* aResult)
{
  *aResult = mResult;
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::GetDefaultIndex(PRInt32* aDefaultIndex)
{
  *aDefaultIndex = mDefaultIndex;
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::GetErrorDescription(nsAString& aDescription)
{
  aDescription.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::GetMatchCount(PRUint32* aCount)
{
  *aCount = mArray.Count();
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::GetValueAt(PRInt32 aIndex, nsAString& aValue)
{
  aValue.Assign(NS_STATIC_CAST(PRUnichar*, mArray.ElementAt(aIndex)));
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::GetCommentAt(PRInt32 aIndex, nsAString& aComment)
{
  aComment.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::GetStyleAt(PRInt32 aIndex, nsAString& aHint)
{
  aHint.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
UserAutoComplete::RemoveValueAt(PRInt32 aIndex, PRBool aRemoveFromDB)
{
  NS_ENSURE_TRUE(aIndex >= 0 && aIndex < mArray.Count(), NS_ERROR_INVALID_ARG);

  PRUnichar *user = NS_STATIC_CAST(PRUnichar*, mArray.ElementAt(aIndex));
  if (aRemoveFromDB)
    sPasswordManager->RemoveUser(mHost, nsDependentString(user));

  nsMemory::Free(user);
  mArray.RemoveElementAt(aIndex);
  return NS_OK;
}

PR_STATIC_CALLBACK(int)
SortPRUnicharComparator(const void* aElement1,
                        const void* aElement2,
                        void* aData)
{
  return nsCRT::strcmp(NS_STATIC_CAST(const PRUnichar*, aElement1),
                       NS_STATIC_CAST(const PRUnichar*, aElement2));
}

PRBool
nsPasswordManager::AutoCompleteSearch(const nsAString& aSearchString,
                                      nsIAutoCompleteResult* aPreviousResult,
                                      nsIDOMHTMLInputElement* aElement,
                                      nsIAutoCompleteResult** aResult)
{
  PRInt32 dummy;
  if (!SingleSignonEnabled() || !mAutoCompleteInputs.Get(aElement, &dummy))
    return PR_FALSE;

  UserAutoComplete* result = nsnull;

  if (aPreviousResult) {

    // We have a list of results for a shorter search string, so just
    // filter them further based on the new search string.

    result = NS_STATIC_CAST(UserAutoComplete*, aPreviousResult);

    if (result->mArray.Count()) {
      for (PRInt32 i = result->mArray.Count() - 1; i >= 0; --i) {
        nsDependentString match(NS_STATIC_CAST(PRUnichar*, result->mArray.ElementAt(i)));
        if (aSearchString.Length() > match.Length() ||
            !StringBeginsWith(match, aSearchString)) {
          nsMemory::Free(result->mArray.ElementAt(i));
          result->mArray.RemoveElementAt(i);
        }
      }
    }
  } else {

    nsCOMPtr<nsIDOMDocument> domDoc;
    aElement->GetOwnerDocument(getter_AddRefs(domDoc));

    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

    nsCAutoString realm;
    if (!GetPasswordRealm(doc->GetDocumentURI(), realm)) {
      *aResult = nsnull;
      return NS_OK;
    }

    // Get all of the matches into an array that we can sort.

    result = new UserAutoComplete(realm, aSearchString);

    SignonHashEntry* hashEnt;
    if (mSignonTable.Get(realm, &hashEnt)) {
      // Protect against a reentrant call to DecryptData.  For example, if
      // DecryptData causes the Master Password dialog to appear, we don't
      // want to respond to a blur on the input element by trying to prefill
      // the password.

      mAutoCompletingField = aElement;

      for (SignonDataEntry* e = hashEnt->head; e; e = e->next) {

        nsAutoString userValue;
        if (NS_FAILED(DecryptData(e->userValue, userValue)))
          return NS_ERROR_FAILURE;

        if (aSearchString.Length() < userValue.Length() &&
            StringBeginsWith(userValue, aSearchString)) {
          result->mArray.AppendElement(ToNewUnicode(userValue));
        }
      }

      mAutoCompletingField = nsnull;
    }

    if (result->mArray.Count()) {
      result->mArray.Sort(SortPRUnicharComparator, nsnull);
      result->mResult = nsIAutoCompleteResult::RESULT_SUCCESS;
      result->mDefaultIndex = 0;
    } else {
      result->mResult = nsIAutoCompleteResult::RESULT_NOMATCH;
      result->mDefaultIndex = -1;
    }
  }

  *aResult = result;
  NS_ADDREF(*aResult);

  return PR_TRUE;
}

// nsIDOMLoadListener implementation

NS_IMETHODIMP
nsPasswordManager::Load(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::BeforeUnload(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

 /* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::RemoveForDOMDocumentEnumerator(nsISupports* aKey,
                                                  PRInt32& aEntry,
                                                  void* aUserData)
{
  nsIDOMDocument* domDoc = NS_STATIC_CAST(nsIDOMDocument*, aUserData);
  nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(aKey);
  nsCOMPtr<nsIDOMDocument> elementDoc;
  element->GetOwnerDocument(getter_AddRefs(elementDoc));
  if (elementDoc == domDoc)
    return PL_DHASH_REMOVE;

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsPasswordManager::Unload(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(target);
  if (domDoc)
    mAutoCompleteInputs.Enumerate(RemoveForDOMDocumentEnumerator, domDoc);

  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::Abort(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::Error(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


// internal methods

/*

Format of the single signon file:

<1-line version header>
<Reject list URL #1>
<Reject list URL #2>
.
<Saved URL #1 realm>
<Saved URL #1 username field name>
<Encrypted Saved URL #1 username field value>
*<Saved URL #1 password field name>
<Encrypted Saved URL #1 password field value>
<Saved URL #1 username #2 field name>
<.....>
.
<Saved URL #2 realm>
.....
<Encrypted Saved URL #N password field value>
.
<EOF>

*/

/* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::WriteRejectEntryEnumerator(const nsACString& aKey,
                                              PRInt32 aEntry,
                                              void* aUserData)
{
  nsIOutputStream* stream = NS_STATIC_CAST(nsIOutputStream*, aUserData);
  PRUint32 bytesWritten;

  nsCAutoString buffer(aKey);
  buffer.Append(NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  return PL_DHASH_NEXT;
}

/* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::WriteSignonEntryEnumerator(const nsACString& aKey,
                                              SignonHashEntry* aEntry,
                                              void* aUserData)
{
  nsIOutputStream* stream = NS_STATIC_CAST(nsIOutputStream*, aUserData);
  PRUint32 bytesWritten;

  nsCAutoString buffer(aKey);
  buffer.Append(NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  for (SignonDataEntry* e = aEntry->head; e; e = e->next) {
    NS_ConvertUCS2toUTF8 userField(e->userField);
    userField.Append(NS_LINEBREAK);
    stream->Write(userField.get(), userField.Length(), &bytesWritten);

    buffer.Assign(NS_ConvertUCS2toUTF8(e->userValue));
    buffer.Append(NS_LINEBREAK);
    stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

    buffer.Assign("*");
    buffer.Append(NS_ConvertUCS2toUTF8(e->passField));
    buffer.Append(NS_LINEBREAK);
    stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

    buffer.Assign(NS_ConvertUCS2toUTF8(e->passValue));
    buffer.Append(NS_LINEBREAK);
    stream->Write(buffer.get(), buffer.Length(), &bytesWritten);
  }

  buffer.Assign("." NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  return PL_DHASH_NEXT;
}

void
nsPasswordManager::WritePasswords(nsIFile* aPasswordFile)
{
  nsCOMPtr<nsIOutputStream> fileStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(fileStream), aPasswordFile, -1,
                              0600, 0);

  if (!fileStream)
    return;

  PRUint32 bytesWritten;

  // File header
  nsCAutoString buffer("#2c" NS_LINEBREAK);
  fileStream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  // Write out the reject list.
  mRejectTable.EnumerateRead(WriteRejectEntryEnumerator, fileStream);

  buffer.Assign("." NS_LINEBREAK);
  fileStream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  // Write out the signon data.
  mSignonTable.EnumerateRead(WriteSignonEntryEnumerator, fileStream);
}

void
nsPasswordManager::AddSignonData(const nsACString& aRealm,
                                 SignonDataEntry* aEntry)
{
  // See if there is already an entry for this URL
  SignonHashEntry* hashEnt;
  if (mSignonTable.Get(aRealm, &hashEnt)) {
    // Add this one at the front of the linked list
    aEntry->next = hashEnt->head;
    hashEnt->head = aEntry;
  } else {
    mSignonTable.Put(aRealm, new SignonHashEntry(aEntry));
  }
}

/* static */ nsresult
nsPasswordManager::DecryptData(const nsAString& aData,
                               nsAString& aPlaintext)
{
  NS_ConvertUCS2toUTF8 flatData(aData);
  char* buffer = nsnull;

  if (flatData.CharAt(0) == '~') {

    // This is a base64-encoded string. Strip off the ~ prefix.
    PRUint32 srcLength = flatData.Length() - 1;

    if (!(buffer = PL_Base64Decode(&(flatData.get())[1], srcLength, NULL)))
      return NS_ERROR_FAILURE;

  } else {

    // This is encrypted using nsISecretDecoderRing.
    EnsureDecoderRing();
    if (!sDecoderRing) {
      NS_WARNING("Unable to get decoder ring service");
      return NS_ERROR_FAILURE;
    }

    if (NS_FAILED(sDecoderRing->DecryptString(flatData.get(), &buffer)))
      return NS_ERROR_FAILURE;

  }

  aPlaintext.Assign(NS_ConvertUTF8toUCS2(buffer));
  PR_Free(buffer);

  return NS_OK;
}

// Note that nsISecretDecoderRing encryption uses a pseudo-random salt value,
// so it's not possible to test equality of two strings by comparing their
// ciphertexts.  We need to decrypt both strings and compare the plaintext.


/* static */ nsresult
nsPasswordManager::EncryptData(const nsAString& aPlaintext,
                               nsACString& aEncrypted)
{
  EnsureDecoderRing();
  NS_ENSURE_TRUE(sDecoderRing, NS_ERROR_FAILURE);

  char* buffer;
  if (NS_FAILED(sDecoderRing->EncryptString(NS_ConvertUCS2toUTF8(aPlaintext).get(), &buffer)))
    return NS_ERROR_FAILURE;

  aEncrypted.Assign(buffer);
  PR_Free(buffer);

  return NS_OK;
}

/* static */ nsresult
nsPasswordManager::EncryptDataUCS2(const nsAString& aPlaintext,
                                   nsAString& aEncrypted)
{
  nsCAutoString buffer;
  nsresult rv = EncryptData(aPlaintext, buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  aEncrypted.Assign(NS_ConvertUTF8toUCS2(buffer));
  return NS_OK;
}

/* static */ void
nsPasswordManager::EnsureDecoderRing()
{
  if (!sDecoderRing) {
    CallGetService("@mozilla.org/security/sdr;1", &sDecoderRing);

    // Ensure that the master password (internal key) has been initialized.
    // If not, set a default empty master password.

    nsCOMPtr<nsIPK11TokenDB> tokenDB = do_GetService(NS_PK11TOKENDB_CONTRACTID);
    if (!tokenDB)
      return;

    nsCOMPtr<nsIPK11Token> token;
    tokenDB->GetInternalKeyToken(getter_AddRefs(token));

    PRBool needUserInit = PR_FALSE;
    token->GetNeedsUserInit(&needUserInit);

    if (needUserInit)
      token->InitPassword(EmptyString().get());
  }
}

nsresult
nsPasswordManager::FindPasswordEntryInternal(const SignonDataEntry* aEntry,
                                             const nsAString&  aUser,
                                             const nsAString&  aPassword,
                                             const nsAString&  aUserField,
                                             SignonDataEntry** aResult)
{
  // host has already been checked, so just look for user/password match.
  const SignonDataEntry* entry = aEntry;
  nsAutoString buffer;

  for (; entry; entry = entry->next) {

    PRBool matched;

    if (aUser.IsEmpty()) {
      matched = PR_TRUE;
    } else {
      if (NS_FAILED(DecryptData(entry->userValue, buffer))) {
        *aResult = nsnull;
        return NS_ERROR_FAILURE;
      }
      matched = aUser.Equals(buffer);
    }

    if (!matched)
      continue;

    if (aPassword.IsEmpty()) {
      matched = PR_TRUE;
    } else {
      if (NS_FAILED(DecryptData(entry->passValue, buffer))) {
        *aResult = nsnull;
        return NS_ERROR_FAILURE;
      }
      matched = aPassword.Equals(buffer);
    }

    if (!matched)
      continue;

    if (aUserField.IsEmpty())
      matched = PR_TRUE;
    else
      matched = entry->userField.Equals(aUserField);

    if (matched)
      break;
  }

  if (entry) {
    *aResult = NS_CONST_CAST(SignonDataEntry*, entry);
    return NS_OK;
  }

  *aResult = nsnull;
  return NS_ERROR_FAILURE;
}

nsresult
nsPasswordManager::FillPassword(nsIDOMEvent* aEvent)
{
  // Try to prefill the password for the just-changed username.
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));

  nsCOMPtr<nsIDOMHTMLInputElement> userField = do_QueryInterface(target);
  if (!userField || userField == mAutoCompletingField)
    return NS_OK;

  nsCOMPtr<nsIContent> fieldContent = do_QueryInterface(userField);

  // The document may be null during teardown, for example as Windows
  // sends a blur event as a native widget is destroyed.
  nsIDocument *doc = fieldContent->GetDocument();
  if (!doc)
    return NS_OK;

  nsCAutoString realm;
  if (!GetPasswordRealm(doc->GetDocumentURI(), realm))
    return NS_OK;

  nsAutoString userValue;
  userField->GetValue(userValue);

  if (userValue.IsEmpty())
    return NS_OK;

  nsAutoString fieldName;
  userField->GetName(fieldName);

  SignonHashEntry* hashEnt;
  if (!mSignonTable.Get(realm, &hashEnt))
    return NS_OK;

  SignonDataEntry* foundEntry;
  FindPasswordEntryInternal(hashEnt->head, userValue, EmptyString(),
                            fieldName, &foundEntry);

  if (!foundEntry)
    return NS_OK;

  nsCOMPtr<nsIDOMHTMLFormElement> formEl;
  userField->GetForm(getter_AddRefs(formEl));
  if (!formEl)
    return NS_OK;

  nsCOMPtr<nsIForm> form = do_QueryInterface(formEl);
  nsCOMPtr<nsISupports> foundNode;
  form->ResolveName(foundEntry->passField, getter_AddRefs(foundNode));
  nsCOMPtr<nsIDOMHTMLInputElement> passField = do_QueryInterface(foundNode);
  if (!passField)
    return NS_OK;

  nsAutoString passValue;
  if (NS_SUCCEEDED(DecryptData(foundEntry->passValue, passValue)))
    passField->SetValue(passValue);

  return NS_OK;
}

void
nsPasswordManager::AttachToInput(nsIDOMHTMLInputElement* aElement)
{
  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(aElement);
  nsIDOMEventListener* listener = NS_STATIC_CAST(nsIDOMFocusListener*, this);

  targ->AddEventListener(NS_LITERAL_STRING("blur"), listener, PR_FALSE);
  targ->AddEventListener(NS_LITERAL_STRING("DOMAutoComplete"), listener, PR_FALSE);

  mAutoCompleteInputs.Put(aElement, 1);
}

PRBool
nsPasswordManager::GetPasswordRealm(nsIURI* aURI, nsACString& aRealm)
{
  // Note: this _is_ different from getting the uri's prePath!
  // We don't want to include a username or password that's part of the
  // URL in the host key... it will cause lookups to work incorrectly, and will
  // also cause usernames and passwords to be stored in cleartext.

  nsCAutoString buffer;
  aURI->GetScheme(buffer);

  aRealm.Append(buffer);
  aRealm.AppendLiteral("://");

  aURI->GetHostPort(buffer);
  if (buffer.IsEmpty()) {
    // The scheme does not support hostnames, so don't attempt to save/restore
    // any signon data. (see bug 159484)
    return PR_FALSE;
  }

  aRealm.Append(buffer);
  return PR_TRUE;
}

/* static */ void
nsPasswordManager::GetLocalizedString(const nsAString& key,
                                      nsAString& aResult,
                                      PRBool aIsFormatted,
                                      const PRUnichar** aFormatArgs,
                                      PRUint32 aFormatArgsLength)
{
  if (!sPMBundle) {
    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    bundleService->CreateBundle(kPMPropertiesURL,
                                &sPMBundle);

    if (!sPMBundle) {
      NS_ERROR("string bundle not present");
      return;
    }
  }

  nsXPIDLString str;
  if (aIsFormatted)
    sPMBundle->FormatStringFromName(PromiseFlatString(key).get(),
                                    aFormatArgs, aFormatArgsLength,
                                    getter_Copies(str));
  else
    sPMBundle->GetStringFromName(PromiseFlatString(key).get(),
                                 getter_Copies(str));
  aResult.Assign(str);
}
