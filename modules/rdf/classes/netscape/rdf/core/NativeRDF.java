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

package netscape.rdf.core;
import netscape.rdf.*;

import java.util.Vector;
import java.util.Enumeration;

final class NativeRDFEnumeration implements Enumeration {
    private double cursor;      // native rdf cursor
    public native boolean hasMoreElements();
    public native Object nextElement();
}

/**
 * A native implementation of RDF
 */
final class NativeRDF implements IRDF { 

    //private double db;
    private String[] dbStores;
  	private native void NativeRDF0(String[] RDFStores);

	public NativeRDF(String[] RDFStores) {
		NativeRDF0(RDFStores);
        // XXX copy dbStores
    }
  
    public native Resource assert(Resource src, Resource arcLabel, Resource target);
    public native Resource assert(Resource src, Resource arcLabel, String target);
    private native Resource assert(Resource src, Resource arcLabel, int target);
    public Resource assert(Resource src, Resource arcLabel, Integer target) {
      	return assert(src, arcLabel, target.intValue());
    }

    public native Resource assertFalse(Resource src, Resource arcLabel, Resource target);
    public native Resource assertFalse(Resource src, Resource arcLabel, String target);
    private native Resource assertFalse(Resource src, Resource arcLabel, int target);
    public Resource assertFalse(Resource src, Resource arcLabel, Integer target) {
      	return assertFalse(src, arcLabel, target.intValue());
    }

    public native Resource unassert(Resource src, Resource arcLabel, Resource target);
    public native Resource unassert(Resource src, Resource arcLabel, String target);
    private native Resource unassert(Resource src, Resource arcLabel, int target);
    public Resource unassert(Resource src, Resource arcLabel, Integer target) {
      	return unassert(src, arcLabel, target.intValue());
    }

    public native boolean hasAssertion(Resource src, Resource arcLabel, Resource target, boolean isTrue);
    public native boolean hasAssertion(Resource src, Resource arcLabel, String target, boolean isTrue);
    private native boolean hasAssertion(Resource src, Resource arcLabel, int target, boolean isTrue);
    public boolean hasAssertion(Resource src, Resource arcLabel, Integer target, boolean isTrue) {
      	return hasAssertion(src, arcLabel, target.intValue(), isTrue);
    }

    public native Enumeration getTargets(Resource src, Resource arcLabel, boolean isTrue);
    public native Object getTarget(Resource src, Resource arcLabel, boolean isTrue);

    public native Enumeration getSources(Resource arcLabel, Resource target, boolean isTrue);
    public native Enumeration getSources(Resource arcLabel, String target, boolean isTrue);
    private native Enumeration getSources(Resource arcLabel, int target, boolean isTrue);
    public Enumeration getSources(Resource arcLabel, Integer target, boolean isTrue) {
      	return getSources(arcLabel, target.intValue(), isTrue);
    }

    public native Resource getSource(Resource arcLabel, Resource target, boolean isTrue);
    public native Resource getSource(Resource arcLabel, String target, boolean isTrue);
    private native Resource getSource(Resource arcLabel, int target, boolean isTrue);
    public Resource getSource(Resource arcLabel, Integer target, boolean isTrue) {
      	return getSource(arcLabel, target.intValue(), isTrue);
    }

    public native boolean addNotifiable(IRDFNotifiable obs, RDFEvent event);
    public native void deleteNotifiable(IRDFNotifiable obs, RDFEvent event);

    public String[] getRDFStores() {
        return dbStores;
    }
}
