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
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jason Voll <jvoll@mozilla.com>
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

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.HashMap;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.repositories.BookmarkNeedsReparentingException;
import org.mozilla.gecko.sync.repositories.NoGuidForIdException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.ParentNotFoundException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.Context;
import android.database.Cursor;
import android.util.Log;


public class AndroidBrowserBookmarksRepositorySession extends AndroidBrowserRepositorySession {

  private HashMap<String, Long> guidToID = new HashMap<String, Long>();
  private HashMap<Long, String> idToGuid = new HashMap<Long, String>();
  private HashMap<String, ArrayList<String>> missingParentToChildren = new HashMap<String, ArrayList<String>>();
  private HashMap<String, JSONArray> parentToChildArray = new HashMap<String, JSONArray>();
  private AndroidBrowserBookmarksDataAccessor dataAccessor;
  private int needsReparenting = 0;

  public AndroidBrowserBookmarksRepositorySession(Repository repository, Context context) {
    super(repository);
    RepoUtils.initialize(context);
    dbHelper = new AndroidBrowserBookmarksDataAccessor(context);
    dataAccessor = (AndroidBrowserBookmarksDataAccessor) dbHelper;
  }

  @SuppressWarnings("unchecked")
  @Override
  protected Record recordFromMirrorCursor(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    long androidParentId = RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.PARENT);
    String guid = idToGuid.get(androidParentId);

    if (guid == null) {
      // if the parent has been stored and somehow has a null guid, throw an error
      if (idToGuid.containsKey(androidParentId)) {
        Log.e(LOG_TAG, "Have the parent android id for the record but the parent's guid wasn't found");
        throw new NoGuidForIdException(null);
      } else {
        return RepoUtils.bookmarkFromMirrorCursor(cur, "", "", null);
      }
    }
    
    // Get parent name
    String parentName = "";
    Cursor name = dataAccessor.fetch(new String[] { guid });
    name.moveToFirst();
    if (!name.isAfterLast()) {
      parentName = RepoUtils.getStringFromCursor(name, BrowserContract.Bookmarks.TITLE);
    }
    else {
      Log.e(LOG_TAG, "Couldn't find record with guid " + guid + " while looking for parent name");
      throw new ParentNotFoundException(null);
    }
    name.close();
    
