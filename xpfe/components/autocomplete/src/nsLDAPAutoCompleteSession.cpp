/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@netscape.com> (Original Author)
 *
 */


// Work around lack of conditional build logic in codewarrior's
// build system.  The MOZ_LDAP_XPCOM preprocessor symbol is only 
// defined on Mac because noone else needs this weirdness; thus 
// the XP_MAC check first.  This conditional encloses the entire
// file, so that in the case where the ldap option is turned off
// in the mac build, a dummy (empty) object will be generated.
//
#if !defined(XP_MAC) || defined(MOZ_LDAP_XPCOM)

#include "nsLDAPAutoCompleteSession.h"
#include "nsIComponentManager.h"
#include "nsIConsoleService.h"
#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "nsString.h"
#include "nsILDAPURL.h"
#include "nsXPIDLString.h"
#include "nspr.h"
#include "nsLDAP.h"

#ifdef PR_LOGGING
static PRLogModuleInfo *sLDAPAutoCompleteLogModule = 0;
#endif

NS_IMPL_ISUPPORTS3(nsLDAPAutoCompleteSession, nsIAutoCompleteSession, 
                   nsILDAPMessageListener, nsILDAPAutoCompleteSession)

nsLDAPAutoCompleteSession::nsLDAPAutoCompleteSession() :
    mState(UNBOUND)
{
    NS_INIT_ISUPPORTS();
}

nsLDAPAutoCompleteSession::~nsLDAPAutoCompleteSession()
{
}

/* void onStartLookup (in wstring searchString, in nsIAutoCompleteResults previousSearchResult, in nsIAutoCompleteListener listener); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnStartLookup(const PRUnichar *searchString, 
                                 nsIAutoCompleteResults *previousSearchResult,
                                         nsIAutoCompleteListener *listener)
{
    nsresult rv; // hold return values from XPCOM calls

#ifdef PR_LOGGING
    // initialize logging, if it hasn't been already
    //
    if (!sLDAPAutoCompleteLogModule) {
        sLDAPAutoCompleteLogModule = PR_NewLogModule("ldapautocomplete");

        NS_ABORT_IF_FALSE(sLDAPAutoCompleteLogModule, 
                          "failed to initialize ldapautocomplete log module");
    }
#endif

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnStartLookup entered\n"));
    
    if (!listener) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): NULL listener"
                 "passed in");
        return NS_ERROR_NULL_POINTER;
    } else {
        mListener = listener;   // save it for later callbacks
    }

    // ignore the empty string and strings with @ in them
    //
    if (searchString[0] == 0 || 
        nsLiteralString(searchString).FindChar(PRUnichar('@'), 0) 
        != kNotFound) {

        FinishAutoCompleteLookup(nsIAutoCompleteStatus::ignored);
        return NS_OK;
    } else {
        mSearchString = searchString;        // save it for later use
    }

    // make sure this was called appropriately.
    //
    if (mState == SEARCHING || mState == BINDING) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): called while "
                 "search already in progress; no lookup started.");
        listener->OnAutoComplete(0, nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // see if this is a narrow search that we could potentially do locally
    //
    if (previousSearchResult) {

        // get the string representing previous search results
        //
        nsXPIDLString prevSearchStr;

        rv = previousSearchResult->GetSearchString(
            getter_Copies(prevSearchStr));
        if ( NS_FAILED(rv) ) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): couldn't "
                     "get search string from previousSearchResult");
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_FAILURE;
        }

        // does the string actually contain anything?
        //
        if ( prevSearchStr.get() && prevSearchStr.get()[0]) {

            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSesion::OnStartLookup(): starting "
                    "narrowing search\n"));

            // XXXdmose for performance, we should really do a local, 
            // synchronous search against the existing dataset instead of 
            // just kicking off a new LDAP search here.  When implementing
            // this, need to be sure that only previous results which did not
            // hit the size limit and were successfully completed are used.
            //
            mState = SEARCHING;
            return StartLDAPSearch();
        }
    }

    // init connection if necesary
    //
    switch (mState) {
    case UNBOUND:

        // initialize the connection.  
        //
        rv = InitConnection();
        if (NS_FAILED(rv)) {

            // InitConnection() will have already called 
            // FinishAutoCompleteLookup for us as necessary
            //
            return rv;
        }

        return NS_OK;

    case BOUND:

        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
               ("nsLDAPAutoComplete::OnStartLookup(): subsequent search "
                "starting"));

        // kick off an LDAP search
        mState = SEARCHING;
        return StartLDAPSearch();

    case BINDING:
    case SEARCHING:
        // we should never get here
        NS_ERROR("nsLDAPAutoCompleteSession::OnStartLookup(): unexpected "
                 "value of mStatus");
        return NS_ERROR_UNEXPECTED;
    }
    
    return NS_ERROR_UNEXPECTED;     /*NOTREACHED*/
}

