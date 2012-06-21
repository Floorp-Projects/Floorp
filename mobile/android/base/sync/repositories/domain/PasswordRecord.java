/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;

import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.android.RepoUtils;

public class PasswordRecord extends Record {
  private static final String LOG_TAG = "PasswordRecord";

  public static final String COLLECTION_NAME = "passwords";
  public static long PASSWORDS_TTL = -1; // Never expire passwords.

  // Payload strings.
  public static final String PAYLOAD_HOSTNAME = "hostname";
  public static final String PAYLOAD_FORM_SUBMIT_URL = "formSubmitURL";
  public static final String PAYLOAD_HTTP_REALM = "httpRealm";
  public static final String PAYLOAD_USERNAME = "username";
  public static final String PAYLOAD_PASSWORD = "password";
  public static final String PAYLOAD_USERNAME_FIELD = "usernameField";
  public static final String PAYLOAD_PASSWORD_FIELD = "passwordField";

  public PasswordRecord(String guid, String collection, long lastModified, boolean deleted) {
    super(guid, collection, lastModified, deleted);
    this.ttl = PASSWORDS_TTL;
  }
  public PasswordRecord(String guid, String collection, long lastModified) {
    this(guid, collection, lastModified, false);
  }
  public PasswordRecord(String guid, String collection) {
    this(guid, collection, 0, false);
  }
  public PasswordRecord(String guid) {
    this(guid, COLLECTION_NAME, 0, false);
  }
  public PasswordRecord() {
    this(Utils.generateGuid(), COLLECTION_NAME, 0, false);
  }

  public String id;
  public String hostname;
  public String formSubmitURL;
  public String httpRealm;
  // TODO these are encrypted in the passwords content provider,
  // need to figure out what we need to do here.
  public String usernameField;
  public String passwordField;
  public String encryptedUsername;
  public String encryptedPassword;
  public String encType;

  public long   timeCreated;
  public long   timeLastUsed;
  public long   timePasswordChanged;
  public long   timesUsed;


  @Override
  public Record copyWithIDs(String guid, long androidID) {
    PasswordRecord out = new PasswordRecord(guid, this.collection, this.lastModified, this.deleted);
    out.androidID = androidID;
    out.sortIndex = this.sortIndex;
    out.ttl       = this.ttl;

    // Copy PasswordRecord fields.
    out.id            = this.id;
    out.hostname      = this.hostname;
    out.formSubmitURL = this.formSubmitURL;
    out.httpRealm     = this.httpRealm;

    out.usernameField       = this.usernameField;
    out.passwordField       = this.passwordField;
    out.encryptedUsername   = this.encryptedUsername;
    out.encryptedPassword   = this.encryptedPassword;
    out.encType             = this.encType;

    out.timeCreated         = this.timeCreated;
    out.timeLastUsed        = this.timeLastUsed;
    out.timePasswordChanged = this.timePasswordChanged;
    out.timesUsed           = this.timesUsed;

    return out;
  }

  @Override
  public void initFromPayload(ExtendedJSONObject payload) {
    this.hostname = payload.getString(PAYLOAD_HOSTNAME);
    this.formSubmitURL = payload.getString(PAYLOAD_FORM_SUBMIT_URL);
    this.httpRealm = payload.getString(PAYLOAD_HTTP_REALM);
    this.encryptedUsername = payload.getString(PAYLOAD_USERNAME);
    this.encryptedPassword = payload.getString(PAYLOAD_PASSWORD);
    this.usernameField = payload.getString(PAYLOAD_USERNAME_FIELD);
    this.passwordField = payload.getString(PAYLOAD_PASSWORD_FIELD);
  }

  @Override
  public void populatePayload(ExtendedJSONObject payload) {
    putPayload(payload, PAYLOAD_HOSTNAME, this.hostname);
    putPayload(payload, PAYLOAD_FORM_SUBMIT_URL, this.formSubmitURL);
    putPayload(payload, PAYLOAD_HTTP_REALM, this.httpRealm);
    putPayload(payload, PAYLOAD_USERNAME, this.encryptedUsername);
    putPayload(payload, PAYLOAD_PASSWORD, this.encryptedPassword);
    putPayload(payload, PAYLOAD_USERNAME_FIELD, this.usernameField);
    putPayload(payload, PAYLOAD_PASSWORD_FIELD, this.passwordField);
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
        // Bug 738347 - SQLiteBridge does not check for nulls in ContentValues.
        // && RepoUtils.stringsEqual(this.httpRealm, other.httpRealm)
        // && RepoUtils.stringsEqual(this.encType, other.encType)
        && RepoUtils.stringsEqual(this.usernameField, other.usernameField)
        && RepoUtils.stringsEqual(this.passwordField, other.passwordField)
        && RepoUtils.stringsEqual(this.encryptedUsername, other.encryptedUsername)
        && RepoUtils.stringsEqual(this.encryptedPassword, other.encryptedPassword);
  }

  @Override
  public boolean equalPayloads(Object o) {
    if (o == null || !(o instanceof PasswordRecord)) {
      return false;
    }

    PasswordRecord other = (PasswordRecord) o;
    Logger.debug("PasswordRecord", "thisRecord:" + this.toString());
    Logger.debug("PasswordRecord", "otherRecord:" + o.toString());

    if (this.deleted) {
      if (other.deleted) {
        // Deleted records are equal if their guids match.
        return RepoUtils.stringsEqual(this.guid, other.guid);
      }
      // One record is deleted, the other is not. Not equal.
      return false;
    }

    if (!super.equalPayloads(other)) {
      Logger.debug(LOG_TAG, "super.equalPayloads returned false.");
      return false;
    }

    return RepoUtils.stringsEqual(this.hostname, other.hostname)
        && RepoUtils.stringsEqual(this.formSubmitURL, other.formSubmitURL)
        // Bug 738347 - SQLiteBridge does not check for nulls in ContentValues.
        // && RepoUtils.stringsEqual(this.httpRealm, other.httpRealm)
        // && RepoUtils.stringsEqual(this.encType, other.encType)
        && RepoUtils.stringsEqual(this.usernameField, other.usernameField)
        && RepoUtils.stringsEqual(this.passwordField, other.passwordField)
        && RepoUtils.stringsEqual(this.encryptedUsername, other.encryptedUsername)
        && RepoUtils.stringsEqual(this.encryptedPassword, other.encryptedPassword);
        // Desktop sync never sets timeCreated so this isn't relevant for sync records.
  }

  @Override
  public String toString() {
    return "PasswordRecord {"
        + "lastModified: " + this.lastModified + ", "
        + "hostname null?: " + (this.hostname == null) + ", "
        + "formSubmitURL null?: " + (this.formSubmitURL == null) + ", "
        + "httpRealm null?: " + (this.httpRealm == null) + ", "
        + "usernameField null?: " + (this.usernameField == null) + ", "
        + "passwordField null?: " + (this.passwordField == null) + ", "
        + "encryptedUsername null?: " + (this.encryptedUsername == null) + ", "
        + "encryptedPassword null?: " + (this.encryptedPassword == null) + ", "
        + "encType: " + this.encType + ", "
        + "timeCreated: " + this.timeCreated + ", "
        + "timeLastUsed: " + this.timeLastUsed + ", "
        + "timePasswordChanged: " + this.timePasswordChanged + ", "
        + "timesUsed: " + this.timesUsed;
  }
}
