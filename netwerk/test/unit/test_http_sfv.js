"use strict";

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const gService = Cc["@mozilla.org/http-sfv-service;1"].getService(
  Ci.nsISFVService
);

add_task(async function test_sfv_bare_item() {
  // tests bare item
  let item_int = gService.newInteger(19);
  Assert.equal(item_int.value, 19, "check bare item value");

  let item_bool = gService.newBool(true);
  Assert.equal(item_bool.value, true, "check bare item value");
  item_bool.value = false;
  Assert.equal(item_bool.value, false, "check bare item value");

  let item_float = gService.newDecimal(145.45);
  Assert.equal(item_float.value, 145.45);

  let item_str = gService.newString("some_string");
  Assert.equal(item_str.value, "some_string", "check bare item value");

  let item_byte_seq = gService.newByteSequence("aGVsbG8=");
  Assert.equal(item_byte_seq.value, "aGVsbG8=", "check bare item value");

  let item_token = gService.newToken("*a");
  Assert.equal(item_token.value, "*a", "check bare item value");
});

add_task(async function test_sfv_params() {
  // test params
  let params = gService.newParameters();
  let bool_param = gService.newBool(false);
  let int_param = gService.newInteger(15);
  let decimal_param = gService.newDecimal(15.45);

  params.set("bool_param", bool_param);
  params.set("int_param", int_param);
  params.set("decimal_param", decimal_param);

  Assert.throws(
    () => {
      params.get("some_param");
    },
    /NS_ERROR_UNEXPECTED/,
    "must throw exception as key does not exist in parameters"
  );
  Assert.equal(
    params.get("bool_param").QueryInterface(Ci.nsISFVBool).value,
    false,
    "get parameter by key and check its value"
  );
  Assert.equal(
    params.get("int_param").QueryInterface(Ci.nsISFVInteger).value,
    15,
    "get parameter by key and check its value"
  );
  Assert.equal(
    params.get("decimal_param").QueryInterface(Ci.nsISFVDecimal).value,
    15.45,
    "get parameter by key and check its value"
  );
  Assert.deepEqual(
    params.keys(),
    ["bool_param", "int_param", "decimal_param"],
    "check that parameters contain all the expected keys"
  );

  params.delete("int_param");
  Assert.deepEqual(
    params.keys(),
    ["bool_param", "decimal_param"],
    "check that parameter has been deleted"
  );

  Assert.throws(
    () => {
      params.delete("some_param");
    },
    /NS_ERROR_UNEXPECTED/,
    "must throw exception upon attempt to delete by non-existing key"
  );
});

add_task(async function test_sfv_inner_list() {
  // create primitives for inner list
  let item1_params = gService.newParameters();
  item1_params.set("param_1", gService.newToken("*smth"));
  let item1 = gService.newItem(gService.newDecimal(172.145865), item1_params);

  let item2_params = gService.newParameters();
  item2_params.set("param_1", gService.newBool(true));
  item2_params.set("param_2", gService.newInteger(145454));
  let item2 = gService.newItem(
    gService.newByteSequence("weather"),
    item2_params
  );

  // create inner list
  let inner_list_params = gService.newParameters();
  inner_list_params.set("inner_param", gService.newByteSequence("tests"));
  let inner_list = gService.newInnerList([item1, item2], inner_list_params);

  // check inner list members & params
  let inner_list_members = inner_list.QueryInterface(Ci.nsISFVInnerList).items;
  let inner_list_parameters = inner_list
    .QueryInterface(Ci.nsISFVInnerList)
    .params.QueryInterface(Ci.nsISFVParams);
  Assert.equal(inner_list_members.length, 2, "check inner list length");

  let inner_item1 = inner_list_members[0].QueryInterface(Ci.nsISFVItem);
  Assert.equal(
    inner_item1.value.QueryInterface(Ci.nsISFVDecimal).value,
    172.145865,
    "check inner list member value"
  );

  let inner_item2 = inner_list_members[1].QueryInterface(Ci.nsISFVItem);
  Assert.equal(
    inner_item2.value.QueryInterface(Ci.nsISFVByteSeq).value,
    "weather",
    "check inner list member value"
  );

  Assert.equal(
    inner_list_parameters.get("inner_param").QueryInterface(Ci.nsISFVByteSeq)
      .value,
    "tests",
    "get inner list parameter by key and check its value"
  );
});