/* void onStopLookup (); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnStopLookup()
{
#ifdef PR_LOGGING
    // initialize logging, if it hasn't been already
    //
    if (!sLDAPAutoCompleteLogModule) {
        sLDAPAutoCompleteLogModule = PR_NewLogModule("ldapautocomplete");

        NS_ABORT_IF_FALSE(sLDAPAutoCompleteLogModule, 
                          "failed to initialize ldapautocomplete log module");
    }
#endif

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnStopLookup entered\n"));

    switch (mState) {
    case UNBOUND:
        // nothing to stop
        return NS_OK;

    case BOUND:
        // nothing to stop
        return NS_OK;

    case BINDING:
        // XXXdmose should this really be the same as searching?
        // waiting for nsILDAPService landing to see.  mState should at
        // least be set differently.

    case SEARCHING:
        nsresult rv = mOperation->Abandon();
        if (NS_FAILED(rv)) {
            // since there's nothing interesting that can or should be done
            // if this abandon failed, warn about it and move on
            //
            NS_WARNING("nsLDAPAutoCompleteSession::OnStopLookup(): "
                       "error calling mOperation->Abandon()");
        }

        // force nsCOMPtr to release mOperation
        //
        mOperation = 0;

        // reset the status
        //
        mState = BOUND;
    }

    mResultsArray = 0;
    mResults = 0;
    mListener = 0;

    return NS_OK;
}

/* void onAutoComplete (in wstring searchString, in nsIAutoCompleteResults previousSearchResult, in nsIAutoCompleteListener listener); */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnAutoComplete(const PRUnichar *searchString, 
                                 nsIAutoCompleteResults *previousSearchResult, 
                                          nsIAutoCompleteListener *listener)
{
    // OnStopLookup should have already been called, so there's nothing to
    // free here.  Additionally, as of this writing, noone is hanging around 
    // waiting for mListener->OnAutoComplete() to be called either, and if 
    // they were, it's unclear what we'd return, since we're not guaranteed 
    // to be in any particular state.  My suspicion is that this method 
    // (nsIAutoCompleteSession::OnAutoComplete) should probably be removed
    // from the interface.

    return NS_OK;
}

