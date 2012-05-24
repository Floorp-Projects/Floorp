/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.Map;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

import android.util.Log;

/**
 * Covers the fields used by all bookmark objects.
 * @author rnewman
 *
 */
public class BookmarkRecord extends Record {
  public static final String PLACES_URI_PREFIX = "places:";

  private static final String LOG_TAG = "BookmarkRecord";

  public static final String COLLECTION_NAME = "bookmarks";
  public static final long BOOKMARKS_TTL = -1; // Never ttl bookmarks.

  public BookmarkRecord(String guid, String collection, long lastModified, boolean deleted) {
    super(guid, collection, lastModified, deleted);
    this.ttl = BOOKMARKS_TTL;
  }
  public BookmarkRecord(String guid, String collection, long lastModified) {
    this(guid, collection, lastModified, false);
  }
  public BookmarkRecord(String guid, String collection) {
    this(guid, collection, 0, false);
  }
  public BookmarkRecord(String guid) {
    this(guid, COLLECTION_NAME, 0, false);
  }
  public BookmarkRecord() {
    this(Utils.generateGuid(), COLLECTION_NAME, 0, false);
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
    out.ttl       = this.ttl;

    // Copy BookmarkRecord fields.
    out.title           = this.title;
    out.bookmarkURI     = this.bookmarkURI;
    out.description     = this.description;
    out.keyword         = this.keyword;
    out.parentID        = this.parentID;
    out.parentName      = this.parentName;
    out.androidParentID = this.androidParentID;
    out.type            = this.type;
    out.androidPosition = this.androidPosition;

    out.children        = this.copyChildren();
    out.tags            = this.copyTags();

    return out;
  }

  public boolean isBookmark() {
    if (type == null) {
      return false;
    }
    return type.equals("bookmark");
  }

  public boolean isFolder() {
    if (type == null) {
      return false;
    }
    return type.equals("folder");
  }

  public boolean isLivemark() {
    if (type == null) {
      return false;
    }
    return type.equals("livemark");
  }

  public boolean isSeparator() {
    if (type == null) {
      return false;
    }
    return type.equals("separator");
  }

  public boolean isMicrosummary() {
    if (type == null) {
      return false;
    }
    return type.equals("microsummary");
  }

  public boolean isQuery() {
    if (type == null) {
      return false;
    }
    return type.equals("query");
  }

  /**
   * Return true if this record should have the Sync fields
   * of a bookmark, microsummary, or query.
   */
  private boolean isBookmarkIsh() {
    if (type == null) {
      return false;
    }
    return type.equals("bookmark") ||
           type.equals("microsummary") ||
           type.equals("query");
  }

  @Override
  protected void initFromPayload(ExtendedJSONObject payload) {
    this.type        = payload.getString("type");
    this.title       = payload.getString("title");
    this.description = payload.getString("description");
    this.parentID    = payload.getString("parentid");
    this.parentName  = payload.getString("parentName");

    if (isFolder()) {
      try {
        this.children = payload.getArray("children");
      } catch (NonArrayJSONException e) {
        Log.e(LOG_TAG, "Got non-array children in bookmark record " + this.guid, e);
        // Let's see if we can recover later by using the parentid pointers.
        this.children = new JSONArray();
      }
      return;
    }

    final String bmkUri = payload.getString("bmkUri");

    // bookmark, microsummary, query.
    if (isBookmarkIsh()) {
      this.keyword = payload.getString("keyword");
      try {
        this.tags = payload.getArray("tags");
      } catch (NonArrayJSONException e) {
        Logger.warn(LOG_TAG, "Got non-array tags in bookmark record " + this.guid, e);
        this.tags = new JSONArray();
      }
    }

    if (isBookmark()) {
      this.bookmarkURI = bmkUri;
      return;
    }

    if (isLivemark()) {
      String siteUri = payload.getString("siteUri");
      String feedUri = payload.getString("feedUri");
      this.bookmarkURI = encodeUnsupportedTypeURI(bmkUri,
                                                  "siteUri", siteUri,
                                                  "feedUri", feedUri);
      return;
    }
    if (isQuery()) {
      String queryId = payload.getString("queryId");
      String folderName = payload.getString("folderName");
      this.bookmarkURI = encodeUnsupportedTypeURI(bmkUri,
                                                  "queryId", queryId,
                                                  "folderName", folderName);
      return;
    }
    if (isMicrosummary()) {
      String generatorUri = payload.getString("generatorUri");
      String staticTitle = payload.getString("staticTitle");
      this.bookmarkURI = encodeUnsupportedTypeURI(bmkUri,
                                                  "generatorUri", generatorUri,
                                                  "staticTitle", staticTitle);
      return;
    }
    if (isSeparator()) {
      Object p = payload.get("pos");
      if (p instanceof Long) {
        this.androidPosition = (Long) p;
      } else if (p instanceof String) {
        try {
          this.androidPosition = Long.parseLong((String) p, 10);
        } catch (NumberFormatException e) {
          return;
        }
      } else {
        Logger.warn(LOG_TAG, "Unsupported position value " + p);
        return;
      }
      String pos = String.valueOf(this.androidPosition);
      this.bookmarkURI = encodeUnsupportedTypeURI(null, "pos", pos, null, null);
      return;
    }
  }