    // If record is a folder, build out the children array
    long isFolder = RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.IS_FOLDER);
    JSONArray childArray = null;
    if (isFolder == 1) {
      long androidID = guidToID.get(RepoUtils.getStringFromCursor(cur, "guid"));
      Cursor children = dataAccessor.getChildren(androidID);
      children.moveToFirst();
      int count = 0;
      
      // Get children into array in correct order
      while(!children.isAfterLast()) {
        count++;
        children.moveToNext();
      }
      children.moveToFirst();
      String[] kids = new String[count];
      while(!children.isAfterLast()) {
        if (childArray == null) childArray = new JSONArray();
        String childGuid = RepoUtils.getStringFromCursor(children, "guid");
        int childPosition = (int) RepoUtils.getLongFromCursor(children, BrowserContract.Bookmarks.POSITION);
        kids[childPosition] = childGuid;
        children.moveToNext();
      }
      children.close();
      for(int i = 0; i < kids.length; i++) {
        childArray.add(kids[i]);
      }
      
    }
    return RepoUtils.bookmarkFromMirrorCursor(cur, guid, parentName, childArray);
  }

  @Override
  protected boolean checkRecordType(Record record) {
    BookmarkRecord bmk = (BookmarkRecord) record;
    if (bmk.type.equalsIgnoreCase(AndroidBrowserBookmarksDataAccessor.TYPE_BOOKMARK) ||
        bmk.type.equalsIgnoreCase(AndroidBrowserBookmarksDataAccessor.TYPE_FOLDER)) {
      return true;
    }
    Log.i(LOG_TAG, "Ignoring record with guid: " + record.guid + " and type: " + ((BookmarkRecord)record).type);
    return false;
  }
  
  @Override
  public void begin(RepositorySessionBeginDelegate delegate) {
    // Check for the existence of special folders
    // and insert them if they don't exist.
    Cursor cur;
    try {
      dataAccessor.checkAndBuildSpecialGuids();
      cur = dataAccessor.getGuidsIDsForFolders();
    } catch (NullCursorException e) {
      delegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      delegate.onBeginFailed(e);
      return;
    }
    
    // To deal with parent mapping of bookmarks we have to do some
    // hairy stuff, here's the setup for it
    cur.moveToFirst();
    while(!cur.isAfterLast()) {
      String guid = RepoUtils.getStringFromCursor(cur, "guid");
      long id = RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks._ID);
      guidToID.put(guid, id);
      idToGuid.put(id, guid);
      cur.moveToNext();
    }
    cur.close();
    
    super.begin(delegate);
  }

  @Override
  public void finish(RepositorySessionFinishDelegate delegate) {
    // Override finish to do this check; make sure all records
    // needing re-parenting have been re-parented.
    if (needsReparenting != 0) {
      Log.e(LOG_TAG, "Finish called but " + needsReparenting +
          " bookmark(s) have been placed in unsorted bookmarks and not been reparented.");
      delegate.onFinishFailed(new BookmarkNeedsReparentingException(null));
    } else {
      super.finish(delegate);
    }
  };

  // TODO this code is yucky, cleanup or comment or something
  @SuppressWarnings("unchecked")
  @Override
  protected long insert(Record record) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    BookmarkRecord bmk = (BookmarkRecord) record;
    
    // Check if parent exists
    if (guidToID.containsKey(bmk.parentID)) {
      bmk.androidParentID = guidToID.get(bmk.parentID);
      JSONArray children = parentToChildArray.get(bmk.parentID);
      if (children != null) {
        if (!children.contains(bmk.guid)) {
          children.add(bmk.guid);
          parentToChildArray.put(bmk.parentID, children);
        }
        bmk.androidPosition = children.indexOf(bmk.guid);
      }
    }
    else {
      bmk.androidParentID = guidToID.get("unfiled");
      ArrayList<String> children;
      if (missingParentToChildren.containsKey(bmk.parentID)) {
        children = missingParentToChildren.get(bmk.parentID);
      } else {
        children = new ArrayList<String>();
      }
      children.add(bmk.guid);
      needsReparenting++;
      missingParentToChildren.put(bmk.parentID, children);
    }

    long id = RepoUtils.getAndroidIdFromUri(dbHelper.insert(bmk));
    putRecordToGuidMap(buildRecordString(bmk), bmk.guid);
    bmk.androidID = id;

    // If record is folder, update maps and re-parent children if necessary
    if(bmk.type.equalsIgnoreCase(AndroidBrowserBookmarksDataAccessor.TYPE_FOLDER)) {
      guidToID.put(bmk.guid, id);
      idToGuid.put(id, bmk.guid);

      JSONArray childArray = bmk.children;

      // Re-parent.
      if(missingParentToChildren.containsKey(bmk.guid)) {
        for (String child : missingParentToChildren.get(bmk.guid)) {
          long position;
          if (bmk.children.contains(child)) {
            position = childArray.indexOf(child);
          } else {
            childArray.add(child);
            position = childArray.indexOf(child);
          }
          dataAccessor.updateParentAndPosition(child, id, position);
          needsReparenting--;
        }
        missingParentToChildren.remove(bmk.guid);
      }
      parentToChildArray.put(bmk.guid, childArray);
    }
    return id;
  }

  @Override
  protected String buildRecordString(Record record) {
    BookmarkRecord bmk = (BookmarkRecord) record;
    return bmk.title + bmk.bookmarkURI + bmk.type + bmk.parentName;
  }
}
