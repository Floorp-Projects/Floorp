/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "rdf.h"

#include "nspr.h"
#include "plhash.h"

PR_BEGIN_EXTERN_C
PR_PUBLIC_API(void) _comwrap_NotificationCB(RDF_Event event, void* pdata);
PR_END_EXTERN_C

class rdfDataBaseWrapper;
class rdfCursorWrapper;
class rdfServiceWrapper;
class rdfServiceFactory;

static nsIRDFService* gRDFServiceSingleton = 0;

class rdfDatabaseWrapper : public nsIRDFDataBase {
public:
  NS_DECL_ISUPPORTS

  rdfDatabaseWrapper(RDF r);
  virtual ~rdfDatabaseWrapper();

  /* nsIRDFDataSource methods:  */

  NS_METHOD GetResource(RDF_String id,
                         RDF_Resource * r);

  NS_METHOD CreateResource(RDF_String id,
                            RDF_Resource* r);

  NS_METHOD ReleaseResource(RDF_Resource r);

  NS_METHOD GetName(const RDF_String* name /* out */ );

  NS_METHOD GetSource(RDF_Node target,
                      RDF_Resource arcLabel,
                      RDF_Resource *source /* out */);

  NS_METHOD GetSource(RDF_Node target,
                      RDF_Resource arcLabel,
                      PRBool tv,
                      RDF_Resource *source /* out */);

  NS_METHOD GetSources(RDF_Resource target,
                       RDF_Resource arcLabel,
                       nsIRDFCursor **sources /* out */);

  NS_METHOD GetSources(RDF_Resource target,
                       RDF_Resource arcLabel,
                       PRBool tv,
                       nsIRDFCursor **sources /* out */);

  NS_METHOD GetTarget(RDF_Resource source,
                      RDF_Resource arcLabel,
                      RDF_ValueType targetType,
                      RDF_NodeStruct& target /* in/out */);

  NS_METHOD GetTarget(RDF_Resource source,
                      RDF_Resource arcLabel,
                      RDF_ValueType targetType,
                      PRBool tv,
                      RDF_NodeStruct& target /* in/out */);

  NS_METHOD GetTargets(RDF_Resource source,
                       RDF_Resource arcLabel,
                       RDF_ValueType targetType,
                       nsIRDFCursor **targets /* out */);

  NS_METHOD GetTargets(RDF_Resource source,
                       RDF_Resource arcLabel,
                       PRBool tv,
                       RDF_ValueType targetType,
                       nsIRDFCursor **targets /* out */);

  NS_METHOD Assert(RDF_Resource source, 
                   RDF_Resource arcLabel, 
                   RDF_Node target,
                   PRBool tv = PR_TRUE);

  NS_METHOD Unassert(RDF_Resource source,
                     RDF_Resource arcLabel,
                     RDF_Node target);

  NS_METHOD HasAssertion(RDF_Resource source,
                         RDF_Resource arcLabel,
                         RDF_Node target,
                         PRBool tv,
                         PRBool* hasAssertion /* out */);

  NS_METHOD AddObserver(nsIRDFObserver *n,
                        RDF_EventMask type = RDF_ANY_NOTIFY);

  NS_METHOD RemoveObserver(nsIRDFObserver *n,
                           RDF_EventMask = RDF_ANY_NOTIFY);

  NS_METHOD ArcLabelsIn(RDF_Node node,
                        nsIRDFCursor **labels /* out */);

  NS_METHOD ArcLabelsOut(RDF_Resource source,
                         nsIRDFCursor **labels /* out */);

  NS_METHOD Flush();

  /* nsIRDFDataBase methods:   */
  NS_METHOD AddDataSource(nsIRDFDataSource* dataSource);

  NS_METHOD RemoveDataSource(nsIRDFDataSource* dataSource);

  NS_METHOD GetDataSource(RDF_String url,
                          nsIRDFDataSource **source /* out */ );

