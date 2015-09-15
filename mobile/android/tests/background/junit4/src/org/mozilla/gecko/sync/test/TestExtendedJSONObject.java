/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.test;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.IOException;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.json.simple.parser.ParseException;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.NonArrayJSONException;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.UnexpectedJSONException.BadRequiredFieldJSONException;
import org.robolectric.RobolectricGradleTestRunner;

@RunWith(RobolectricGradleTestRunner.class)
public class TestExtendedJSONObject {
  public static String exampleJSON = "{\"modified\":1233702554.25,\"success\":[\"{GXS58IDC}12\",\"{GXS58IDC}13\",\"{GXS58IDC}15\",\"{GXS58IDC}16\",\"{GXS58IDC}18\",\"{GXS58IDC}19\"],\"failed\":{\"{GXS58IDC}11\":[\"invalid parentid\"],\"{GXS58IDC}14\":[\"invalid parentid\"],\"{GXS58IDC}17\":[\"invalid parentid\"],\"{GXS58IDC}20\":[\"invalid parentid\"]}}";
  public static String exampleIntegral = "{\"modified\":1233702554,}";

  @Test
  public void testDeepCopy() throws NonObjectJSONException, IOException, ParseException, NonArrayJSONException {
    ExtendedJSONObject a = new ExtendedJSONObject(exampleJSON);
    ExtendedJSONObject c = a.deepCopy();
    assertTrue(a != c);
    assertTrue(a.equals(c));
    assertTrue(a.get("modified") == c.get("modified"));
    assertTrue(a.getArray("success") != c.getArray("success"));
    assertTrue(a.getArray("success").equals(c.getArray("success")));
  }

  @Test
  public void testFractional() throws IOException, ParseException, NonObjectJSONException {
    ExtendedJSONObject o = new ExtendedJSONObject(exampleJSON);
    assertTrue(o.containsKey("modified"));
    assertTrue(o.containsKey("success"));
    assertTrue(o.containsKey("failed"));
    assertFalse(o.containsKey(" "));
    assertFalse(o.containsKey(""));
    assertFalse(o.containsKey("foo"));
    assertTrue(o.get("modified") instanceof Number);
    assertTrue(o.get("modified").equals(Double.parseDouble("1233702554.25")));
    assertEquals(Long.valueOf(1233702554250L), o.getTimestamp("modified"));
    assertEquals(null, o.getTimestamp("foo"));
  }

  @Test
  public void testIntegral() throws IOException, ParseException, NonObjectJSONException {
    ExtendedJSONObject o = new ExtendedJSONObject(exampleIntegral);
    assertTrue(o.containsKey("modified"));
    assertFalse(o.containsKey("success"));
    assertTrue(o.get("modified") instanceof Number);
    assertTrue(o.get("modified").equals(Long.parseLong("1233702554")));
    assertEquals(Long.valueOf(1233702554000L), o.getTimestamp("modified"));
    assertEquals(null, o.getTimestamp("foo"));
  }

  @Test
  public void testSafeInteger() {
    ExtendedJSONObject o = new ExtendedJSONObject();
    o.put("integer", Integer.valueOf(5));
    o.put("double",  Double.valueOf(1.2));
    o.put("string",  "66");
    o.put("object",  new ExtendedJSONObject());
    o.put("null",    null);

    assertEquals(Integer.valueOf(5),  o.getIntegerSafely("integer"));
    assertEquals(Integer.valueOf(66), o.getIntegerSafely("string"));
    assertNull(o.getIntegerSafely(null));
  }

  @Test
  public void testParseJSONArray() throws Exception {
    JSONArray result = ExtendedJSONObject.parseJSONArray("[0, 1, {\"test\": 2}]");
    assertNotNull(result);

    assertThat((Long) result.get(0), is(equalTo(0L)));
    assertThat((Long) result.get(1), is(equalTo(1L)));
    assertThat((Long) ((JSONObject) result.get(2)).get("test"), is(equalTo(2L)));
  }

  @Test
  public void testBadParseJSONArray() throws Exception {
    try {
      ExtendedJSONObject.parseJSONArray("[0, ");
      fail();
    } catch (ParseException e) {
      // Do nothing.
    }

    try {
      ExtendedJSONObject.parseJSONArray("{}");
      fail();
    } catch (NonArrayJSONException e) {
      // Do nothing.
    }
  }

