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
 *   Jason Voll <jvoll@mozilla.com>
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

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

public class PasswordRecord extends Record {

  public static final String COLLECTION_NAME = "passwords";

  public PasswordRecord(String guid, String collection, long lastModified,
      boolean deleted) {
    super(guid, collection, lastModified, deleted);
  }
  public PasswordRecord(String guid, String collection, long lastModified) {
    super(guid, collection, lastModified, false);
  }
  public PasswordRecord(String guid, String collection) {
    super(guid, collection, 0, false);
  }
  public PasswordRecord(String guid) {
    super(guid, COLLECTION_NAME, 0, false);
  }
  public PasswordRecord() {
    super(Utils.generateGuid(), COLLECTION_NAME, 0, false);
  }

  public String hostname;
  public String formSubmitURL;
  public String httpRealm;
  // TODO these are encrypted in the passwords content provider,
  // need to figure out what we need to do here.
  public String username;
  public String password;
  public String usernameField;
  public String passwordField;
  public String encType;
  public long   timeLastUsed;
  public long   timesUsed;


  @Override
  public Record copyWithIDs(String guid, long androidID) {
    PasswordRecord out = new PasswordRecord(guid, this.collection, this.lastModified, this.deleted);
    out.androidID = androidID;
    out.sortIndex = this.sortIndex;

    // Copy HistoryRecord fields.
    out.hostname      = this.hostname;
    out.formSubmitURL = this.formSubmitURL;
    out.httpRealm     = this.httpRealm;
    out.username      = this.username;
    out.password      = this.password;
    out.usernameField = this.usernameField;
    out.passwordField = this.passwordField;
    out.encType       = this.encType;
    out.timeLastUsed  = this.timeLastUsed;
    out.timesUsed     = this.timesUsed;

    return out;
  }

  @Override
  public void initFromPayload(ExtendedJSONObject payload) {
    // TODO: implement.
  }

  @Override
  public void populatePayload(ExtendedJSONObject payload) {
    // TODO: implement.
  }
  
  @Override
  public boolean congruentWith(Object o) {
    if (o == null || !(o instanceof PasswordRecord)) {
      return false;
    }
    PasswordRecord other = (PasswordRecord) o;
    if (!super.congruentWith(other)) {
      return false;
    }
    return RepoUtils.stringsEqual(this.hostname, other.hostname)
        && RepoUtils.stringsEqual(this.formSubmitURL, other.formSubmitURL)
        && RepoUtils.stringsEqual(this.httpRealm, other.httpRealm)
        && RepoUtils.stringsEqual(this.username, other.username)
        && RepoUtils.stringsEqual(this.password, other.password)
        && RepoUtils.stringsEqual(this.usernameField, other.usernameField)
        && RepoUtils.stringsEqual(this.passwordField, other.passwordField)
        && RepoUtils.stringsEqual(this.encType, other.encType);
  }

  @Override
  public boolean equalPayloads(Object o) {
    if (o == null || !(o instanceof PasswordRecord)) {
      return false;
    }
    PasswordRecord other = (PasswordRecord) o;
    if (!super.equalPayloads(other)) {
      return false;
    }
    return RepoUtils.stringsEqual(this.hostname, other.hostname)
        && RepoUtils.stringsEqual(this.formSubmitURL, other.formSubmitURL)
        && RepoUtils.stringsEqual(this.httpRealm, other.httpRealm)
        && RepoUtils.stringsEqual(this.username, other.username)
        && RepoUtils.stringsEqual(this.password, other.password)
        && RepoUtils.stringsEqual(this.usernameField, other.usernameField)
        && RepoUtils.stringsEqual(this.passwordField, other.passwordField)
        && RepoUtils.stringsEqual(this.encType, other.encType)
        && (this.timeLastUsed == other.timeLastUsed)
        && (this.timesUsed == other.timesUsed);
  }

}
