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
 * Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync.repositories.domain;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.AndroidBrowserBookmarksDataAccessor;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

import android.util.Log;

/**
 * Covers the fields used by all bookmark objects.
 * @author rnewman
 *
 */
public class BookmarkRecord extends Record {
  private static final String LOG_TAG = "BookmarkRecord";

  public static final String COLLECTION_NAME = "bookmarks";

  public BookmarkRecord(String guid, String collection, long lastModified, boolean deleted) {
    super(guid, collection, lastModified, deleted);
  }
  public BookmarkRecord(String guid, String collection, long lastModified) {
    super(guid, collection, lastModified, false);
  }
  public BookmarkRecord(String guid, String collection) {
    super(guid, collection, 0, false);
  }
  public BookmarkRecord(String guid) {
    super(guid, COLLECTION_NAME, 0, false);
  }
  public BookmarkRecord() {
    super(Utils.generateGuid(), COLLECTION_NAME, 0, false);
  }

  // Note: redundant accessors are evil. We're all grownups; let's just use
  // public fields.
  public String  title;
  public String  bookmarkURI;
  public String  description;
  public String  keyword;
  public String  parentID;
  public String  parentName;
  public long    androidParentID;
  public String  type;
  public String  pos;
  public long    androidPosition;

  public JSONArray children;
  public JSONArray tags;

  @Override
  public String toString() {
    return "#<Bookmark " + guid + " (" + androidID + "), parent " +
           parentID + "/" + androidParentID + "/" + parentName + ">";
  }

  // Oh God, this is terribly thread-unsafe. These record objects should be immutable.
  @SuppressWarnings("unchecked")
  protected JSONArray copyChildren() {
    if (this.children == null) {
      return null;
    }
    JSONArray children = new JSONArray();
    children.addAll(this.children);
    return children;
  }

  @SuppressWarnings("unchecked")
  protected JSONArray copyTags() {
    if (this.tags == null) {
      return null;
    }
    JSONArray tags = new JSONArray();
    tags.addAll(this.tags);
    return tags;
  }

  @Override
  public Record copyWithIDs(String guid, long androidID) {
    BookmarkRecord out = new BookmarkRecord(guid, this.collection, this.lastModified, this.deleted);
    out.androidID = androidID;
    out.sortIndex = this.sortIndex;

    // Copy BookmarkRecord fields.
    out.title           = this.title;
    out.bookmarkURI     = this.bookmarkURI;
    out.description     = this.description;
    out.keyword         = this.keyword;
    out.parentID        = this.parentID;
    out.parentName      = this.parentName;
    out.androidParentID = this.androidParentID;
    out.type            = this.type;
    out.pos             = this.pos;
    out.androidPosition = this.androidPosition;

    out.children        = this.copyChildren();
    out.tags            = this.copyTags();

    return out;
  }

  @Override
  protected void initFromPayload(ExtendedJSONObject payload) {
    this.type        = (String) payload.get("type");
    this.title       = (String) payload.get("title");
    this.description = (String) payload.get("description");
    this.parentID    = (String) payload.get("parentid");
    this.parentName  = (String) payload.get("parentName");

    // Bookmark.
    if (isBookmark()) {
      this.bookmarkURI = (String) payload.get("bmkUri");
      this.keyword     = (String) payload.get("keyword");
      try {
        this.tags = payload.getArray("tags");
      } catch (NonArrayJSONException e) {
        Log.e(LOG_TAG, "Got non-array tags in bookmark record " + this.guid, e);
        this.tags = new JSONArray();
      }
    }

    // Folder.
    if (isFolder()) {
      try {
        this.children = payload.getArray("children");
      } catch (NonArrayJSONException e) {
        Log.e(LOG_TAG, "Got non-array children in bookmark record " + this.guid, e);
        // Let's see if we can recover later by using the parentid pointers.
        this.children = new JSONArray();
      }
    }

    // TODO: predecessor ID?
    // TODO: type-specific attributes:
    /*
      public String generatorURI;
      public String staticTitle;
      public String folderName;
      public String queryID;
      public String siteURI;
      public String feedURI;
      public String pos;
     */
  }

  @Override
  protected void populatePayload(ExtendedJSONObject payload) {
    putPayload(payload, "type", this.type);
    putPayload(payload, "title", this.title);
    putPayload(payload, "description", this.description);
    putPayload(payload, "parentid", this.parentID);
    putPayload(payload, "parentName", this.parentName);

    if (isBookmark()) {
      payload.put("bmkUri", bookmarkURI);
      payload.put("keyword", keyword);
      payload.put("tags", this.tags);
    } else if (isFolder()) {
      payload.put("children", this.children);
    }
  }

  public boolean isBookmark() {
    return AndroidBrowserBookmarksDataAccessor.TYPE_BOOKMARK.equalsIgnoreCase(this.type);
  }

  public boolean isFolder() {
    return AndroidBrowserBookmarksDataAccessor.TYPE_FOLDER.equalsIgnoreCase(this.type);
  }

  private void trace(String s) {
    Logger.trace(LOG_TAG, s);
  }