/**
 * Messages received are passed back via this function.
 *
 * @arg aMessage  The message that was returned, NULL if none was.
 *
 * void OnLDAPMessage (in nsILDAPMessage aMessage)
 */
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::OnLDAPMessage(nsILDAPMessage *aMessage)
{
    PRInt32 messageType;

    // just in case.
    // XXXdmose the semantics of NULL are currently undefined, but are likely
    // to be defined once we insert timeout handling code into the XPCOM SDK
    // At that time we should figure out if this still the right thing to do.
    // 
    if (!aMessage) {
        return NS_OK;
    }

    // figure out what sort of message was returned
    //
    nsresult rv = aMessage->GetType(&messageType);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsAutoCompleteSession::OnLDAPMessage(): unexpected error in "
                 "aMessage->GetType()");
       
        // don't call FinishAutoCompleteLookup(), as this could conceivably 
        // be an anomaly, and perhaps the next message will be ok.  if this
        // really was a problem, this search should eventually get
        // reaped by a timeout (once that code gets implemented).
        //
        return NS_ERROR_UNEXPECTED;
    }

    // XXXdmose - we may want a small state machine either here or
    // or in the nsLDAPConnection object, to make sure that things are 
    // happening in the right order and timing out appropriately.  this will
    // certainly depend on how timeouts are implemented, and how binding
    // gets is dealt with by nsILDAPService.  also need to deal with the case
    // where a bind callback happens after onStopLookup was called.
    // 
    switch (messageType) {

    case nsILDAPMessage::RES_BIND:

        // a bind has completed
        //
        return OnLDAPBind(aMessage);

    case nsILDAPMessage::RES_SEARCH_ENTRY:
        
        // ignore this if OnStopLookup was already called
        //
        // XXX one possible race condition still exists (bug 77452):
        // if another search has been started before everything from
        // the last search has been ignored.  Waiting for the patch in
        // bug 70422 to land, as this implements
        // nsILDAPMessage::GetOperation which will allow us to tell if
        // the incoming message is stale or not.
        //
        if (mState != SEARCHING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP search "
                    "entry returned after OnStopLoookup() called; ignoring"));
            return NS_OK;
        }

        // a search entry has been returned
        //
        return OnLDAPSearchEntry(aMessage);

    case nsILDAPMessage::RES_SEARCH_RESULT:

        // ignore this if OnStopLookup was already called
        //
        // XXX one possible race condition still exists (bug 77452):
        // if another search has been started before everything from
        // the last search has been ignored.  Waiting for the patch in
        // bug 70422 to land, as this implements
        // nsILDAPMessage::GetOperation which will allow us to tell if
        // the incoming message is stale or not.
        //
        if (mState != SEARCHING) {
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG,
                   ("nsLDAPAutoCompleteSession::OnLDAPMessage(): LDAP search "
                    "result returned after OnStopLookup called; ignoring"));
            return NS_OK;
        }

        // the search is finished; we're all done
        //  
        return OnLDAPSearchResult(aMessage);

    default:
        
        // Given the LDAP operations nsLDAPAutoCompleteSession uses, we should
        // never get here.  If we do get here in a release build, it's
        // probably a bug, but maybe it's the LDAP server doing something
        // weird.  Might as well try and continue anyway.  The session should
        // eventually get reaped by the timeout code, if necessary.
        //
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPMessage(): unexpected "
                 "LDAP message received");
        return NS_OK;
    }
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPBind(nsILDAPMessage *aMessage)
{

    PRInt32 errCode;

    mOperation = 0;  // done with bind op; make nsCOMPtr release it

    // get the status of the bind
    //
    nsresult rv = aMessage->GetErrorCode(&errCode);
    if (NS_FAILED(rv)) {
        
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPBind(): couldn't get "
                 "error code from aMessage");

        // reset to the default state
        //
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }


    // check to be sure the bind succeeded
    //
    if ( errCode != nsILDAPErrors::SUCCESS) {

        // XXXdmose should this be propagated to the UI somehow?
        //
        PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_WARNING, 
               ("nsLDAPAutoCompleteSession::OnLDAPBind(): error binding to "
                "LDAP server, errCode = %d", errCode));

        // reset to the default state
        //
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // ok, we're starting a search
    //
    mState = SEARCHING;

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPBind(): initial search "
            "starting\n"));

    return StartLDAPSearch();
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPSearchEntry(nsILDAPMessage *aMessage)
{
    // ldap attribute names used to fill in fields in nsIAutoCompleteItem
    //
    // XXXdmose need to get this from outputFormat
    //
    const char *valueField="mail";

    nsresult rv;        // temporary for return vals

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPSearchEntry entered\n"));

    // make sure this is only getting called after StartSearch has initialized
    // the result set
    //
    NS_ASSERTION(mResultsArray,
                 "nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                 "mResultsArrayItems is uninitialized");

    // create an nsIAutoCompleteItem to hold the returned value
    //
    nsCOMPtr<nsIAutoCompleteItem> item = do_CreateInstance(
        NS_AUTOCOMPLETEITEM_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchEntry couldn't"
                 " create " NS_AUTOCOMPLETEITEM_CONTRACTID "\n");

        // don't call FinishAutoCompleteLookup(), as this could conceivably 
        // be an anomaly, and perhaps the next message will be ok.  if this
        // really was a problem, this search should eventually get
        // reaped by a timeout (once that code gets implemented).
        //
        return NS_ERROR_FAILURE;
    }
        
    // get the attribute values for the field which will be used to fill in
    // nsIAutoCompleteItem::value
    //
    PRUint32 numVals;
    char **values;

    rv = aMessage->GetValues(valueField, &numVals, &values);
    if (NS_FAILED(rv)) {

        switch (rv) {
        case NS_ERROR_LDAP_DECODING_ERROR:
            // XXXdmose log error for non-debug builds?  could mean that this 
            // entry didn't have an attr named "email".
            //
            NS_WARNING("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                       "error decoding email attribute");
            return NS_ERROR_LDAP_DECODING_ERROR;

        case NS_ERROR_OUT_OF_MEMORY:
        case NS_ERROR_UNEXPECTED:
            return rv;

        default:
            NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                     "unexpected return code from aMessage->getValues()");
            return NS_ERROR_UNEXPECTED;
        }
    }

    // just use the first value for the email attribute; subsequent values
    // are ignored
    //
    rv = item->SetValue(NS_ConvertUTF8toUCS2(values[0]).GetUnicode());
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(numVals, values);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                 "item->SetValue failed");

        return NS_ERROR_FAILURE;
    }

    // XXXdmose process outputFormat here

    rv = mResultsArray->AppendElement(item);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchEntry(): "
                 "mItems->AppendElement() failed");
        return NS_ERROR_FAILURE;
    }

    // remember that something has been returned
    //
    mEntriesReturned++;

    return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::OnLDAPSearchResult(nsILDAPMessage *aMessage)
{
    nsresult rv;        // temp for return vals

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::OnLDAPSearchResult entered\n"));

    // figure out if we succeeded or failed, and set the status 
    // and default index appropriately
    //
    AutoCompleteStatus status;

    switch (mEntriesReturned) {

    case 0:
        // we could potentially keep track of non-fatal errors to the 
        // search, and if there has been more than 1, and there are no entries,
        // we could return |failed| instead of |noMatch|.  It's unclear to me
        // that this actually buys us anything though.
        //
        status = nsIAutoCompleteStatus::noMatch;
        break;

    case 1:
        status = nsIAutoCompleteStatus::matchFound;

        // there's only one match, so the default index should point to it
        //
        rv = mResults->SetDefaultItemIndex(0);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): "
                     "mResults->SetDefaultItemIndex(0) failed");
            mState = BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        }

        break;

    default:
        status = nsIAutoCompleteStatus::matchFound;

        // we must have more than one match, so the default index should be
        // unset (-1)
        //
        rv = mResults->SetDefaultItemIndex(-1);
        if (NS_FAILED(rv)) {
            NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): "
                     "mResults->SetDefaultItemIndex(-1) failed");
            mState = BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        }

        break;
    }

    // This seems to be necessary for things to work, though I'm not sure
    // why that's true.
    //
    rv = mResults->SetSearchString(mSearchString.GetUnicode());
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::OnLDAPSearchResult(): couldn't "
                 "set search string in results object");

        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);

        return NS_ERROR_FAILURE;
    }

    // XXXdmose just hang out with a bound connection forever?  waiting
    // for nsILDAPService landing to decide on this
    //
    mState = BOUND;

    // call the mListener's OnAutoComplete and clean up
    //
    FinishAutoCompleteLookup(status);

    return NS_OK;
}