  @Test
  public void testParseUTF8AsJSONObject() throws Exception {
    String TEST = "{\"key\":\"value\"}";

    ExtendedJSONObject o = ExtendedJSONObject.parseUTF8AsJSONObject(TEST.getBytes("UTF-8"));
    assertNotNull(o);
    assertEquals("value", o.getString("key"));
  }

  @Test
  public void testBadParseUTF8AsJSONObject() throws Exception {
    try {
      ExtendedJSONObject.parseUTF8AsJSONObject("{}".getBytes("UTF-16"));
      fail();
    } catch (ParseException e) {
      // Do nothing.
    }

    try {
      ExtendedJSONObject.parseUTF8AsJSONObject("{".getBytes("UTF-8"));
      fail();
    } catch (ParseException e) {
      // Do nothing.
    }
  }

  @Test
  public void testHashCode() throws Exception {
    ExtendedJSONObject o = new ExtendedJSONObject(exampleJSON);
    assertEquals(o.hashCode(), o.hashCode());
    ExtendedJSONObject p = new ExtendedJSONObject(exampleJSON);
    assertEquals(o.hashCode(), p.hashCode());

    ExtendedJSONObject q = new ExtendedJSONObject(exampleJSON);
    q.put("modified", 0);
    assertNotSame(o.hashCode(), q.hashCode());
  }

  @Test
  public void testEquals() throws Exception {
    ExtendedJSONObject o = new ExtendedJSONObject(exampleJSON);
    ExtendedJSONObject p = new ExtendedJSONObject(exampleJSON);
    assertEquals(o, p);

    ExtendedJSONObject q = new ExtendedJSONObject(exampleJSON);
    q.put("modified", 0);
    assertNotSame(o, q);
    q.put("modified", o.get("modified"));
    assertEquals(o, q);
  }

  @Test
  public void testGetBoolean() throws Exception {
    ExtendedJSONObject o = new ExtendedJSONObject("{\"truekey\":true, \"falsekey\":false, \"stringkey\":\"string\"}");
    assertEquals(true, o.getBoolean("truekey"));
    assertEquals(false, o.getBoolean("falsekey"));
    try {
      o.getBoolean("stringkey");
      fail();
    } catch (Exception e) {
      assertTrue(e instanceof ClassCastException);
    }
    assertEquals(null, o.getBoolean("missingkey"));
  }

  @Test
  public void testNullLong() throws Exception {
    ExtendedJSONObject o = new ExtendedJSONObject("{\"x\": null}");
    Long x = o.getLong("x");
    assertNull(x);

    long y = o.getLong("x", 5L);
    assertEquals(5L, y);
  }

  protected void assertException(ExtendedJSONObject o, String[] requiredFields, Class<?> requiredFieldClass) {
    try {
      o.throwIfFieldsMissingOrMisTyped(requiredFields, requiredFieldClass);
      fail();
    } catch (Exception e) {
      assertTrue(e instanceof BadRequiredFieldJSONException);
    }
  }

  @Test
  public void testThrow() throws Exception {
    ExtendedJSONObject o = new ExtendedJSONObject("{\"true\":true, \"false\":false, \"string\":\"string\", \"long\":40000000000, \"int\":40, \"nested\":{\"inner\":10}}");
    o.throwIfFieldsMissingOrMisTyped(new String[] { "true", "false" }, Boolean.class);
    o.throwIfFieldsMissingOrMisTyped(new String[] { "string" }, String.class);
    o.throwIfFieldsMissingOrMisTyped(new String[] { "long" }, Long.class);
    o.throwIfFieldsMissingOrMisTyped(new String[] { "int" }, Long.class);
    o.throwIfFieldsMissingOrMisTyped(new String[] { "int" }, null);

    // Perhaps a bit unexpected, but we'll document it here.
    o.throwIfFieldsMissingOrMisTyped(new String[] { "nested" }, JSONObject.class);

    // Should fail.
    assertException(o, new String[] { "int" }, Integer.class); // Irritating, but...
    assertException(o, new String[] { "long" }, Integer.class); // Ditto.
    assertException(o, new String[] { "missing" }, String.class);
    assertException(o, new String[] { "missing" }, null);
    assertException(o, new String[] { "string", "int" }, String.class); // Irritating, but...
  }
}