add_task(async function test_sfv_item() {
  // create parameters for item
  let params = gService.newParameters();
  let param1 = gService.newBool(false);
  let param2 = gService.newString("str_value");
  let param3 = gService.newBool(true);
  params.set("param_1", param1);
  params.set("param_2", param2);
  params.set("param_3", param3);

  // create item field
  let item = gService.newItem(gService.newToken("*abc"), params);

  Assert.equal(
    item.value.QueryInterface(Ci.nsISFVToken).value,
    "*abc",
    "check items's value"
  );
  Assert.equal(
    item.params.get("param_1").QueryInterface(Ci.nsISFVBool).value,
    false,
    "get item parameter by key and check its value"
  );
  Assert.equal(
    item.params.get("param_2").QueryInterface(Ci.nsISFVString).value,
    "str_value",
    "get item parameter by key and check its value"
  );
  Assert.equal(
    item.params.get("param_3").QueryInterface(Ci.nsISFVBool).value,
    true,
    "get item parameter by key and check its value"
  );

  // check item field serialization
  let serialized = item.serialize();
  Assert.equal(
    serialized,
    `*abc;param_1=?0;param_2="str_value";param_3`,
    "serialized output must match expected one"
  );
});

add_task(async function test_sfv_list() {
  // create primitives for List
  let item1_params = gService.newParameters();
  item1_params.set("param_1", gService.newToken("*smth"));
  let item1 = gService.newItem(gService.newDecimal(145454.14568), item1_params);

  let item2_params = gService.newParameters();
  item2_params.set("param_1", gService.newBool(true));
  let item2 = gService.newItem(
    gService.newByteSequence("weather"),
    item2_params
  );

  let inner_list = gService.newInnerList(
    [item1, item2],
    gService.newParameters()
  );

  // create list field
  let list = gService.newList([item1, inner_list]);

  // check list's members
  let list_members = list.members;
  Assert.equal(list_members.length, 2, "check list length");

  // check list's member of item type
  let member1 = list_members[0].QueryInterface(Ci.nsISFVItem);
  Assert.equal(
    member1.value.QueryInterface(Ci.nsISFVDecimal).value,
    145454.14568,
    "check list member's value"
  );
  let member1_parameters = member1.params;
  Assert.equal(
    member1_parameters.get("param_1").QueryInterface(Ci.nsISFVToken).value,
    "*smth",
    "get list member's parameter by key and check its value"
  );

  // check list's member of inner list type
  let inner_list_members = list_members[1].QueryInterface(Ci.nsISFVInnerList)
    .items;
  Assert.equal(inner_list_members.length, 2, "check inner list length");

  let inner_item1 = inner_list_members[0].QueryInterface(Ci.nsISFVItem);
  Assert.equal(
    inner_item1.value.QueryInterface(Ci.nsISFVDecimal).value,
    145454.14568,
    "check inner list member's value"
  );

  let inner_item2 = inner_list_members[1].QueryInterface(Ci.nsISFVItem);
  Assert.equal(
    inner_item2.value.QueryInterface(Ci.nsISFVByteSeq).value,
    "weather",
    "check inner list member's value"
  );

  // check inner list member's params
  list_members[1]
    .QueryInterface(Ci.nsISFVInnerList)
    .params.QueryInterface(Ci.nsISFVParams);

  // check serialization of list field
  let expected_serialized =
    "145454.146;param_1=*smth, (145454.146;param_1=*smth :d2VhdGhlcg==:;param_1)";
  let actual_serialized = list.serialize();
  Assert.equal(
    actual_serialized,
    expected_serialized,
    "serialized output must match expected one"
  );
});