  NS_METHOD DeleteAllArcs(RDF_Resource resource);

private:
  RDF mRDF;

  PLHashTable* mpObserverMap;
};

class rdfCursorWrapper : public nsIRDFCursor {
public:
  NS_DECL_ISUPPORTS
  
  rdfCursorWrapper(RDF_Cursor c);
  virtual ~rdfCursorWrapper();

  NS_METHOD HasElements(PRBool& hasElements);

  NS_METHOD Next(RDF_NodeStruct& n);

private:
  RDF_Cursor mCursor;
  PRBool mHasCached;
  RDF_NodeStruct mNextValue;
};

class rdfServiceWrapper : public nsIRDFService {
public:
  NS_DECL_ISUPPORTS
  
  rdfServiceWrapper();
  ~rdfServiceWrapper();

  NS_METHOD CreateDatabase(const char** url,
                           nsIRDFDataBase** db);

  NS_METHOD ResourceIdentifier(RDF_Resource r,
                            char**   url /* out */);
#ifdef MOZILLA_CLIENT
  NS_METHOD SetBookmarkFile(const char* bookmarkFilePath);
#endif
};



class rdfServiceFactory : public nsIFactory {
public:
  NS_DECL_ISUPPORTS

  rdfServiceFactory();
  virtual ~rdfServiceFactory();

  NS_METHOD CreateInstance(nsISupports *aOuter,
                           REFNSIID anIID,
                           void **aResult);

  NS_METHOD LockFactory(PRBool aLock);

};

/*
  
  rdfDataBaseWrapper:

*/

NS_IMPL_ISUPPORTS( rdfDatabaseWrapper, NS_IRDFDATABASE_IID )

rdfDatabaseWrapper::rdfDatabaseWrapper(RDF r) : mRDF(r)
{
  mpObserverMap = PL_NewHashTable( 100,
                                   NULL, // XXX isn't there are hash fn for pointers???
                                   PL_CompareValues,
                                   PL_CompareValues,
                                   0,
                                   0 );

  PR_ASSERT( mpObserverMap );
#ifdef XXX
  if( !mpObserverMap ) // XXX just like 'new' failing on this object?
    throw bad_alloc("rdf: unable to allocate observer map" );
#endif

}

rdfDatabaseWrapper::~rdfDatabaseWrapper()
{
  PL_HashTableDestroy( mpObserverMap );
}

