/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package netscape.rdf;

/** 
 * An RDF Resource
 */
public final class Resource {

	private String resourceID;  
	private Object resourceObject;  

    // Hashtable that manages Resource to id mapping
    private static StringResourceHashtable mUnitHash = new StringResourceHashtable(1024);

    public static Resource instanceOf;
	public static Resource parent;
	public static Resource string;
	public static Resource integer;
	public static Resource resource;
	public static Resource url;
	public static Resource Class;
	public static Resource column;
	public static Resource domain;
	public static Resource range;
	public static Resource name;

    static {
        instanceOf = getResource("instanceOf", true);
        parent = getResource("parent", true);
        string = getResource("string", true);
        integer = getResource("integer", true);
        resource = getResource("resource", true);
        url = getResource("url", true);
        Class = getResource("Class", true);
        column = getResource("column", true);
        domain = getResource("domain", true);
        range = getResource("range", true);
        name = getResource("name", true);
    }

    /**
     * Get a named Resource.
     *
     * @param id
     *		a unique identifier for the resource
     * @param createp
     *		create the resource if it is not found
     * @return the named resource, or <code>null</code>
     */
    private Resource(String id) {
        resourceID = id;
    }

    public static Resource getResource(String id, boolean create) {
        Resource ret;
        
        synchronized(mUnitHash) {
            ret = (Resource)mUnitHash.get(id);
            if ( null == ret && create) {
                ret = new Resource(id);
                mUnitHash.put(id, ret);
            }
        }

        return ret;
    }

    /**
     * Get a named Resource.
     *
     * @param id
     *		a unique identifier for the resource
     * @return the named resource, or <code>null</code>
     */
    public static Resource getResource(String id) {
		return getResource(id, true);
    }

    /**
     * Returns the unique identifier associated with this resource.
     * 
     * @return	The unique name of this resource
     */
    public String getUniqueID() {
		return resourceID;
    }

    /**
     * Associates an arbitrary object with this Resource.
     * 
     * @param data
     *		An Object to be associated with this Resource, or <code>null</code>
     * @return	An object previously set with setObject, or <code>null</code>
     */
    public Object setObject(Object data) {
		Object prev = resourceObject;
		resourceObject = data;
        return prev;
    }

    /**
     * Returns any Object associated with the Resource
     * 
     * @return	An object previously set with setObject, or <code>null</code>
     */
    public Object getObject() {
		return resourceObject;
    }
}