add_task(async function test_sfv_dictionary() {
  // create primitives for dictionary field

  // dict member1
  let params1 = gService.newParameters();
  params1.set("mp_1", gService.newBool(true));
  params1.set("mp_2", gService.newDecimal(68.758602));
  let member1 = gService.newItem(gService.newString("member_1"), params1);

  // dict member2
  let params2 = gService.newParameters();
  let inner_item1 = gService.newItem(
    gService.newString("inner_item_1"),
    gService.newParameters()
  );
  let inner_item2 = gService.newItem(
    gService.newToken("tok"),
    gService.newParameters()
  );
  let member2 = gService.newInnerList([inner_item1, inner_item2], params2);

  // dict member3
  let params_3 = gService.newParameters();
  params_3.set("mp_1", gService.newInteger(6586));
  let member3 = gService.newItem(gService.newString("member_3"), params_3);

  // create dictionary field
  let dict = gService.newDictionary();
  dict.set("key_1", member1);
  dict.set("key_2", member2);
  dict.set("key_3", member3);

  // check dictionary keys
  let expected = ["key_1", "key_2", "key_3"];
  Assert.deepEqual(
    expected,
    dict.keys(),
    "check dictionary contains all the expected keys"
  );

  // check dictionary members
  Assert.throws(
    () => {
      dict.get("key_4");
    },
    /NS_ERROR_UNEXPECTED/,
    "must throw exception as key does not exist in dictionary"
  );

  // let dict_member1 = dict.get("key_1").QueryInterface(Ci.nsISFVItem);
  let dict_member2 = dict.get("key_2").QueryInterface(Ci.nsISFVInnerList);
  let dict_member3 = dict.get("key_3").QueryInterface(Ci.nsISFVItem);

  // Assert.equal(
  //   dict_member1.value.QueryInterface(Ci.nsISFVString).value,
  //   "member_1",
  //   "check dictionary member's value"
  // );
  // Assert.equal(
  //   dict_member1.params.get("mp_1").QueryInterface(Ci.nsISFVBool).value,
  //   true,
  //   "get dictionary member's parameter by key and check its value"
  // );
  // Assert.equal(
  //   dict_member1.params.get("mp_2").QueryInterface(Ci.nsISFVDecimal).value,
  //   "68.758602",
  //   "get dictionary member's parameter by key and check its value"
  // );

  let dict_member2_items = dict_member2.QueryInterface(Ci.nsISFVInnerList)
    .items;
  let dict_member2_params = dict_member2
    .QueryInterface(Ci.nsISFVInnerList)
    .params.QueryInterface(Ci.nsISFVParams);
  Assert.equal(
    dict_member2_items[0]
      .QueryInterface(Ci.nsISFVItem)
      .value.QueryInterface(Ci.nsISFVString).value,
    "inner_item_1",
    "get dictionary member of inner list type, and check inner list member's value"
  );
  Assert.equal(
    dict_member2_items[1]
      .QueryInterface(Ci.nsISFVItem)
      .value.QueryInterface(Ci.nsISFVToken).value,
    "tok",
    "get dictionary member of inner list type, and check inner list member's value"
  );
  Assert.throws(
    () => {
      dict_member2_params.get("some_param");
    },
    /NS_ERROR_UNEXPECTED/,
    "must throw exception as dict member's parameters are empty"
  );

  Assert.equal(
    dict_member3.value.QueryInterface(Ci.nsISFVString).value,
    "member_3",
    "check dictionary member's value"
  );
  Assert.equal(
    dict_member3.params.get("mp_1").QueryInterface(Ci.nsISFVInteger).value,
    6586,
    "get dictionary member's parameter by key and check its value"
  );

  // check serialization of Dictionary field
  let expected_serialized = `key_1="member_1";mp_1;mp_2=68.759, key_2=("inner_item_1" tok), key_3="member_3";mp_1=6586`;
  let actual_serialized = dict.serialize();
  Assert.equal(
    actual_serialized,
    expected_serialized,
    "serialized output must match expected one"
  );

  // check deleting dict member
  dict.delete("key_2");
  Assert.deepEqual(
    dict.keys(),
    ["key_1", "key_3"],
    "check that dictionary member has been deleted"
  );

  Assert.throws(
    () => {
      dict.delete("some_key");
    },
    /NS_ERROR_UNEXPECTED/,
    "must throw exception upon attempt to delete by non-existing key"
  );
});

add_task(async function test_sfv_item_parsing() {
  Assert.ok(gService.parseItem(`"str"`), "input must be parsed into Item");
  Assert.ok(gService.parseItem("12.35;a "), "input must be parsed into Item");
  Assert.ok(gService.parseItem("12.35;  a "), "input must be parsed into Item");
  Assert.ok(gService.parseItem("12.35    "), "input must be parsed into Item");

  Assert.throws(
    () => {
      gService.parseItem("12.35;\ta ");
    },
    /NS_ERROR_FAILURE/,
    "item parsing must fail: invalid parameters delimiter"
  );

  Assert.throws(
    () => {
      gService.parseItem("125666.3565648855455");
    },
    /NS_ERROR_FAILURE/,
    "item parsing must fail: decimal too long"
  );
});

add_task(async function test_sfv_list_parsing() {
  Assert.ok(
    gService.parseList(
      "(?1;param_1=*smth   :d2VhdGhlcg==:;param_1;param_2=145454);inner_param=:d2VpcmR0ZXN0cw==:"
    ),
    "input must be parsed into List"
  );
  Assert.ok("a, (b c)", "input must be parsed into List");

  Assert.throws(() => {
    gService.parseList("?tok", "list parsing must fail");
  }, /NS_ERROR_FAILURE/);

  Assert.throws(() => {
    gService.parseList(
      "a, (b, c)",
      "list parsing must fail: invalid delimiter within inner list"
    );
  }, /NS_ERROR_FAILURE/);

  Assert.throws(
    () => {
      gService.parseList("a, b c");
    },
    /NS_ERROR_FAILURE/,
    "list parsing must fail: invalid delimiter"
  );
});