nsresult
nsLDAPAutoCompleteSession::StartLDAPSearch()
{
    nsresult rv; // temp for xpcom return values
    nsCOMPtr<nsILDAPMessageListener> selfProxy; // for callback

    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession::StartLDAPSearch entered\n"));

    // create and initialize an LDAP operation (to be used for the search
    //  
    mOperation = 
        do_CreateInstance("@mozilla.org/network/ldap-operation;1", &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): couldn't "
                 "create @mozilla.org/network/ldap-operation;1");

        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // get a proxy object so the callback happens on the main thread
    //
    rv = NS_GetProxyForObject(NS_UI_THREAD_EVENTQ, 
                              NS_GET_IID(nsILDAPMessageListener), 
			      NS_STATIC_CAST(nsILDAPMessageListener *, this), 
                              PROXY_ASYNC | PROXY_ALWAYS, 
                              getter_AddRefs(selfProxy));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): couldn't "
                 "create proxy to this object for callback");
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // initialize the LDAP operation object
    //
    rv = mOperation->Init(mConnection, selfProxy);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch(): couldn't "
                 "initialize LDAP operation");

        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED;
    }

    // XXXdmose process the mFilterTemplate here; ensuring that it's been set.
    // This is waiting on substring replacement code, bug 74896.  Note that
    // we need to vet the input text for special LDAP filter chars lik
    // '|', '(', etc.
    //
    nsCAutoString searchFilter("(|(cn=");
    searchFilter.Append(NS_ConvertUCS2toUTF8(mSearchString));
    searchFilter.Append("*)(sn=");
    searchFilter.Append(NS_ConvertUCS2toUTF8(mSearchString));
    searchFilter.Append("*))");

    // create a result set 
    //
    mResults = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID, &rv);

    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch() couldn't"
                 " create " NS_AUTOCOMPLETERESULTS_CONTRACTID);
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // get a pointer to the array in question now, so that we don't have to
    // keep re-fetching it every time an entry callback happens
    //
    rv = mResults->GetItems(getter_AddRefs(mResultsArray));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::StartLDAPSearch() couldn't "
                 "get results array.");
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // nothing returned yet!
    //
    mEntriesReturned = 0;
    
    // get the base dn to search
    //
    nsXPIDLCString dn;
    rv = mServerURL->GetDn(getter_Copies(dn));
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED;
    }

    // and the scope
    //
    PRInt32 scope;
    rv = mServerURL->GetScope(&scope);
    if ( NS_FAILED(rv) ){
        mState = BOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED;
    }

    // time to kick off the search.
    //
    // XXXdmose what about timeouts? 
    // XXXdmose optimization: just request the attributes needed, not all 
    //          attributes. requires tweaking SearchExt.
    //
    rv = mOperation->SearchExt(dn, scope, 
                               searchFilter.get(),
                               0, nsILDAPOperation::NO_LIMIT);
    if (NS_FAILED(rv)) {
        switch(rv) {

        case NS_ERROR_LDAP_ENCODING_ERROR:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::StartSearch(): SearchExt "
                    "returned NS_ERROR_LDAP_ENCODING_ERROR"));

            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_OK;

        case NS_ERROR_LDAP_SERVER_DOWN:
            // XXXdmose discuss with leif how to handle this in general in the 
            // LDAP XPCOM SDK.  

            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::StartSearch(): SearchExt "
                    "returned NS_ERROR_LDAP_SERVER_DOWN"));

            mState=UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_OK;

        case NS_ERROR_OUT_OF_MEMORY:

            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_OUT_OF_MEMORY;

        case NS_ERROR_LDAP_NOT_SUPPORTED:
        case NS_ERROR_NOT_INITIALIZED:        
        case NS_ERROR_INVALID_ARG:
        default:

            // all this stuff indicates code bugs
            //
            NS_ERROR("nsLDAPAutoCompleteSession::StartSearch(): SearchExt "
                     "returned unexpected value");

            mState=BOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}

