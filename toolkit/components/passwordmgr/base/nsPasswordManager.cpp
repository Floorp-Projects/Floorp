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

static const char kSatchelPropertiesURL[] = "chrome://passwordmgr/locale/passwordmgr.properties";
static PRBool sRememberPasswords = PR_FALSE;
static PRBool sPrefsInitialized = PR_FALSE;

static nsIStringBundle* sSatchelBundle;

static void SatchelLocalizedString(const nsAString& key, nsAString& aResult,
                                   PRBool aFormatted = PR_FALSE);

class nsPasswordManager::SignonDataEntry
{
public:
  nsString userField;
  nsString userValue;
  nsString passField;
  nsString passValue;
  SignonDataEntry* next;

  SignonDataEntry() : next(nsnull) { }
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
};

NS_IMPL_ISUPPORTS1(nsPasswordManager::PasswordEntry, nsIPassword)

nsPasswordManager::PasswordEntry::PasswordEntry(const nsACString& aKey,
                                                SignonDataEntry* aData)
  : mHost(aKey)
{
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
  aUser.Assign(mUser);
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::PasswordEntry::GetPassword(nsAString& aPassword)
{
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
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPasswordManager)
NS_INTERFACE_MAP_END

nsPasswordManager::nsPasswordManager()
{
}

nsPasswordManager::~nsPasswordManager()
{
}


nsresult
nsPasswordManager::Init()
{
  mSignonTable.Init();
  mRejectTable.Init();

  sPrefsInitialized = PR_TRUE;

  nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ASSERTION(prefService, "No pref service");

  nsCOMPtr<nsIPrefBranch> prefBranch;
  prefService->GetBranch("signon.", getter_AddRefs(prefBranch));
  NS_ASSERTION(prefBranch, "No pref branch");

  prefBranch->GetBoolPref("rememberSignons", &sRememberPasswords);

  nsCOMPtr<nsIPrefBranchInternal> branchInternal = do_QueryInterface(prefBranch);

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
  prefBranch->GetCharPref("SignonFileName", getter_Copies(signonFile));
  NS_ASSERTION(!signonFile.IsEmpty(), "Fallback for signon filename on present");

  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mSignonFile));
  mSignonFile->AppendNative(signonFile);

  nsCAutoString path;
  mSignonFile->GetNativePath(path);

  ReadSignonFile();

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

/* static */ nsresult
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

/* static */ nsresult
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
  NS_IF_RELEASE(sSatchelBundle);
}

// nsIPasswordManager implementation

