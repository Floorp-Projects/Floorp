/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/*

  The RDF service interface. This is a singleton object, and should be
  obtained from the <tt>nsServiceManager</tt>.

  XXX I'm not particularly happy with the current APIs for named data
  sources. The idea is that you want "rdf:bookmarks" to always map to
  the "bookmarks" data store. The thing that bothers me now is that
  the implementation has to pre-load all of the data sources: that
  means you need to parse the bookmarks file, read the history, etc.,
  rather than doing this on-demand. With a little thought, it may be
  able to enormously improve these APIs so that pre-loading data
  sources is unnecessary.

 */

#ifndef nsIRDFService_h__
#define nsIRDFService_h__

#if 1 //defined(USE_XPIDL_INTERFACES)
#include "nsRDFInterfaces.h"
#else

#include "nscore.h"
#include "nsISupports.h"
#include "prtime.h"
class nsIRDFDataBase;
class nsIRDFDataSource;
class nsIRDFLiteral;
class nsIRDFResource;
class nsIRDFResourceFactory;
class nsIRDFDate;
class nsIRDFInt;

// {BFD05261-834C-11d2-8EAC-00805F29F370}
#define NS_IRDFSERVICE_IID \
{ 0xbfd05261, 0x834c, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }


class nsIRDFService : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFSERVICE_IID; return iid; }

    // Resource management routines

    /**
     * Construct an RDF resource from a single-byte URI. <tt>nsIRDFSerivce</tt>
     * caches resources that are in-use, so multiple calls to <tt>GetResource()</tt>
     * for the same <tt>uri</tt> will return identical pointers. FindResource
     * is used to find out whether there already exists a resource corresponding to that url.
     */
    NS_IMETHOD GetResource(const char* uri, nsIRDFResource** resource) = 0;
    NS_IMETHOD FindResource(const char* uri, nsIRDFResource** resource, PRBool* found) = 0;

    /**
     * Construct an RDF resource from a Unicode URI. This is provided
     * as a convenience method, allowing automatic, in-line C++
     * conversion from <tt>nsString</tt> objects. The <tt>uri</tt> will
     * be converted to a single-byte representation internally.
     */
    NS_IMETHOD GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource) = 0;

    /**
     * Construct an RDF literal from a Unicode string.
     */
    NS_IMETHOD GetLiteral(const PRUnichar* value, nsIRDFLiteral** literal) = 0;

    /**

     * Construct an RDF literal from a PRTime.
     */
    NS_IMETHOD GetDateLiteral(const PRTime value, nsIRDFDate** date) = 0;

    /**
     * Construct an RDF literal from an int.
     */
    NS_IMETHOD GetIntLiteral(const int32 value, nsIRDFInt** intLiteral) = 0;

    /**

     * Registers a resource with the RDF system, making it unique w.r.t.
     * GetResource.
     *
     * An implementation of nsIRDFResource should call this in its
     * Init() method if it wishes the resource to be globally unique
     * (which is usually the case).
     *
     * NOTE that the resource will <i>not</i> be ref-counted by the
     * RDF service: the assumption is that the resource implementation
     * will call nsIRDFService::UnregisterResource() when the last
     * reference to the resource is released.
     *
     * NOTE that the nsIRDFService implementation may choose to
     * maintain a reference to the resource's URI; therefore, the
     * resource implementation should ensure that the resource's URI
     * (accessable via nsIRDFResource::GetValue(const char* *aURI)) is
     * valid before calling RegisterResource(). Furthermore, the
     * resource implementation should ensure that this pointer
     * <i>remains</i> valid for the lifetime of the resource. (The
     * implementation of the resource cache in nsIRDFService uses the
     * URI maintained "internally" in the resource as a key into the
     * cache rather than copying the resource URI itself.)
     */
    NS_IMETHOD RegisterResource(nsIRDFResource* aResource, PRBool replace = PR_FALSE) = 0;

    /**

     * Called to notify the resource manager that a resource is no
     * longer in use. This method should only be called from the
     * destructor of a "custom" resource implementation to notify the
     * RDF service that the last reference to the resource has been
     * released, so the resource is no longer valid.
     *
     * NOTE. As mentioned in nsIRDFResourceFactory::CreateResource(),
     * the RDF service will use the result of
     * nsIRDFResource::GetValue() as a key into its cache. For this
     * reason, you must always un-cache the resource <b>before</b>
     * releasing the storage for the <tt>const char*</tt> URI.
     */
    NS_IMETHOD UnregisterResource(nsIRDFResource* aResource) = 0;

    // Data source management routines

    /**
     * Register a <i>named data source</i>. The RDF service will call
     * <tt>nsIRDFDataSource::GetURI()</tt> to determine the URI under
     * which to register the data source.
     *
     * Note that the data source will <i>not</i> be refcounted by the
     * RDF service! The assumption is that an RDF data source
     * registers with the service once it is initialized (via
     * <tt>nsIRDFDataSource::Init()</tt>), and unregisters when the
     * last reference to the data source is released.
     */
    NS_IMETHOD RegisterDataSource(nsIRDFDataSource* dataSource,
                                  PRBool replace = PR_FALSE) = 0;

    /**
     * Unregister a <i>named data source</i>. The RDF service will call
     * <tt>nsIRDFDataSource::GetURI()</tt> to determine the URI under which the
     * data source was registered.
     */
    NS_IMETHOD UnregisterDataSource(nsIRDFDataSource* dataSource) = 0;

    /**
     * Get the <i>named data source</i> corresponding to the URI. If a data
     * source has been registered via <tt>RegisterDataSource()</tt>, that
     * data source will be returned.
     *
     * If no data source is currently
     * registered for the specified URI, and a data source <i>constructor</i>
     * function has been registered via <tt>RegisterDatasourceConstructor()</tt>,
     * the RDF service will call the constructor to attempt to construct a
     * new data source. If construction is successful, the data source will
     * be initialized via <tt>nsIRDFDataSource::Init()</tt>.
     */ 
    NS_IMETHOD GetDataSource(const char* uri, nsIRDFDataSource** dataSource) = 0;

    /**
     * Create a database that contains the specified named data
     * sources. This is a convenience method whose functionality is
     * equivalent to (1) constructing a new nsIRDFDataBase object from
     * the repository, and (2) iteratively adding the named data sources
     * to it, in order.
     */ 
    NS_IMETHOD CreateDatabase(const char** uris, nsIRDFDataBase** dataBase) = 0;

    /**
     * Get the database aggregating all the stuff that Navigator sees as a default,
     * including a "local" store.
     */ 
    NS_IMETHOD CreateBrowserDatabase(nsIRDFDataBase** dataBase) = 0;

};

#endif

extern nsresult
NS_NewRDFService(nsIRDFService** result);

#endif // nsIRDFService_h__
