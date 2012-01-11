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

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.StringReader;
import java.util.Map;
import java.util.Map.Entry;

import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;

/**
 * Extend JSONObject to do little things, like, y'know, accessing members.
 *
 * @author rnewman
 *
 */
public class ExtendedJSONObject {

  public JSONObject object;

  public static Object parse(InputStreamReader reader) throws IOException, ParseException {
    Object parseOutput = new JSONParser().parse(reader);
    if (parseOutput instanceof JSONObject) {
      return new ExtendedJSONObject((JSONObject) parseOutput);
    } else {
      return parseOutput;
    }
  }

  public static Object parse(InputStream stream) throws IOException, ParseException {
    InputStreamReader reader = new InputStreamReader(stream, "UTF-8");
    return ExtendedJSONObject.parse(reader);
  }

  /**
   * Helper method to get a JSONObject from a String. Input: String containing
   * JSON. Output: Extracted JSONObject. Throws: Exception if JSON is invalid.
   *
   * @throws ParseException
   * @throws IOException
   * @throws NonObjectJSONException
   *           If the object is valid JSON, but not an object.
   */
  public static ExtendedJSONObject parseJSONObject(String jsonString)
                                                                     throws IOException,
                                                                     ParseException,
                                                                     NonObjectJSONException {
    return new ExtendedJSONObject(jsonString);
  }

  public ExtendedJSONObject() {
    this.object = new JSONObject();
  }

  public ExtendedJSONObject(JSONObject o) {
    this.object = o;
  }

  public ExtendedJSONObject(String jsonString) throws IOException, ParseException, NonObjectJSONException {
    if (jsonString == null) {
      this.object = new JSONObject();
      return;
    }
    Reader in = new StringReader(jsonString);
    Object obj = new JSONParser().parse(in);
    if (obj instanceof JSONObject) {
      this.object = ((JSONObject) obj);
    } else {
      throw new NonObjectJSONException(obj);
    }
  }

  // Passthrough methods.
  public Object get(String key) {
    return this.object.get(key);
  }
  public Long getLong(String key) {
    return (Long) this.get(key);
  }

  public boolean containsKey(String key) {
    return this.object.containsKey(key);
  }

  public String toJSONString() {
    return this.object.toJSONString();
  }

  public String toString() {
    return this.object.toString();
  }

  public void put(String key, Object value) {
    @SuppressWarnings("unchecked")
    Map<Object, Object> map = this.object;
    map.put(key, value);
  }

  public ExtendedJSONObject getObject(String key) throws NonObjectJSONException {
    Object o = this.object.get(key);
    if (o instanceof ExtendedJSONObject) {
      return (ExtendedJSONObject) o;
    }
    if (o instanceof JSONObject) {
      return new ExtendedJSONObject((JSONObject) o);
    }
    throw new NonObjectJSONException(o);
  }

  public ExtendedJSONObject clone() {
    return new ExtendedJSONObject((JSONObject) this.object.clone());
  }

  /**
   * Helper method for extracting a JSONObject from its string encoding within
   * another JSONObject.
   *
   * Input: JSONObject and key. Output: JSONObject extracted. Throws: Exception
   * if JSON is invalid.
   *
   * @throws NonObjectJSONException
   * @throws ParseException
   * @throws IOException
   */
  public ExtendedJSONObject getJSONObject(String key) throws IOException,
                                                     ParseException,
                                                     NonObjectJSONException {
    String val = (String) this.object.get(key);
    return ExtendedJSONObject.parseJSONObject(val);
  }

  @SuppressWarnings("unchecked")
  public Iterable<Entry<String, Object>> entryIterable() {
    return this.object.entrySet();
  }
}
