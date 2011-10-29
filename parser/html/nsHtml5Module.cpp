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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

#include "nsHtml5AttributeName.h"
#include "nsHtml5ElementName.h"
#include "nsHtml5HtmlAttributes.h"
#include "nsHtml5NamedCharacters.h"
#include "nsHtml5Portability.h"
#include "nsHtml5StackNode.h"
#include "nsHtml5Tokenizer.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5UTF16Buffer.h"
#include "nsHtml5Module.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

// static
bool nsHtml5Module::sEnabled = true;
bool nsHtml5Module::sOffMainThread = true;
nsIThread* nsHtml5Module::sStreamParserThread = nsnull;
nsIThread* nsHtml5Module::sMainThread = nsnull;

// static
void
nsHtml5Module::InitializeStatics()
{
  Preferences::AddBoolVarCache(&sOffMainThread, "html5.offmainthread");
  nsHtml5Atoms::AddRefAtoms();
  nsHtml5AttributeName::initializeStatics();
  nsHtml5ElementName::initializeStatics();
  nsHtml5HtmlAttributes::initializeStatics();
  nsHtml5NamedCharacters::initializeStatics();
  nsHtml5Portability::initializeStatics();
  nsHtml5StackNode::initializeStatics();
  nsHtml5Tokenizer::initializeStatics();
  nsHtml5TreeBuilder::initializeStatics();
  nsHtml5UTF16Buffer::initializeStatics();
  nsHtml5StreamParser::InitializeStatics();
#ifdef DEBUG
  sNsHtml5ModuleInitialized = true;
#endif
}

// static
void
nsHtml5Module::ReleaseStatics()
{
#ifdef DEBUG
  sNsHtml5ModuleInitialized = false;
#endif
  nsHtml5AttributeName::releaseStatics();
  nsHtml5ElementName::releaseStatics();
  nsHtml5HtmlAttributes::releaseStatics();
  nsHtml5NamedCharacters::releaseStatics();
  nsHtml5Portability::releaseStatics();
  nsHtml5StackNode::releaseStatics();
  nsHtml5Tokenizer::releaseStatics();
  nsHtml5TreeBuilder::releaseStatics();
  nsHtml5UTF16Buffer::releaseStatics();
  NS_IF_RELEASE(sStreamParserThread);
  NS_IF_RELEASE(sMainThread);
}

// static
already_AddRefed<nsIParser>
nsHtml5Module::NewHtml5Parser()
{
  NS_ABORT_IF_FALSE(sNsHtml5ModuleInitialized, "nsHtml5Module not initialized.");
  nsIParser* rv = static_cast<nsIParser*> (new nsHtml5Parser());
  NS_ADDREF(rv);
  return rv;
}

// static
nsresult
nsHtml5Module::Initialize(nsIParser* aParser, nsIDocument* aDoc, nsIURI* aURI, nsISupports* aContainer, nsIChannel* aChannel)
{
  NS_ABORT_IF_FALSE(sNsHtml5ModuleInitialized, "nsHtml5Module not initialized.");
  nsHtml5Parser* parser = static_cast<nsHtml5Parser*> (aParser);
  return parser->Initialize(aDoc, aURI, aContainer, aChannel);
}

class nsHtml5ParserThreadTerminator : public nsIObserver
{
  public:
    NS_DECL_ISUPPORTS
    nsHtml5ParserThreadTerminator(nsIThread* aThread)
      : mThread(aThread)
    {}
    NS_IMETHODIMP Observe(nsISupports *, const char *topic, const PRUnichar *)
    {
      NS_ASSERTION(!strcmp(topic, "xpcom-shutdown-threads"), 
                   "Unexpected topic");
      if (mThread) {
        mThread->Shutdown();
        mThread = nsnull;
      }
      return NS_OK;
    }
  private:
    nsCOMPtr<nsIThread> mThread;
};

NS_IMPL_ISUPPORTS1(nsHtml5ParserThreadTerminator, nsIObserver)

// static 
nsIThread*
nsHtml5Module::GetStreamParserThread()
{
  if (sOffMainThread) {
    if (!sStreamParserThread) {
      NS_NewThread(&sStreamParserThread);
      NS_ASSERTION(sStreamParserThread, "Thread creation failed!");
      nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
      NS_ASSERTION(os, "do_GetService failed");
      os->AddObserver(new nsHtml5ParserThreadTerminator(sStreamParserThread), 
                      "xpcom-shutdown-threads",
                      false);
    }
    return sStreamParserThread;
  }
  if (!sMainThread) {
    NS_GetMainThread(&sMainThread);
    NS_ASSERTION(sMainThread, "Main thread getter failed");
  }
  return sMainThread;
}

#ifdef DEBUG
bool nsHtml5Module::sNsHtml5ModuleInitialized = false;
#endif