// XXXdmose - stopgap routine until nsLDAPService is working
//
nsresult
nsLDAPAutoCompleteSession::InitConnection()
{
    nsCOMPtr<nsILDAPMessageListener> selfProxy;
    nsresult rv;        // temp for xpcom return values
    
    // create an LDAP connection
    //
    mConnection = do_CreateInstance(
        "@mozilla.org/network/ldap-connection;1", &rv);
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitConnection(): could "
                 "not create @mozilla.org/network/ldap-connection;1");
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // have we been properly initialized?
    //
    if (!mServerURL) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitConnection(): mServerURL "
                 "is NULL");
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_NOT_INITIALIZED;
    }

    // host to connect to
    //
    nsXPIDLCString host;
    rv = mServerURL->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // on which port
    //
    PRInt32 port;
    rv = mServerURL->GetPort(&port);
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }
        
    rv = mConnection->Init(host, port, 0);
    if NS_FAILED(rv) {
        switch (rv) {

        case NS_ERROR_OUT_OF_MEMORY:
        case NS_ERROR_NOT_AVAILABLE:
        case NS_ERROR_FAILURE:
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return rv;

        case NS_ERROR_ILLEGAL_VALUE:
        default:
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return rv;
        }
    }

    // create and initialize an LDAP operation (to be used for the bind)
    //  
    mOperation = do_CreateInstance("@mozilla.org/network/ldap-operation;1", 
                                   &rv);
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // get a proxy object so the callback happens on the main thread
    //
    rv = NS_GetProxyForObject(NS_UI_THREAD_EVENTQ,
                              NS_GET_IID(nsILDAPMessageListener), 
                              NS_STATIC_CAST(nsILDAPMessageListener *, this), 
                              PROXY_ASYNC | PROXY_ALWAYS, 
                              getter_AddRefs(selfProxy));
    if (NS_FAILED(rv)) {
        NS_ERROR("nsLDAPAutoCompleteSession::InitLDAPConnection(): couldn't "
                 "create proxy to this object for callback");
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_FAILURE;
    }

    // our OnLDAPMessage accepts all result callbacks
    //
    rv = mOperation->Init(mConnection, selfProxy);
    if (NS_FAILED(rv)) {
        mState = UNBOUND;
        FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
        return NS_ERROR_UNEXPECTED; // this should never happen
    }

    // kick off a bind operation 
    // 
    PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
           ("nsLDAPAutoCompleteSession:InitConnection: initiating "
            "SimpleBind\n"));
    rv = mOperation->SimpleBind(NULL); 
    if (NS_FAILED(rv)) {

        switch (rv) {

        case NS_ERROR_LDAP_SERVER_DOWN:
            // XXXdmose try to rebind for this one?  wait for nsILDAPServer to 
            // see...
            //
        case NS_ERROR_LDAP_CONNECT_ERROR:
        case NS_ERROR_LDAP_ENCODING_ERROR:
        case NS_ERROR_OUT_OF_MEMORY:
            PR_LOG(sLDAPAutoCompleteLogModule, PR_LOG_DEBUG, 
                   ("nsLDAPAutoCompleteSession::InitConnection(): mSimpleBind "
                    "failed, rv = 0x%lx", rv));
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_OK;

        case NS_ERROR_UNEXPECTED:
        default:
            mState = UNBOUND;
            FinishAutoCompleteLookup(nsIAutoCompleteStatus::failed);
            return NS_ERROR_UNEXPECTED;
        }
    }

    // set our state
    //
    mState = BINDING;

    return NS_OK;
}