  @Override
  public boolean equalPayloads(Object o) {
    trace("Calling BookmarkRecord.equalPayloads.");
    if (o == null || !(o instanceof BookmarkRecord)) {
      return false;
    }

    BookmarkRecord other = (BookmarkRecord) o;
    if (!super.equalPayloads(other)) {
      return false;
    }

    // Check children.
    if (isFolder() && (this.children != other.children)) {
      trace("BookmarkRecord.equals: this folder: " + this.title + ", " + this.guid);
      trace("BookmarkRecord.equals: other: " + other.title + ", " + other.guid);
      if (this.children  == null &&
          other.children != null) {
        trace("Records differ: one children array is null.");
        return false;
      }
      if (this.children  != null &&
          other.children == null) {
        trace("Records differ: one children array is null.");
        return false;
      }
      if (this.children.size() != other.children.size()) {
        trace("Records differ: children arrays differ in size (" +
              this.children.size() + " vs. " + other.children.size() + ").");
        return false;
      }

      for (int i = 0; i < this.children.size(); i++) {
        String child = (String) this.children.get(i);
        if (!other.children.contains(child)) {
          trace("Records differ: child " + child + " not found.");
          return false;
        }
      }
    }

    trace("Checking strings.");
    return RepoUtils.stringsEqual(this.title, other.title)
        && RepoUtils.stringsEqual(this.bookmarkURI, other.bookmarkURI)
        && RepoUtils.stringsEqual(this.parentID, other.parentID)
        && RepoUtils.stringsEqual(this.parentName, other.parentName)
        && RepoUtils.stringsEqual(this.type, other.type)
        && RepoUtils.stringsEqual(this.description, other.description)
        && RepoUtils.stringsEqual(this.keyword, other.keyword)
        && jsonArrayStringsEqual(this.tags, other.tags);
  }

  // TODO: two records can be congruent if their child lists are different.
  @Override
  public boolean congruentWith(Object o) {
    return this.equalPayloads(o) &&
           super.congruentWith(o);
  }

  // Converts two JSONArrays to strings and checks if they are the same.
  // This is only useful for stuff like tags where we aren't actually
  // touching the data there (and therefore ordering won't change)
  private boolean jsonArrayStringsEqual(JSONArray a, JSONArray b) {
    // Check for nulls
    if (a == b) return true;
    if (a == null && b != null) return false;
    if (a != null && b == null) return false;
    return RepoUtils.stringsEqual(a.toJSONString(), b.toJSONString());
  }
}


/*
// Bookmark:
{cleartext:
  {id:            "l7p2xqOTMMXw",
   type:          "bookmark",
   title:         "Your Flight Status",
   parentName:    "mobile",
   bmkUri:        "http: //www.flightstats.com/go/Mobile/flightStatusByFlightProcess.do;jsessionid=13A6C8DCC9592AF141A43349040262CE.web3: 8009?utm_medium=cpc&utm_campaign=co-op&utm_source=airlineInformationAndStatus&id=212492593",
   tags:          [],
   keyword:       null,
   description:   null,
   loadInSidebar: false,
   parentid:      "mobile"},
 data: {payload: {ciphertext: null},
 id:         "l7p2xqOTMMXw",
 sortindex:  107},
 collection: "bookmarks"}

// Folder:
{cleartext:
  {id:          "mobile",
   type:        "folder",
   parentName:  "",
   title:       "mobile",
   description: null,
   children:    ["1ROdlTuIoddD", "3Z_bMIHPSZQ8", "4mSDUuOo2iVB", "8aEdE9IIrJVr",
                 "9DzPTmkkZRDb", "Qwwb99HtVKsD", "s8tM36aGPKbq", "JMTi61hOO3JV",
                 "JQUDk0wSvYip", "LmVH-J1r3HLz", "NhgQlC5ykYGW", "OVanevUUaqO2",
                 "OtQVX0PMiWQj", "_GP5cF595iie", "fkRssjXSZDL3", "k7K_NwIA1Ya0",
                 "raox_QGzvqh1", "vXYL-xHjK06k", "QKHKUN6Dm-xv", "pmN2dYWT2MJ_",
                 "EVeO_J1SQiwL", "7N-qkepS7bec", "NIGa3ha-HVOE", "2Phv1I25wbuH",
                 "TTSIAH1fV0VE", "WOmZ8PfH39Da", "gDTXNg4m1AJZ", "ayI30OZslHbO",
                 "zSEs4O3n6CzQ", "oWTDR0gO2aWf", "wWHUoFaInXi9", "F7QTuVJDpsTM",
                 "FIboggegplk-", "G4HWrT5nfRYS", "MHA7y9bupDdv", "T_Ldzmj0Ttte",
                 "U9eYu3SxsE_U", "bk463Kl9IO_m", "brUfrqJjFNSR", "ccpawfWsD-bY",
                 "l7p2xqOTMMXw", "o-nSDKtXYln7"],
   parentid: "places"},
 data:        {payload: {ciphertext: null},
 id:          "mobile",
 sortindex:   1000000},
 collection: "bookmarks"}
*/