NS_METHOD
rdfDatabaseWrapper::GetResource(RDF_String id,
                         RDF_Resource * r)
{
  PR_ASSERT( 0 != r );
  if( 0 == r )
    return NS_ERROR_NULL_POINTER;

  *r = 0;

  *r = RDF_GetResource( mRDF, id, PR_FALSE );

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::CreateResource(RDF_String id,
                            RDF_Resource* r)
{
  PR_ASSERT( 0 != r );
  if( 0 == r )
    return NS_ERROR_NULL_POINTER;

  *r = 0;

  *r = RDF_GetResource( mRDF, id, PR_TRUE );
  if( 0 == *r ) { // XXX
    PR_ASSERT( PR_FALSE );
    return NS_ERROR_BASE;
  }

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::ReleaseResource(RDF_Resource r)
{
  RDF_ReleaseResource( mRDF, r );
  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::GetName(const RDF_String* name /* out */ )
{
  PR_ASSERT( PR_FALSE );
  return NS_ERROR_NOT_IMPLEMENTED; // XXX
}

NS_METHOD
rdfDatabaseWrapper::GetSource(RDF_Node target,
                              RDF_Resource arcLabel,
                              RDF_Resource *source /* out */)
{
  PR_ASSERT( target && source );
  *source = (RDF_Resource) RDF_GetSlotValue( mRDF,
                                             target->value.r,
                                             arcLabel,
                                             RDF_RESOURCE_TYPE, // anything else makes no sense
                                             PR_TRUE,
                                             PR_TRUE );

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::GetSource(RDF_Node target,
                              RDF_Resource arcLabel,
                              PRBool tv,
                              RDF_Resource *source /* out */)
{
  *source = (RDF_Resource) RDF_GetSlotValue( mRDF,
                                             target->value.r,
                                             arcLabel,
                                             RDF_RESOURCE_TYPE, // anything else makes no sense
                                             PR_TRUE,
                                             tv );

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::GetSources(RDF_Resource target,
                               RDF_Resource arcLabel,
                               nsIRDFCursor **sources /* out */)
{
  return GetSources(target,arcLabel,PR_TRUE,sources);
}

NS_METHOD
rdfDatabaseWrapper::GetSources(RDF_Resource target,
                               RDF_Resource arcLabel,
                               PRBool tv,
                               nsIRDFCursor **sources /* out */)
{
  PR_ASSERT( sources );
  if( 0 == sources )
    return NS_ERROR_NULL_POINTER;

  *sources = 0;

  RDF_Cursor c = RDF_GetSources( mRDF,
                                 target,
                                 arcLabel,
                                 RDF_RESOURCE_TYPE, // anything else makes no sense
                                 tv );

  if( c ) {
    *sources = new rdfCursorWrapper( c );
    (*sources)->AddRef();
  }

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::GetTarget(RDF_Resource source,
                              RDF_Resource arcLabel,
                              RDF_ValueType targetType,
                              RDF_NodeStruct& target /* in/out */)
{
  return GetTarget(source,arcLabel,targetType,PR_TRUE,target);
}

NS_METHOD
rdfDatabaseWrapper::GetTarget(RDF_Resource source,
                              RDF_Resource arcLabel,
                              RDF_ValueType targetType,
                              PRBool tv,
                              RDF_NodeStruct& target /* in/out */)
{
  PR_ASSERT( targetType != RDF_ANY_TYPE ); // not ready to support this yet

  void* value  = RDF_GetSlotValue( mRDF,
                                   target.value.r,
                                   arcLabel,
                                   targetType, // anything else makes no sense
                                   PR_FALSE,
                                   tv );

  target.type = targetType;
  target.value.r = (RDF_Resource) value; // reasonable? XXX

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::GetTargets(RDF_Resource source,
                               RDF_Resource arcLabel,
                               RDF_ValueType targetType,
                               nsIRDFCursor **targets /* out */)
{
  return GetTargets(source,arcLabel,PR_TRUE,targetType,targets);
}

NS_METHOD
rdfDatabaseWrapper::GetTargets(RDF_Resource source,
                               RDF_Resource arcLabel,
                               PRBool tv,
                               RDF_ValueType targetType,
                               nsIRDFCursor **targets /* out */)
{
  PR_ASSERT( targets );
  if( 0 == targets )
    return NS_ERROR_NULL_POINTER;

  *targets = 0;

  RDF_Cursor c = RDF_GetTargets( mRDF,
                                 source,
                                 arcLabel,
                                 targetType,
                                 tv );

  if( c ) {
    *targets = new rdfCursorWrapper( c );
    (*targets)->AddRef();
  }

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::Assert(RDF_Resource source, 
                           RDF_Resource arcLabel, 
                           RDF_Node target,
                           PRBool tv)
{
  PRBool b = tv ? RDF_Assert( mRDF, source, arcLabel, (void*)target->value.r, target->type ) :
    RDF_AssertFalse( mRDF, source, arcLabel, (void*)target->value.r, target->type );

  // XXX
  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::Unassert(RDF_Resource source,
                             RDF_Resource arcLabel,
                             RDF_Node target)
{
  PRBool b = RDF_Unassert( mRDF, 
                           source, 
                           arcLabel, 
                           target->value.r, 
                           target->type ); // XXX

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::HasAssertion(RDF_Resource source,
                                 RDF_Resource arcLabel,
                                 RDF_Node target,
                                 PRBool truthValue,
                                 PRBool* hasAssertion /* out */)
{
  *hasAssertion = RDF_HasAssertion( mRDF,
                                    source,
                                    arcLabel,
                                    target->value.r,
                                    target->type,
                                    truthValue );
  
  return NS_OK;
}

PR_IMPLEMENT(void)
  _comwrap_NotificationCB(RDF_Event event, void* pdata)
{
  nsIRDFObserver* observer = (nsIRDFObserver*) pdata;
  // XXX QueryInterface & release??
  observer->HandleEvent( (nsIRDFDataSource*)pdata, event );
}

NS_METHOD
rdfDatabaseWrapper::AddObserver(nsIRDFObserver *observer,
                                RDF_EventMask type)
{
  // XXX event masking does not currently work

  RDF_Notification notification = (RDF_Notification) PL_HashTableLookup( mpObserverMap, observer );
  if( !notification ) {
    observer->AddRef();
    notification = RDF_AddNotifiable( mRDF,
                                      _comwrap_NotificationCB,
                                      NULL, // XXX
                                      observer );
    PL_HashTableAdd( mpObserverMap,
                     observer,
                     notification );
  }

  return NS_OK; // XXX
}

NS_METHOD
rdfDatabaseWrapper::RemoveObserver(nsIRDFObserver *observer,
                                   RDF_EventMask type)
{

  RDF_Notification notification = (RDF_Notification) PL_HashTableLookup( mpObserverMap, observer );
  if( !notification )
    return NS_ERROR_ILLEGAL_VALUE;
  
  RDF_Error err = RDF_DeleteNotifiable( notification );
  PR_ASSERT( !err ); // the current implementation never fails!
  PL_HashTableRemove( mpObserverMap, observer );
  observer->Release();
    
  return NS_OK; // XXX
}

NS_METHOD
rdfDatabaseWrapper::ArcLabelsIn(RDF_Node node,
                                nsIRDFCursor **labels /* out */)
{
  PR_ASSERT( labels );
  if( 0 == labels )
    return NS_ERROR_NULL_POINTER;
  *labels = 0;

  RDF_Cursor c = RDF_ArcLabelsIn( mRDF, node->value.r );

  if( c ) {
    *labels = new rdfCursorWrapper( c );
    (*labels)->AddRef();
  }

  return NS_OK;

}

NS_METHOD
rdfDatabaseWrapper::ArcLabelsOut(RDF_Resource source,
                                 nsIRDFCursor **labels /* out */)
{
  PR_ASSERT( labels );
  if( 0 == labels )
    return NS_ERROR_NULL_POINTER;
  *labels = 0;

  RDF_Cursor c = RDF_ArcLabelsOut( mRDF, source );

  if( c ) {
    *labels = new rdfCursorWrapper( c );
    (*labels)->AddRef();
  }

  return NS_OK;
}

NS_METHOD
rdfDatabaseWrapper::Flush()
{
  return NS_ERROR_NOT_IMPLEMENTED; // XXX
}


NS_METHOD
rdfDatabaseWrapper::DeleteAllArcs(RDF_Resource resource)
{
  return RDF_DeleteAllArcs( mRDF, resource );
}

/*
  
  rdfServiceWrapper:  the RDF service singleton

*/


NS_IMPL_ISUPPORTS(rdfServiceWrapper, NS_IRDFSERVICE_IID )

rdfServiceWrapper::rdfServiceWrapper()
{
  nsresult err = RDF_Init(); // this method currently cannot fail
  PR_ASSERT( err == NS_OK ); // XXX
}

rdfServiceWrapper::~rdfServiceWrapper()
{
  nsresult err = RDF_Shutdown(); // this method currently cannot fail
  PR_ASSERT( err == NS_OK ); // XXX
}

NS_METHOD
rdfServiceWrapper::CreateDatabase(const char** url_ary,
                                  nsIRDFDataBase **db)
{
  PR_ASSERT( 0 != db );
  if( 0 == db )
    return NS_ERROR_NULL_POINTER;

  *db = 0;

  nsresult result = NS_OK;
  RDF rdf = RDF_GetDB(url_ary);

  if( 0 == rdf ) {
    result = RDF_ERROR_UNABLE_TO_CREATE; // XXX this is too wishy-washy
  } else {
    *db = new rdfDatabaseWrapper(rdf); // XXX
  }

  return result;
}

NS_METHOD
rdfServiceWrapper::ResourceIdentifier(RDF_Resource r,
                                      char** url)
{
  char* resourceID(RDF_Resource r);
  PR_ASSERT( 0 != r );
  if( 0 == r )
    return NS_ERROR_NULL_POINTER;
  *url = RDF_ResourceID(r);
  return NS_OK;
}

#ifdef MOZILLA_CLIENT
NS_METHOD 
rdfServiceWrapper::SetBookmarkFile(const char* bookmarkFilePath) {
  SetBookmarkURL(bookmarkFilePath);
  return NS_OK;
}
#endif  


/*

  rdfCursorWrapper

*/

NS_IMPL_ISUPPORTS( rdfCursorWrapper, NS_IRDFCURSOR_IID )

rdfCursorWrapper::rdfCursorWrapper(RDF_Cursor c) : mCursor(c) 
{
  mHasCached = PR_FALSE;
}

rdfCursorWrapper::~rdfCursorWrapper() 
{ 
  RDF_DisposeCursor( mCursor ); 
}

NS_METHOD 
rdfCursorWrapper::HasElements(PRBool& hasElements)
{
  mNextValue.type = RDF_CursorValueType( mCursor );
  mNextValue.value.r = (RDF_Resource) RDF_NextValue( mCursor );

  if( mNextValue.value.r != NULL ) {
    hasElements = PR_FALSE;
    mHasCached = PR_TRUE;
  } else {
    hasElements = PR_TRUE;
    mHasCached = PR_FALSE;
  }

  return NS_OK;
}

NS_METHOD
rdfCursorWrapper::Next(RDF_NodeStruct& next)
{
  if( mHasCached == PR_TRUE ) {
    next.type = mNextValue.type;
    next.value.r = mNextValue.value.r;
    mHasCached = PR_FALSE;
  } else {
    next.type = RDF_CursorValueType( mCursor );
    next.value.r = (RDF_Resource) RDF_NextValue( mCursor );
  }

  return NS_OK;
}

/*

  rdfServiceFactory

*/

NS_IMPL_ISUPPORTS( rdfServiceFactory, NS_IFACTORY_IID )

NS_METHOD
rdfServiceFactory::CreateInstance( nsISupports *aOuter,
                                   REFNSIID aIID,
                                   void **aResult )
{
  PR_ASSERT( aResult );
  if( 0 == aResult )
    return NS_ERROR_NULL_POINTER;
  *aResult = 0;

  nsISupports* instance = new rdfServiceWrapper();
    
  nsresult result = instance->QueryInterface( aIID, aResult );
  PR_ASSERT( result = NS_OK );

  if( result != NS_OK )
    delete instance; // wrong interface!

  return result;
}

NS_METHOD
rdfServiceFactory::LockFactory(PRBool lock)
{
  PR_ASSERT( PR_FALSE );
  return NS_ERROR_NOT_IMPLEMENTED;
}

PR_IMPLEMENT(nsresult)
NS_GetRDFService(nsIRDFService **s)
{
  PR_ASSERT( s );
  if( 0 == s )
    return NS_ERROR_NULL_POINTER;

  *s = 0;

  if( 0 == gRDFServiceSingleton ) {
    gRDFServiceSingleton = new rdfServiceWrapper(); // XXX use factory, check for errors
  }

  *s = gRDFServiceSingleton;

  return NS_OK;
}
