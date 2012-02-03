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
import org.mozilla.gecko.sync.Utils;
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

  // TODO: synchronization for these.
  private HashMap<String, Long> guidToID = new HashMap<String, Long>();
  private HashMap<Long, String> idToGuid = new HashMap<Long, String>();

  private HashMap<String, ArrayList<String>> missingParentToChildren = new HashMap<String, ArrayList<String>>();
  private HashMap<String, JSONArray> parentToChildArray = new HashMap<String, JSONArray>();
  private AndroidBrowserBookmarksDataAccessor dataAccessor;
  private int needsReparenting = 0;

  /**
   * Return true if the provided record GUID should be skipped
   * in child lists or fetch results.
   *
   * @param recordGUID
   * @return
   */
  public static boolean forbiddenGUID(String recordGUID) {
    return recordGUID == null ||
           "places".equals(recordGUID) ||
           "tags".equals(recordGUID);
  }

  public AndroidBrowserBookmarksRepositorySession(Repository repository, Context context) {
    super(repository);
    RepoUtils.initialize(context);
    dbHelper = new AndroidBrowserBookmarksDataAccessor(context);
    dataAccessor = (AndroidBrowserBookmarksDataAccessor) dbHelper;
  }

  private boolean rowIsFolder(Cursor cur) {
    return RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.IS_FOLDER) == 1;
  }

  private String getGUIDForID(long androidID) {
    String guid = idToGuid.get(androidID);
    trace("  " + androidID + " => " + guid);
    return guid;
  }

  private String getGUID(Cursor cur) {
    return RepoUtils.getStringFromCursor(cur, "guid");
  }

  private long getParentID(Cursor cur) {
    return RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks.PARENT);
  }

  private String getParentName(String parentGUID) throws ParentNotFoundException, NullCursorException {
    if (parentGUID == null) {
      return "";
    }
    if (RepoUtils.SPECIAL_GUIDS_MAP.containsKey(parentGUID)) {
      return RepoUtils.SPECIAL_GUIDS_MAP.get(parentGUID);
    }

    // Get parent name from database.
    String parentName = "";
    Cursor name = dataAccessor.fetch(new String[] { parentGUID });
    try {
      name.moveToFirst();
      if (!name.isAfterLast()) {
        parentName = RepoUtils.getStringFromCursor(name, BrowserContract.Bookmarks.TITLE);
      }
      else {
        Log.e(LOG_TAG, "Couldn't find record with guid '" + parentGUID + "' when looking for parent name.");
        throw new ParentNotFoundException(null);
      }
    } finally {
      name.close();
    }
    return parentName;
  }

  @SuppressWarnings("unchecked")
  private JSONArray getChildren(long androidID) throws NullCursorException {
    trace("Calling getChildren for androidID " + androidID);
    JSONArray childArray = new JSONArray();
    Cursor children = dataAccessor.getChildren(androidID);
    try {
      if (!children.moveToFirst()) {
        trace("No children: empty cursor.");
        return childArray;
      }

      int count = children.getCount();
      String[] kids = new String[count];
      trace("Expecting " + count + " children.");

      // Track badly positioned records.
      // TODO: use a mechanism here that preserves ordering.
      HashMap<String, Long> broken = new HashMap<String, Long>();

      // Get children into array in correct order.
      while (!children.isAfterLast()) {
        String childGuid = getGUID(children);
        trace("  Child GUID: " + childGuid);
        int childPosition = (int) RepoUtils.getLongFromCursor(children, BrowserContract.Bookmarks.POSITION);
        trace("  Child position: " + childPosition);
        if (childPosition >= count) {
          Log.w(LOG_TAG, "Child position " + childPosition + " greater than expected children " + count);
          broken.put(childGuid, 0L);
        } else {
          String existing = kids[childPosition];
          if (existing != null) {
            Log.w(LOG_TAG, "Child position " + childPosition + " already occupied! (" +
                childGuid + ", " + existing + ")");
            broken.put(childGuid, 0L);
          } else {
            kids[childPosition] = childGuid;
          }
        }
        children.moveToNext();
      }

      try {
        Utils.fillArraySpaces(kids, broken);
      } catch (Exception e) {
        Log.e(LOG_TAG, "Unable to reposition children to yield a valid sequence. Data loss may result.", e);
      }
      // TODO: now use 'broken' to edit the records on disk.

      // Collect into a more friendly data structure.
      for (int i = 0; i < count; ++i) {
        String kid = kids[i];
        if (forbiddenGUID(kid)) {
          continue;
        }
        childArray.add(kid);
      }
      if (Utils.ENABLE_TRACE_LOGGING) {
        Log.d(LOG_TAG, "Output child array: " + childArray.toJSONString());
      }
    } finally {
      children.close();
    }
    return childArray;
  }

  @Override
  protected Record recordFromMirrorCursor(Cursor cur) throws NoGuidForIdException, NullCursorException, ParentNotFoundException {
    String recordGUID = getGUID(cur);
    Log.d(LOG_TAG, "Record from mirror cursor: " + recordGUID);

    if (forbiddenGUID(recordGUID)) {
      Log.d(LOG_TAG, "Ignoring " + recordGUID + " record in recordFromMirrorCursor.");
      return null;
    }

    long androidParentID     = getParentID(cur);
    String androidParentGUID = getGUIDForID(androidParentID);

    if (androidParentGUID == null) {
      Log.d(LOG_TAG, "No parent GUID for record " + recordGUID + " with parent " + androidParentID);
      // If the parent has been stored and somehow has a null GUID, throw an error.
      if (idToGuid.containsKey(androidParentID)) {
        Log.e(LOG_TAG, "Have the parent android ID for the record but the parent's GUID wasn't found.");
        throw new NoGuidForIdException(null);
      }
    }

    // If record is a folder, build out the children array.
    JSONArray childArray = getChildArrayForCursor(cur, recordGUID);
    String parentName = getParentName(androidParentGUID);
    return RepoUtils.bookmarkFromMirrorCursor(cur, androidParentGUID, parentName, childArray);
  }

  protected JSONArray getChildArrayForCursor(Cursor cur, String recordGUID) throws NullCursorException {
    JSONArray childArray = null;
    boolean isFolder = rowIsFolder(cur);
    Log.d(LOG_TAG, "Record " + recordGUID + " is a " + (isFolder ? "folder." : "bookmark."));
    if (isFolder) {
      long androidID = guidToID.get(recordGUID);
      childArray = getChildren(androidID);
    }
    if (childArray != null) {
      Log.d(LOG_TAG, "Fetched " + childArray.size() + " children for " + recordGUID);
    }
    return childArray;
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
      Log.d(LOG_TAG, "Check and build special GUIDs.");
      dataAccessor.checkAndBuildSpecialGuids();
      cur = dataAccessor.getGuidsIDsForFolders();
      Log.d(LOG_TAG, "Got GUIDs for folders.");
    } catch (android.database.sqlite.SQLiteConstraintException e) {
      Log.e(LOG_TAG, "Got sqlite constraint exception working with Fennec bookmark DB.", e);
      delegate.onBeginFailed(e);
      return;
    } catch (NullCursorException e) {
      delegate.onBeginFailed(e);
      return;
    } catch (Exception e) {
      delegate.onBeginFailed(e);
      return;
    }
    
    // To deal with parent mapping of bookmarks we have to do some
    // hairy stuff. Here's the setup for it.

    Log.d(LOG_TAG, "Preparing folder ID mappings.");
    idToGuid.put(0L, "places");       // Fake our root.
    try {
      cur.moveToFirst();
      while (!cur.isAfterLast()) {
        String guid = getGUID(cur);
        long id = RepoUtils.getLongFromCursor(cur, BrowserContract.Bookmarks._ID);
        guidToID.put(guid, id);
        idToGuid.put(id, guid);
        Log.d(LOG_TAG, "GUID " + guid + " maps to " + id);
        cur.moveToNext();
      }
    } finally {
      cur.close();
    }
    Log.d(LOG_TAG, "Done with initial setup of bookmarks session.");
    super.begin(delegate);
  }

  @Override
  public void finish(RepositorySessionFinishDelegate delegate) {
    // Override finish to do this check; make sure all records
    // needing re-parenting have been re-parented.
    if (needsReparenting != 0) {
      Log.e(LOG_TAG, "Finish called but " + needsReparenting +
            " bookmark(s) have been placed in unsorted bookmarks and not been reparented.");

      // TODO: handling of failed reparenting.
      // E.g., delegate.onFinishFailed(new BookmarkNeedsReparentingException(null));
    }
    super.finish(delegate);
  };

  @Override
  @SuppressWarnings("unchecked")
  protected Record prepareRecord(Record record) {
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

    if (bmk.isFolder()) {
      Log.d(LOG_TAG, "Inserting folder " + bmk.guid + ", " + bmk.title +
                     " with parent " + bmk.androidParentID +
                     " (" + bmk.parentID + ", " + bmk.parentName +
                     ", " + bmk.pos + ")");
    } else {
      Log.d(LOG_TAG, "Inserting bookmark " + bmk.guid + ", " + bmk.title + ", " +
                     bmk.bookmarkURI + " with parent " + bmk.androidParentID +
                     " (" + bmk.parentID + ", " + bmk.parentName +
                     ", " + bmk.pos + ")");
    }
    return bmk;
  }

  @Override
  @SuppressWarnings("unchecked")
  protected void updateBookkeeping(Record record) throws NoGuidForIdException,
                                                         NullCursorException,
                                                         ParentNotFoundException {
    super.updateBookkeeping(record);
    BookmarkRecord bmk = (BookmarkRecord) record;

    // If record is folder, update maps and re-parent children if necessary
    if (bmk.type.equalsIgnoreCase(AndroidBrowserBookmarksDataAccessor.TYPE_FOLDER)) {
      guidToID.put(bmk.guid, bmk.androidID);
      idToGuid.put(bmk.androidID, bmk.guid);

      JSONArray childArray = bmk.children;

      // Re-parent.
      if (missingParentToChildren.containsKey(bmk.guid)) {
        for (String child : missingParentToChildren.get(bmk.guid)) {
          long position;
          if (!bmk.children.contains(child)) {
            childArray.add(child);
          }
          position = childArray.indexOf(child);
          dataAccessor.updateParentAndPosition(child, bmk.androidID, position);
          needsReparenting--;
        }
        missingParentToChildren.remove(bmk.guid);
      }
      parentToChildArray.put(bmk.guid, childArray);
    }
  }

  @Override
  protected String buildRecordString(Record record) {
    BookmarkRecord bmk = (BookmarkRecord) record;
    return bmk.title + bmk.bookmarkURI + bmk.type + bmk.parentName;
  }
}
