/* 
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
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

package org.mozilla.jss.util;

import org.mozilla.jss.util.*;
import java.util.Hashtable;
import java.util.Random;
import java.util.Enumeration;

/**
 * NativeProxy, a superclass for Java classes that mirror C data structures.
 *
 * It contains some code to help make sure that native memory is getting
 * freed properly.
 *
 * @author nicolson
 * @version $Revision: 1.3 $ $Date: 2002/05/02 04:04:23 $ 
 */
public abstract class NativeProxy
{

    /**
     * Default constructor. Should not be called.
     */
    private NativeProxy() {
        Assert._assert(false);
    }

    /**
     * Create a NativeProxy from a byte array representing a C pointer.
     * This is the only way to create a NativeProxy, it should be called
     * from the constructor of your subclass.
     *
     * @param pointer A byte array, created with JSS_ptrToByteArray, that
     * contains a pointer pointing to a native data structure.  The
     * NativeProxy instance acts as a proxy for that native data structure.
     */
    public NativeProxy(byte[] pointer) {
		Assert._assert(pointer!=null);
        if(Debug.DEBUG) {
            registryIndex = register();
        }
        mPointer = pointer;
    }

    /**
     * Deep comparison operator.
     * @return true if <code>obj</code> has the same underlying native
     *      pointer. false if the <code>obj</code> is null or has
     *      a different underlying native pointer.
     */
    public boolean equals(Object obj) {
        if(obj==null) {
            return false;
        }
        if( ! (obj instanceof NativeProxy) ) {
            return false;
        }
        if( ((NativeProxy)obj).mPointer.length != mPointer.length) {
            return false;
        }
        for(int i=0; i < mPointer.length; i++) {
            if(mPointer[i] != ((NativeProxy)obj).mPointer[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * Release the native resources used by this proxy.
     * Subclasses of NativeProxy must define this method to clean up
     * data structures in C code that are referenced by this proxy.
     * releaseNativeResources() will usually be implemented as a native method.
     * <p>You don't call this method; NativeProxy.finalize() calls it for you.
     * <p>You must declare a finalize() method which calls super.finalize().
     */
    protected abstract void releaseNativeResources();

    /**
     * Finalize this NativeProxy by releasing its native resources.
     * The finalizer calls releaseNativeResources() so you don't have to.
     * This finalizer should be called from the finalize() method of all
     * subclasses:
     * class MyProxy extends NativeProxy {
     *      [...]
     *      protected void finalize() throws Throwable {
     *          // do any object-specific finalization other than
     *          // releasing native resources
     *          [...]
     *          super.finalize();
     *      }
     * }
     */
    protected void finalize() throws Throwable {
        if(Debug.DEBUG) {
            unregister(registryIndex);
        }
        releaseNativeResources();
    }

    /**
     * Byte array containing native pointer bytes.
     */
    private byte mPointer[];

    /**
     * <p><b>Native Proxy Registry</b>
     * <p>In debug mode, we keep track of all NativeProxy objects in a 
     * static registry.  Whenever a NativeProxy is constructed, it 
     * registers.  Whenever it finalizes, it unregisters.  At the end of
     * the game, we should be able to garbage collect and then assert that
     * the registry is empty. This could be done, for example, in the
     * jssjava JVM after main() completes.
     * This registration process verifies that people are calling
     * NativeProxy.finalize() from their subclasses of NativeProxy, so that
     * releaseNativeResources() gets called.
     */
    private long registryIndex;
    static Hashtable registry;
    static Random indexGenerator;

    static {
        registry = new Hashtable();
        indexGenerator = new Random();
    }

    /**
     * Register a NativeProxy instance.
     *
     * @return The unique index of this object in the registry.
     */
    private synchronized static long register() {
        Long index;

        // Get a unique index
        do {
            index = new Long(indexGenerator.nextLong());
        } while( registry.containsKey(index) );
        registry.put(index, index);

        return index.longValue();
    }

    /**
     * Unregister a NativeProxy instance.
     *
     * @param index The index of this object in the registry, as returned
     * from the previous call to register().
     */
    private synchronized static void unregister(long index) {
        Long Lindex = new Long(index);
        Long element;

        element = (Long) registry.remove(Lindex);
        Assert._assert(element != null);
    }

    /**
     * @return A list of the indices in the registry. Each element is a Long.
     * @see NativeProxy#getRegistryIndex
     */
    public synchronized static Enumeration getRegistryIndices() {
        return registry.keys();
    }

    /**
     * @return The index of this NativeProxy in the NativeProxy registry.
     * @see NativeProxy#getRegistryIndices
     */
    public long getRegistryIndex() {
        return registryIndex;
    }

    /**
     * Assert that the Registry is empty.  Only works in debug mode; in
     * ship mode, it is a no-op.  If the Registry is not empty when this
     * is called, an assertion (org.mozilla.jss.util.AssertionException)
     * is thrown.
     */
    public synchronized static void assertRegistryEmpty() {
        if( Debug.DEBUG ) {
			if(! registry.isEmpty()) {
				Debug.trace(Debug.VERBOSE, "Warning: "+
					String.valueOf(registry.size())+
            		" NativeProxys are still registered.");
			} else {
        		Debug.trace(Debug.OBNOXIOUS, "NativeProxy registry is empty");
			}
		}
    }
}