add_task(async function test_sfv_dict_parsing() {
  Assert.ok(
    gService.parseDictionary(`abc=123;a=1;b=2, def=456, ghi=789;q=9;r="+w"`),
    "input must be parsed into Dictionary"
  );
  Assert.ok(
    gService.parseDictionary("a=1\t,\t\t\t   c=*"),
    "input must be parsed into Dictionary"
  );
  Assert.ok(
    gService.parseDictionary("a=1\t,\tc=*    \t\t"),
    "input must be parsed into Dictionary"
  );

  Assert.throws(
    () => {
      gService.parseDictionary("a=1\t,\tc=*,");
    },
    /NS_ERROR_FAILURE/,
    "dictionary parsing must fail: trailing comma"
  );

  Assert.throws(
    () => {
      gService.parseDictionary("a=1 c=*");
    },
    /NS_ERROR_FAILURE/,
    "dictionary parsing must fail: invalid delimiter"
  );

  Assert.throws(
    () => {
      gService.parseDictionary("INVALID_key=1, c=*");
    },
    /NS_ERROR_FAILURE/,
    "dictionary parsing must fail: invalid key format, can't be in uppercase"
  );
});

add_task(async function test_sfv_list_parse_serialize() {
  let list_field = gService.parseList("1  ,  42, (42 43)");
  Assert.equal(
    list_field.serialize(),
    "1, 42, (42 43)",
    "serialized output must match expected one"
  );

  // create new inner list with parameters
  let inner_list_params = gService.newParameters();
  inner_list_params.set("key1", gService.newString("value1"));
  inner_list_params.set("key2", gService.newBool(true));
  inner_list_params.set("key3", gService.newBool(false));
  let inner_list_items = [
    gService.newItem(
      gService.newDecimal(-1865.75653),
      gService.newParameters()
    ),
    gService.newItem(gService.newToken("token"), gService.newParameters()),
    gService.newItem(gService.newString(`no"yes`), gService.newParameters()),
  ];
  let new_list_member = gService.newInnerList(
    inner_list_items,
    inner_list_params
  );

  // set one of list members to inner list and check it's serialized as expected
  let members = list_field.members;
  members[1] = new_list_member;
  list_field.members = members;
  Assert.equal(
    list_field.serialize(),
    `1, (-1865.757 token "no\\"yes");key1="value1";key2;key3=?0, (42 43)`,
    "update list member and check list is serialized as expected"
  );
});

add_task(async function test_sfv_dict_parse_serialize() {
  let dict_field = gService.parseDictionary(
    "a=1,     b; foo=*, \tc=3, \t     \tabc=123;a=1;b=2\t"
  );
  Assert.equal(
    dict_field.serialize(),
    "a=1, b;foo=*, c=3, abc=123;a=1;b=2",
    "serialized output must match expected one"
  );

  // set new value for existing dict's key
  dict_field.set(
    "a",
    gService.newItem(gService.newInteger(165), gService.newParameters())
  );

  // add new member to dict
  dict_field.set(
    "key",
    gService.newItem(gService.newDecimal(45.0), gService.newParameters())
  );

  // check dict is serialized properly after the above changes
  Assert.equal(
    dict_field.serialize(),
    "a=165, b;foo=*, c=3, abc=123;a=1;b=2, key=45.0",
    "update dictionary members and dictionary list is serialized as expected"
  );
});

add_task(async function test_sfv_list_parse_more() {
  // check parsing of multiline header of List type
  let list_field = gService.parseList("(12 abc), 12.456\t\t   ");
  list_field.parseMore("11, 15, tok");
  Assert.equal(
    list_field.serialize(),
    "(12 abc), 12.456, 11, 15, tok",
    "multi-line field value parsed and serialized successfully"
  );

  // should fail parsing one more line
  Assert.throws(
    () => {
      list_field.parseMore("(tk\t1)");
    },
    /NS_ERROR_FAILURE/,
    "line parsing must fail: invalid delimiter in inner list"
  );
  Assert.equal(
    list_field.serialize(),
    "(12 abc), 12.456, 11, 15, tok",
    "parsed value must not change if parsing one more line of header fails"
  );
});

add_task(async function test_sfv_dict_parse_more() {
  // check parsing of multiline header of Dictionary type
  let dict_field = gService.parseDictionary("");
  dict_field.parseMore("key2=?0, key3=?1, key4=itm");
  dict_field.parseMore("key1,    key5=11, key4=45");

  Assert.equal(
    dict_field.serialize(),
    "key2=?0, key3, key4=45, key1, key5=11",
    "multi-line field value parsed and serialized successfully"
  );

  // should fail parsing one more line
  Assert.throws(
    () => {
      dict_field.parseMore("c=12, _k=13");
    },
    /NS_ERROR_FAILURE/,
    "line parsing must fail: invalid key format"
  );
});
