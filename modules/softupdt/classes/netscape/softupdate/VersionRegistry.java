/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * VersionRegistry.java
 */
package netscape.softupdate ;

import java.util.Enumeration;
import java.util.NoSuchElementException;

/**
 * The VersionRegistry class is a set of static functions that form a wrapper
 * around the Navigator's built-in Version Registry.  The Version Registry
 * represents a hierarchical tree of Navigator "components".  Each component
 * represents a physical file installed on the disk or a logical grouping of
 * other components (e.g. Java).  All components should have an associated
 * version used to determine when an update is necessary; components that
 * represent a physical file will also have the disk location of the file.<p>
 *
 * Component names passed as arguments can be specified as full paths from
 * the VersionRegistry root, or as paths relative to the most recently
 * run version of the Navigator.  3rd-party add-ons that work with
 * multiple versions of Navigator will probably want to start their
 * own sub-tree under the root, independent of the components for the
 * Navigator itself.<p>
 *
 * Because this class is intended to be called from JavaScript in addition
 * to Java, errors are indicated through return values rather than exceptions.
 *
 * It is not yet determined if this class will be public for generic use, or
 * only available through the Automatic Software Download feature.
 *
 * @version    1.0 96/11/16
 * @author     Daniel Veditz
 */
final class VersionRegistry implements RegistryErrors {

    /**
     * This class is simply static function wrappers; don't "new" one
     */
    private VersionRegistry() {}

   /**
    * Return the physical disk path for the specified component.
    * @param   component  Registry path of the item to look up in the Registry
    * @return  The disk path for the specified component; NULL indicates error
    * @see     VersionRegistry#checkComponent
    */
   protected static native String componentPath( String component );

   /**
    * Return the version information for the specified component.
    * @param   component  Registry path of the item to look up in the Registry
    * @return  A VersionInfo object for the specified component; NULL indicates error
    * @see     VersionRegistry#checkComponent
    */
   protected static native VersionInfo componentVersion( String component );

   protected static native String getDefaultDirectory( String component );

   protected static native int    setDefaultDirectory( String component, String directory );

   protected static native int installComponent( String name, String path,
      VersionInfo version );

   /**
    * Delete component.
    * @param   component  Registry path of the item to delete
    * @return  Error code
    */
   protected static native int deleteComponent( String component );

   /**
    * Check the status of a named components.
    * @param   component  Registry path of the item to check
    * @return  Error code.  REGERR_OK means the named component was found in
    * the registry, the filepath referred to an existing file, and the
    * checksum matched the physical file. Other error codes can be used to
    * distinguish which of the above was not true.
    */
   protected static native int validateComponent( String component );

   /**
    * verify that the named component is in the registry (light-weight
    * version of validateComponent since it does no disk access).
    * @param  component   Registry path of the item to verify
    * @return Error code. REGERR_OK means it is in the registry.
    * REGERR_NOFIND will usually be the result otherwise.
    */
   protected static native int inRegistry( String component );

   /**
    * Closes the registry file.
    * @return  Error code
    */
   protected static native int close();

   /**
    * Returns an enumeration of the Version Registry contents.  Use the
    * enumeration methods of the returned object to fetch individual
    * elements sequentially.
    */
   protected static Enumeration elements() {
      return new VerRegEnumerator();
   }
}



/**
 * Helper class used to enumerate the Version Registry
 * @see VersionRegistry#elements
 */
final class VerRegEnumerator implements Enumeration {
   private String   path = "";
   private int      state = 0;

   public boolean hasMoreElements() {
      int     saveState = state;
      String  tmp = regNext();
      state = saveState;

      return (tmp != null);
   }

   public Object nextElement() {
      path = regNext();
      if (path == null) {
         throw new NoSuchElementException("VerRegEnumerator");
      }
      return "/"+path;
   }

   private native String regNext();
}
