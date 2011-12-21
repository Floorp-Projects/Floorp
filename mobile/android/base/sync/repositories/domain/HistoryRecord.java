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

package org.mozilla.gecko.sync.repositories.domain;

import java.util.HashMap;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

public class HistoryRecord extends Record {

  public static final String COLLECTION_NAME = "history";

  public HistoryRecord(String guid, String collection, long lastModified,
      boolean deleted) {
    super(guid, collection, lastModified, deleted);
  }
  public HistoryRecord(String guid, String collection, long lastModified) {
    super(guid, collection, lastModified, false);
  }
  public HistoryRecord(String guid, String collection) {
    super(guid, collection, 0, false);
  }
  public HistoryRecord(String guid) {
    super(guid, COLLECTION_NAME, 0, false);
  }
  public HistoryRecord() {
    super(Utils.generateGuid(), COLLECTION_NAME, 0, false);
  }

  public String     title;
  public String     histURI;
  public JSONArray  visits;
  public long       fennecDateVisited;
  public long       fennecVisitCount;

  @Override
  public void initFromPayload(CryptoRecord payload) {
    this.histURI = (String) payload.payload.get("histUri");
    this.title       = (String) payload.payload.get("title");
    // TODO add missing fields
  }
  @Override
  public CryptoRecord getPayload() {
    // TODO Auto-generated method stub
    return null;
  }

  @Override
  public boolean equals(Object o) {
    if (!o.getClass().equals(HistoryRecord.class)) return false;
    HistoryRecord other = (HistoryRecord) o;
    return
        super.equals(other) &&
        RepoUtils.stringsEqual(this.title, other.title) &&
        RepoUtils.stringsEqual(this.histURI, other.histURI) &&
        this.checkVisitsEquals(other);
  }
  
  private boolean checkVisitsEquals(HistoryRecord other) {
    
    // Handle nulls
    if (this.visits == other.visits) return true;
    else if ((this.visits == null || this.visits.size() == 0) && (other.visits != null && other.visits.size() !=0)) return false;
    else if ((this.visits != null && this.visits.size() != 0) && (other.visits == null || other.visits.size() == 0)) return false;
    
    // Check size
    if (this.visits.size() != other.visits.size()) return false;
    
    HashMap<Long, Long> otherVisits = new HashMap<Long, Long>();
    for (int i = 0; i < other.visits.size(); i++) {
      JSONObject visit = (JSONObject) other.visits.get(i);
      otherVisits.put((Long)visit.get("date"), (Long)visit.get("type"));
    }
    
    for (int i = 0; i < this.visits.size(); i++) {
      JSONObject visit = (JSONObject) this.visits.get(i);
      if (!otherVisits.containsKey(visit.get("date"))) return false;
      if (otherVisits.get(visit.get("date")) != (Long) visit.get("type")) return false;
    }
    
    return true;
  }
  
//  
//  Example record:
//
//  {id:"--DUvUomABNq",
//   histUri:"https://bugzilla.mozilla.org/show_bug.cgi?id=697634",
//   title:"697634 \u2013 xpcshell test failures on 10.7",
//   visits:[{date:1320087601465600, type:2},
//           {date:1320084970724990, type:1},
//           {date:1320084847035717, type:1},
//           {date:1319764134412287, type:1},
//           {date:1319757917982518, type:1},
//           {date:1319751664627351, type:1},
//           {date:1319681421072326, type:1},
//           {date:1319681306455594, type:1},
//           {date:1319678117125234, type:1},
//           {date:1319677508862901, type:1}]
//  }
//   
//"type" is a transition type:
//
//https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsINavHistoryService#Transition_type_constants

}