  @Override
  protected void populatePayload(ExtendedJSONObject payload) {
    putPayload(payload, "type", this.type);
    putPayload(payload, "title", this.title);
    putPayload(payload, "description", this.description);
    putPayload(payload, "parentid", this.parentID);
    putPayload(payload, "parentName", this.parentName);
    putPayload(payload, "keyword", this.keyword);

    if (isFolder()) {
      payload.put("children", this.children);
      return;
    }

    // bookmark, microsummary, query.
    if (isBookmarkIsh()) {
      if (isBookmark()) {
        payload.put("bmkUri", bookmarkURI);
      }

      if (isQuery()) {
        Map<String, String> parts = Utils.extractURIComponents(PLACES_URI_PREFIX, this.bookmarkURI);
        putPayload(payload, "queryId", parts.get("queryId"));
        putPayload(payload, "folderName", parts.get("folderName"));
        return;
      }

      if (this.tags != null) {
        payload.put("tags", this.tags);
      }

      putPayload(payload, "keyword", this.keyword);
      return;
    }

    if (isLivemark()) {
      Map<String, String> parts = Utils.extractURIComponents(PLACES_URI_PREFIX, this.bookmarkURI);
      putPayload(payload, "siteUri", parts.get("siteUri"));
      putPayload(payload, "feedUri", parts.get("feedUri"));
      return;
    }
    if (isMicrosummary()) {
      Map<String, String> parts = Utils.extractURIComponents(PLACES_URI_PREFIX, this.bookmarkURI);
      putPayload(payload, "generatorUri", parts.get("generatorUri"));
      putPayload(payload, "staticTitle", parts.get("staticTitle"));
      return;
    }
    if (isSeparator()) {
      Map<String, String> parts = Utils.extractURIComponents(PLACES_URI_PREFIX, this.bookmarkURI);
      String pos = parts.get("pos");
      if (pos == null) {
        return;
      }
      try {
        payload.put("pos", Long.parseLong(pos, 10));
      } catch (NumberFormatException e) {
        return;
      }
      return;
    }
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

    if (!RepoUtils.stringsEqual(this.type, other.type)) {
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

  /**
   * URL-encode the provided string. If the input is null,
   * the empty string is returned.
   *
   * @param in the string to encode.
   * @return a URL-encoded version of the input.
   */
  protected static String encode(String in) {
    if (in == null) {
      return "";
    }
    try {
      return URLEncoder.encode(in, "UTF-8");
    } catch (UnsupportedEncodingException e) {
      // Will never occur.
      return null;
    }
  }

  /**
   * Take the provided URI and two parameters, constructing a URI like
   *
   *   places:uri=$uri&p1=$p1&p2=$p2
   *
   * null values in either parameter or value result in the parameter being omitted.
   */
  protected static String encodeUnsupportedTypeURI(String originalURI, String p1, String v1, String p2, String v2) {
    StringBuilder b = new StringBuilder(PLACES_URI_PREFIX);
    boolean previous = false;
    if (originalURI != null) {
      b.append("uri=");
      b.append(encode(originalURI));
      previous = true;
    }
    if (p1 != null) {
      if (previous) {
        b.append("&");
      }
      b.append(p1);
      b.append("=");
      b.append(encode(v1));
      previous = true;
    }
    if (p2 != null) {
      if (previous) {
        b.append("&");
      }
      b.append(p2);
      b.append("=");
      b.append(encode(v2));
      previous = true;
    }
    return b.toString();
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