void
nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(AutoCompleteStatus 
                                                    aACStatus)
{
    nsresult rv; // temp for return value 

    // inform the listener that the search is over
    //
    if (aACStatus == nsIAutoCompleteStatus::matchFound) {
        rv = mListener->OnAutoComplete(mResults, aACStatus);
    } else {
        rv = mListener->OnAutoComplete(0, aACStatus);
    }
    if (NS_FAILED(rv)) {

        // there's nothing we can actually do here other than warn
        //
        NS_WARNING("nsLDAPAutoCompleteSession::FinishAutoCompleteLookup(): "
                   "error calling mListener->OnAutoComplete()");
    }

    // we're done with various things; cause nsCOMPtr to release them
    //
    mResultsArray = 0;
    mResults = 0;
    mListener = 0;
    mOperation = 0;
}

// methods for nsILDAPAutoCompleteSession

// attribute wstring searchFilter;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetFilterTemplate(PRUnichar * *aFilterTemplate)
{
    if (!aFilterTemplate) {
        return NS_ERROR_NULL_POINTER;
    }

    *aFilterTemplate = mFilterTemplate.ToNewUnicode();
    if (!*aFilterTemplate) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetFilterTemplate(const PRUnichar * aFilterTemplate)
{
    mFilterTemplate = aFilterTemplate;

    return NS_OK;
}

// attribute wstring outputFormat;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetOutputFormat(PRUnichar * *aOutputFormat)
{
    if (!aOutputFormat) {
        return NS_ERROR_NULL_POINTER;
    }

    *aOutputFormat = mOutputFormat.ToNewUnicode();
    if (!*aOutputFormat) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetOutputFormat(const PRUnichar * aOutputFormat)
{
    mOutputFormat = aOutputFormat;

    return NS_OK;
}

// attribute long sizeLimit;
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetSizeLimit(PRInt32 *aSizeLimit)
{
    if (!aSizeLimit) {
        return NS_ERROR_NULL_POINTER;
    }

    *aSizeLimit = mSizeLimit;
    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetSizeLimit(PRInt32 aSizeLimit)
{
    mSizeLimit = aSizeLimit;
    return NS_OK;
}

// attribute nsILDAPURL serverURL; 
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::GetServerURL(nsILDAPURL * *aServerURL)
{
    if (! aServerURL ) {
        return NS_ERROR_NULL_POINTER;
    }
    
    *aServerURL = mServerURL;
    NS_IF_ADDREF(*aServerURL);

    return NS_OK;
}
NS_IMETHODIMP 
nsLDAPAutoCompleteSession::SetServerURL(nsILDAPURL * aServerURL)
{
    if (! aServerURL ) {
        return NS_ERROR_NULL_POINTER;
    }

    mServerURL = aServerURL;

    return NS_OK;
}

#endif