NS_IMETHODIMP
nsPasswordManager::AddUser(const nsACString& aHost,
                           const nsAString& aUser,
                           const nsAString& aPassword)
{
  SignonDataEntry* entry = new SignonDataEntry();
  entry->userValue.Assign(aUser);
  entry->passValue.Assign(aPassword);

  AddSignonData(aHost, entry);
  WriteSignonFile();

  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::RemoveUser(const nsACString& aHost, const nsAString& aUser)
{
  SignonDataEntry* entry, *prevEntry = nsnull;

  if (!mSignonTable.Get(aHost, &entry) || !entry)
    return NS_ERROR_FAILURE;

  for (; entry; prevEntry = entry, entry = entry->next) {
    if (entry->userValue.Equals(aUser)) {
      if (prevEntry) {
        prevEntry->next = entry->next;
        delete entry;
      } else // the hashtable will delete the entry
        mSignonTable.Remove(aHost);

      WriteSignonFile();

      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPasswordManager::AddReject(const nsACString& aHost)
{
  mRejectTable.Put(aHost, 1);
  return NS_OK;
}

NS_IMETHODIMP
nsPasswordManager::RemoveReject(const nsACString& aHost)
{
  mRejectTable.Remove(aHost);
  WriteSignonFile();
  return NS_OK;
}

/* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::BuildArrayEnumerator(const nsACString& aKey,
                                        SignonDataEntry* aEntry,
                                        void* aUserData)
{
  nsIMutableArray* array = NS_STATIC_CAST(nsIMutableArray*, aUserData);

  nsCOMPtr<nsIPassword> passwordEntry = new PasswordEntry(aKey, aEntry);
  array->AppendElement(passwordEntry, PR_FALSE);

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
                                       SignonDataEntry* aEntry,
                                       void* aUserData)
{
  findEntryContext* context = NS_STATIC_CAST(findEntryContext*, aUserData);
  nsPasswordManager* manager = context->manager;
  nsresult rv;

  rv = manager->FindPasswordEntryFromSignonData(aEntry,
                                                context->hostURI,
                                                context->username,
                                                context->password,
                                                context->hostURIFound,
                                                context->usernameFound,
                                                context->passwordFound);
  if (NS_SUCCEEDED(rv)) {
    context->matched = PR_TRUE;
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
    SignonDataEntry* entry;
    if (mSignonTable.Get(aHostURI, &entry) && entry) {
      return FindPasswordEntryFromSignonData(entry, aHostURI, aUsername,
                                             aPassword, aHostURIFound,
                                             aUsernameFound, aPasswordFound);
    }

    return NS_ERROR_FAILURE;
  }

  // No host given, so enumerate all entries in the hashtable
  findEntryContext context(this, aHostURI, aUsername, aPassword,
                           aHostURIFound, aUsernameFound, aPasswordFound);

  mSignonTable.EnumerateRead(FindEntryEnumerator, &context);

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
    NS_ASSERTION(branch, "subject should be a pref branch");

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

  nsCOMPtr<nsIURI> uri;
  doc->GetDocumentURL(getter_AddRefs(uri));

  nsCAutoString realm;
  if (uri)
    uri->GetPrePath(realm);

  // Only prefill if there is exactly one username saved for this
  // realm.  Otherwise, we'll wait to prefill the password when the user
  // autocompletes the username.

  SignonDataEntry* entry;
  if (!mSignonTable.Get(realm, &entry) || !entry ||
      (entry && entry->next))
    return NS_OK;

  // Locate the username field by searching each form on the page

  PRUint32 formCount;
  forms->GetLength(&formCount);

  for (PRUint32 i = 0; i < formCount; ++i) {
    nsCOMPtr<nsIDOMNode> formNode;
    forms->Item(i, getter_AddRefs(formNode));

    nsCOMPtr<nsIForm> form = do_QueryInterface(formNode);
    nsCOMPtr<nsISupports> foundNode;
    form->ResolveName(entry->userField, getter_AddRefs(foundNode));

    nsCOMPtr<nsIDOMHTMLInputElement> userField = do_QueryInterface(foundNode);

    if (!foundNode && !userField)
      continue;

    // Ensure that the password field is also present, otherwise
    // this is not a login form.

    form->ResolveName(entry->passField, getter_AddRefs(foundNode));
    nsCOMPtr<nsIDOMHTMLInputElement> passField = do_QueryInterface(foundNode);
    if (!foundNode || !passField)
      continue;

    userField->SetValue(entry->userValue);
    passField->SetValue(entry->passValue);
  }

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
  // Check the reject list
  nsCOMPtr<nsIDocument> formDoc;
  aFormNode->GetDocument(getter_AddRefs(formDoc));
  nsCOMPtr<nsIURI> uri;
  formDoc->GetDocumentURL(getter_AddRefs(uri));

  nsCAutoString realm;
  uri->GetPrePath(realm);
  PRInt32 rejectValue;
  if (mRejectTable.Get(realm, &rejectValue)) {
    // The user has opted to never save passwords for this site.
    return NS_OK;
  }

  nsCOMPtr<nsIForm> formElement = do_QueryInterface(aFormNode);

  PRUint32 numControls;
  formElement->GetElementCount(&numControls);

  // Look for a password field in this form.  If there isn't one,
  // don't try to save any login data.

  nsCOMPtr<nsIDOMHTMLInputElement> userField;
  nsCOMPtr<nsIDOMHTMLInputElement> passField;

  PRUint32 i;
  for (i = 0; i < numControls; ++i) {

    nsCOMPtr<nsIFormControl> control;
    formElement->GetElementAt(i, getter_AddRefs(control));

    if (control->GetType() == NS_FORM_INPUT_PASSWORD) {
      // We've got the password field now
      passField = do_QueryInterface(control);
      break;
    }
  }

  if (!passField)  // no passwords, don't save anything
    return NS_OK;

  // Search backwards from the password field to find a username field.
  for (PRUint32 j = i - 1; j >= 0; --j) {
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

    if (autocomplete.EqualsIgnoreCase("off"))
      return NS_OK;
  }

  nsCOMPtr<nsIDOMElement> formDOMEl = do_QueryInterface(aFormNode);
  formDOMEl->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
  if (autocomplete.EqualsIgnoreCase("off"))
    return NS_OK;


  // Check whether this username and password are already stored
  nsAutoString userValue, passValue, userFieldName, passFieldName;

  if (userField) {
    userField->GetValue(userValue);
    userField->GetName(userFieldName);
  }

  passField->GetValue(passValue);
  passField->GetName(passFieldName);

  SignonDataEntry* entry;
  if (mSignonTable.Get(realm, &entry)) {
    while (entry) {
      if (entry->userField.Equals(userFieldName) &&
          entry->userValue.Equals(userValue) &&
          entry->passField.Equals(passFieldName) &&
          entry->passValue.Equals(passValue)) {

        // It's already present; nothing else to do.
        return NS_OK;
      }

      entry = entry->next;
    }
  }

  nsCOMPtr<nsIPrompt> prompt;
  aWindow->GetPrompter(getter_AddRefs(prompt));

  nsAutoString dialogTitle, dialogText, neverText;
  SatchelLocalizedString(NS_LITERAL_STRING("savePasswordTitle"), dialogTitle);
  SatchelLocalizedString(NS_LITERAL_STRING("savePasswordText"),
                         dialogText,
                         PR_TRUE);
  SatchelLocalizedString(NS_LITERAL_STRING("neverForSite"), neverText);

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
    entry->userValue.Assign(userValue);
    entry->passValue.Assign(passValue);

    AddSignonData(realm, entry);
  }

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
.
<Saved URL #2 realm>
.....
<Encrypted Saved URL #N password field value>
.
<EOF>

*/

void
nsPasswordManager::ReadSignonFile()
{
  nsCOMPtr<nsIInputStream> fileStream;
  NS_NewLocalFileInputStream(getter_AddRefs(fileStream), mSignonFile);
  if (!fileStream)
    return;

  nsCOMPtr<nsILineInputStream> lineStream = do_QueryInterface(fileStream);
  NS_ASSERTION(lineStream, "File stream is not an nsILineInputStream");

  // Read the header
  nsAutoString buffer;
  PRBool moreData = PR_FALSE;
  nsresult rv = lineStream->ReadLine(buffer, &moreData);
  if (NS_FAILED(rv))
    return;

  if (!buffer.Equals(NS_LITERAL_STRING("#2c"))) {
    NS_ERROR("Unexpected version header in signon file");
    return;
  }

  enum { STATE_REJECT, STATE_REALM, STATE_USERFIELD, STATE_USERVALUE,
         STATE_PASSFIELD, STATE_PASSVALUE } state = STATE_REJECT;

  nsCAutoString realm;
  nsAutoString decryptedValue;
  SignonDataEntry* entry = nsnull;
  PRBool decryptError = PR_FALSE;

  do {
    rv = lineStream->ReadLine(buffer, &moreData);
    if (NS_FAILED(rv))
      return;

    switch (state) {
    case STATE_REJECT:
      if (buffer.Equals(NS_LITERAL_STRING(".")))
        state = STATE_REALM;
      else
        mRejectTable.Put(NS_ConvertUCS2toUTF8(buffer), 1);

      break;

    case STATE_REALM:
      realm.Assign(NS_ConvertUCS2toUTF8(buffer));
      state = STATE_USERFIELD;
      break;

    case STATE_USERFIELD:
      // If the line is a ., we've reached the end of this realm's entries.
      if (buffer.Equals(NS_LITERAL_STRING("."))) {
        if (entry) {
          // Add this entry to the hashtable, unless we had a decryption error
          if (!decryptError)
            AddSignonData(realm, entry);
          entry = nsnull;
          decryptError = PR_FALSE;
          state = STATE_REALM;
        }
      } else {
        entry = new SignonDataEntry();
        entry->userField.Assign(buffer);
        state = STATE_USERVALUE;
      }

      break;

    case STATE_USERVALUE:
      NS_ASSERTION(entry, "bad state");

      if (NS_SUCCEEDED(DecryptData(buffer, decryptedValue)))
        entry->userValue.Assign(decryptedValue);
      else
        decryptError = PR_TRUE;  // abort this entry

      state = STATE_PASSFIELD;
      break;

    case STATE_PASSFIELD:
      NS_ASSERTION(entry, "bad state");

      // Strip off the leading "*" character
      if (!decryptError)
        entry->passField.Assign(Substring(buffer, 1, buffer.Length() - 1));

      state = STATE_PASSVALUE;
      break;

    case STATE_PASSVALUE:
      NS_ASSERTION(entry, "bad state");

      if (!decryptError) {
        if (NS_SUCCEEDED(DecryptData(buffer, decryptedValue)))
          entry->passValue.Assign(decryptedValue);
        else
          decryptError = PR_TRUE;  // abort this entry
      }

      state = STATE_USERFIELD;
      break;
    }
  } while (moreData);

  // Don't leak if the file ended unexpectedly
  delete entry;
}

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

struct writeSignonEntryContext {
  nsIOutputStream* stream;
  nsPasswordManager* manager;
};

/* static */ PLDHashOperator PR_CALLBACK
nsPasswordManager::WriteSignonEntryEnumerator(const nsACString& aKey,
                                              SignonDataEntry* aEntry,
                                              void* aUserData)
{
  writeSignonEntryContext* context = NS_STATIC_CAST(writeSignonEntryContext*,
                                                    aUserData);

  nsIOutputStream* stream = context->stream;
  nsPasswordManager* manager = context->manager;
  PRUint32 bytesWritten;

  nsCAutoString buffer(aKey);
  buffer.Append(NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  NS_ConvertUCS2toUTF8 userField(aEntry->userField);
  userField.Append(NS_LINEBREAK);
  stream->Write(userField.get(), userField.Length(), &bytesWritten);

  manager->EncryptData(aEntry->userValue, buffer);
  buffer.Append(NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  buffer.Assign("*");
  buffer.Append(NS_ConvertUCS2toUTF8(aEntry->passField));
  buffer.Append(NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  manager->EncryptData(aEntry->passValue, buffer);
  buffer.Append(NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  buffer.Assign("." NS_LINEBREAK);
  stream->Write(buffer.get(), buffer.Length(), &bytesWritten);

  return PL_DHASH_NEXT;
}

void
nsPasswordManager::WriteSignonFile()
{
  nsCOMPtr<nsIOutputStream> fileStream;
  NS_NewLocalFileOutputStream(getter_AddRefs(fileStream), mSignonFile);
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
  writeSignonEntryContext context = { fileStream, this };
  mSignonTable.EnumerateRead(WriteSignonEntryEnumerator, &context);
}

void
nsPasswordManager::AddSignonData(const nsACString& aRealm,
                                 SignonDataEntry* aEntry)
{
  // See if there is already an entry for this URL
  SignonDataEntry* oldEntry;
  if (mSignonTable.Get(aRealm, &oldEntry) && oldEntry) {
    // Add this one at the front of the linked list
    aEntry->next = oldEntry;
  }

  mSignonTable.Put(aRealm, aEntry);
}

nsresult
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
    if (!mDecoderRing) {
      NS_WARNING("Unable to get decoder ring service");
      return NS_ERROR_FAILURE;
    }

    if (NS_FAILED(mDecoderRing->DecryptString(flatData.get(), &buffer)))
      return NS_ERROR_FAILURE;

  }

  aPlaintext.Assign(NS_ConvertUTF8toUCS2(buffer));
  PR_Free(buffer);

  return NS_OK;
}

nsresult
nsPasswordManager::EncryptData(const nsAString& aPlaintext,
                               nsACString& aEncrypted)
{
  EnsureDecoderRing();
  NS_ENSURE_TRUE(mDecoderRing, NS_ERROR_FAILURE);

  char* buffer;
  if (NS_FAILED(mDecoderRing->EncryptString(NS_ConvertUCS2toUTF8(aPlaintext).get(), &buffer)))
    return NS_ERROR_FAILURE;

  aEncrypted.Assign(buffer);
  PR_Free(buffer);

  return NS_OK;
}

void
nsPasswordManager::EnsureDecoderRing()
{
  if (!mDecoderRing)
    mDecoderRing = do_GetService("@mozilla.org/security/sdr;1");
}

nsresult
nsPasswordManager::FindPasswordEntryFromSignonData(SignonDataEntry* aEntry,
                                                   const nsACString& aHost,
                                                   const nsAString&  aUser,
                                                   const nsAString&  aPassword,
                                                   nsACString& aHostFound,
                                                   nsAString&  aUserFound,
                                                   nsAString&  aPasswordFound)
{
  // host has already been checked, so just look for user/password match.
  SignonDataEntry* entry = aEntry;

  for (; entry; entry = entry->next) {
    PRBool userMatched = (aUser.IsEmpty() || aUser.Equals(entry->userValue));
    PRBool passMatched = (aPassword.IsEmpty() ||
                          aPassword.Equals(entry->passValue));

    if (userMatched && passMatched)
      break;
  }

  if (entry) {
    aHostFound.Assign(aHost);
    aUserFound.Assign(entry->userValue);
    aPasswordFound.Assign(entry->passValue);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

//////////////////////////////////////
// nsSingleSignonPrompt

NS_IMPL_ISUPPORTS2(nsSingleSignonPrompt,
                   nsIAuthPromptWrapper,
                   nsIAuthPrompt)

// nsIAuthPrompt
NS_IMETHODIMP
nsSingleSignonPrompt::Prompt(const PRUnichar* aDialogTitle,
                             const PRUnichar* aText,
                             const PRUnichar* aPasswordRealm,
                             PRUint32 aSavePassword,
                             const PRUnichar* aDefaultText,
                             PRUnichar** aResult,
                             PRBool* aConfirm)
{
  nsAutoString checkMsg;
  nsString emptyString;
  PRBool checkValue = (aSavePassword == SAVE_PASSWORD_PERMANENTLY);
  PRBool *checkPtr = nsnull;
  PRUnichar* value = nsnull;
  nsCOMPtr<nsIPasswordManagerInternal> mgrInternal;

  if (nsPasswordManager::SingleSignonEnabled()) {
    SatchelLocalizedString(NS_LITERAL_STRING("rememberValue"), checkMsg);
    checkPtr = &checkValue;

    mgrInternal = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
    nsCAutoString outHost;
    nsAutoString outUser, outPassword;

    mgrInternal->FindPasswordEntry(NS_ConvertUCS2toUTF8(aPasswordRealm),
                                   emptyString,
                                   emptyString,
                                   outHost,
                                   outUser,
                                   outPassword);

    value = ToNewUnicode(outUser);
  }

  mPrompt->Prompt(aDialogTitle,
                  aText,
                  &value,
                  checkMsg.get(),
                  checkPtr,
                  aConfirm);

  if (*aConfirm && checkValue && value[0] != '\0') {
    // The user requested that we save the value
    // TODO: support SAVE_PASSWORD_FOR_SESSION

    nsCOMPtr<nsIPasswordManager> manager = do_QueryInterface(mgrInternal);

    manager->AddUser(NS_ConvertUCS2toUTF8(aPasswordRealm),
                     nsDependentString(value),
                     emptyString);
  }

  if (value)
    nsMemory::Free(value);

  return NS_OK;
}

NS_IMETHODIMP
nsSingleSignonPrompt::PromptUsernameAndPassword(const PRUnichar* aDialogTitle,
                                                const PRUnichar* aText,
                                                const PRUnichar* aPasswordRealm,
                                                PRUint32 aSavePassword,
                                                PRUnichar** aUser,
                                                PRUnichar** aPassword,
                                                PRBool* aConfirm)
{
  nsAutoString checkMsg;
  nsString emptyString;
  PRBool checkValue = (aSavePassword == SAVE_PASSWORD_PERMANENTLY);
  PRBool *checkPtr = nsnull;
  PRUnichar *user = nsnull, *password = nsnull;
  nsCOMPtr<nsIPasswordManagerInternal> mgrInternal;

  if (nsPasswordManager::SingleSignonEnabled()) {
    SatchelLocalizedString(NS_LITERAL_STRING("rememberPassword"), checkMsg);
    checkPtr = &checkValue;

    mgrInternal = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
    nsCAutoString outHost;
    nsAutoString outUser, outPassword;

    mgrInternal->FindPasswordEntry(NS_ConvertUCS2toUTF8(aPasswordRealm),
                                   emptyString,
                                   emptyString,
                                   outHost,
                                   outUser,
                                   outPassword);

    user = ToNewUnicode(outUser);
    password = ToNewUnicode(outPassword);
  }

  mPrompt->PromptUsernameAndPassword(aDialogTitle,
                                     aText,
                                     &user,
                                     &password,
                                     checkMsg.get(),
                                     checkPtr,
                                     aConfirm);

  if (*aConfirm && checkValue && user[0] != '\0') {
    // The user requested that we save the values
    // TODO: support SAVE_PASSWORD_FOR_SESSION

    nsCOMPtr<nsIPasswordManager> manager = do_QueryInterface(mgrInternal);

    manager->AddUser(NS_ConvertUCS2toUTF8(aPasswordRealm),
                     nsDependentString(user),
                     nsDependentString(password));
  }

  if (user)
    nsMemory::Free(user);
  if (password)
    nsMemory::Free(password);

  return NS_OK;
}

NS_IMETHODIMP
nsSingleSignonPrompt::PromptPassword(const PRUnichar* aDialogTitle,
                                     const PRUnichar* aText,
                                     const PRUnichar* aPasswordRealm,
                                     PRUint32 aSavePassword,
                                     PRUnichar** aPassword,
                                     PRBool* aConfirm)
{
  nsAutoString checkMsg;
  nsString emptyString;
  PRBool checkValue = (aSavePassword == SAVE_PASSWORD_PERMANENTLY);
  PRBool *checkPtr = nsnull;
  PRUnichar* password = nsnull;
  nsCOMPtr<nsIPasswordManagerInternal> mgrInternal;

  if (nsPasswordManager::SingleSignonEnabled()) {
    SatchelLocalizedString(NS_LITERAL_STRING("rememberPassword"), checkMsg);
    checkPtr = &checkValue;

    mgrInternal = do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
    nsCAutoString outHost;
    nsAutoString outUser, outPassword;

    mgrInternal->FindPasswordEntry(NS_ConvertUCS2toUTF8(aPasswordRealm),
                                   emptyString,
                                   emptyString,
                                   outHost,
                                   outUser,
                                   outPassword);

    password = ToNewUnicode(outPassword);
  }

  mPrompt->PromptPassword(aDialogTitle,
                          aText,
                          &password,
                          checkMsg.get(),
                          checkPtr,
                          aConfirm);

  if (*aConfirm && checkValue && password[0] != '\0') {
    // The user requested that we save the password
    // TODO: support SAVE_PASSWORD_FOR_SESSION

    nsCOMPtr<nsIPasswordManager> manager = do_QueryInterface(mgrInternal);

    manager->AddUser(NS_ConvertUCS2toUTF8(aPasswordRealm),
                     emptyString,
                     nsDependentString(password));
  }

  if (password)
    nsMemory::Free(password);

  return NS_OK;
}

// nsIAuthPromptWrapper

NS_IMETHODIMP
nsSingleSignonPrompt::SetPromptDialogs(nsIPrompt* aDialogs)
{
  mPrompt = aDialogs;
  return NS_OK;
}

static void
SatchelLocalizedString(const nsAString& key,
                       nsAString& aResult,
                       PRBool aIsFormatted)
{
  if (!sSatchelBundle) {
    nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    bundleService->CreateBundle(kSatchelPropertiesURL,
                                &sSatchelBundle);

    if (!sSatchelBundle) {
      NS_ERROR("string bundle not present");
      return;
    }
  }

  nsXPIDLString str;
  if (aIsFormatted)
    sSatchelBundle->FormatStringFromName(PromiseFlatString(key).get(),
                                         nsnull, 0, getter_Copies(str));
  else
    sSatchelBundle->GetStringFromName(PromiseFlatString(key).get(),
                                      getter_Copies(str));
  aResult.Assign(str);
}
