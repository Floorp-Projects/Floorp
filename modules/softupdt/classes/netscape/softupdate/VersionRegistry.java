/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

   protected static int installComponent( String name, String path,
      VersionInfo version, int refCount )
   {    
       int  err = installComponent( name, path, version );

       if ( err == REGERR_OK )
           err = setRefCount( name, refCount );
       return err;
   }


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

   /**
    * Set the refcount of a named component.
    * @param   component  Registry path of the item to check
    * @param   refcount  value to be set
    * @return  Error code
    */
   protected static native int setRefCount( String component, int refcount );

   /**
    * Return the refcount of a named component.
    * @param   component  Registry path of the item to check
    * @return  the value of refCount
    */
   protected static native Integer getRefCount( String component );

   /**
    * Creates a node for the item in the Uninstall list.
    * @param   regPackageName  Registry name of the package we are installing
    * @return  userPackagename User-readable package name
    * @return  Error code
    */
   protected static int uninstallCreate( String regPackageName, String userPackageName )
   {
       String temp = convertPackageName(regPackageName);
       regPackageName = temp;
       return uninstallCreateNode(regPackageName, userPackageName);
   }

   protected static native int uninstallCreateNode( String regPackageName, String userPackageName );

   /**
    * Replaces all '/' with '_',in the given string.If an '_' already exists in the
    * given string, it is escaped by adding another '_' to it.
    * @param   regPackageName  Registry name of the package we are installing
    * @return  modified String
    */
   private static String convertPackageName( String regPackageName )
   {
       String tempStr = regPackageName;
       String convertedPackageName;
       boolean bSharedUninstall = false;
        
       if (regPackageName.startsWith("/"))
           bSharedUninstall = true;

       int index = tempStr.indexOf('_');
       while (index != -1)
       {
           StringBuffer temp = new StringBuffer(tempStr);
           temp.insert(index + 1, '_');
           tempStr = temp.toString();
           index = tempStr.indexOf('_', index + 2);
       }

       convertedPackageName = tempStr.replace('/', '_');  
       if (bSharedUninstall)
       {
           StringBuffer temp = new StringBuffer(convertedPackageName);
           temp.setCharAt(0, '/');
           convertedPackageName = temp.toString();
       }
       return convertedPackageName;
   }

   
   /**
    * Adds the file as a property of the Shared Files node under the appropriate 
    * packageName node in the Uninstall list.
    * @param   regPackageName  Registry name of the package installed
    * @param   vrName  registry name of the shared file
    * @return  Error code
    */
   protected static int uninstallAddFile( String regPackageName, String vrName )
   {
       String temp = convertPackageName(regPackageName);
       regPackageName = temp;
       return uninstallAddFileToList(regPackageName, vrName);
   }

   protected static native int uninstallAddFileToList( String regPackageName, String vrName );

   /**
    * Checks if the shared file exists in the uninstall list of the package
    * @param   regPackageName  Registry name of the package installed
    * @param   vrName  registry name of the shared file
    * @return true or false 
    */
   protected static int uninstallFileExists( String regPackageName, String vrName )
   {
       String temp = convertPackageName(regPackageName);
       regPackageName = temp;
       return uninstallFileExistsInList(regPackageName, vrName);
   }

   protected static native int uninstallFileExistsInList( String regPackageName, String vrName );
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
