/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Rondpoint = ChromeUtils.import(
  "resource://gre/modules/components-utils/RustRondpoint.jsm"
);

const {
  Dictionnaire,
  Enumeration,
  copieDictionnaire,
  copieEnumeration,
  copieEnumerations,
  copieCarte,
  EnumerationAvecDonnees,
  switcheroo,
  Retourneur,
  DictionnaireNombresSignes,
  DictionnaireNombres,
  Stringifier,
  Optionneur,
  OptionneurDictionnaire,
} = Rondpoint;
add_task(async function() {
  const dico = new Dictionnaire(Enumeration.DEUX, true, 0, 1235);
  const copyDico = await copieDictionnaire(dico);
  Assert.deepEqual(dico, copyDico);

  Assert.equal(await copieEnumeration(Enumeration.DEUX), Enumeration.DEUX);
  Assert.deepEqual(
    await copieEnumerations([Enumeration.UN, Enumeration.DEUX]),
    [Enumeration.UN, Enumeration.DEUX]
  );
  const obj = {
    "0": new EnumerationAvecDonnees.Zero(),
    "1": new EnumerationAvecDonnees.Un(1),
    "2": new EnumerationAvecDonnees.Deux(2, "deux"),
  };

  Assert.deepEqual(await copieCarte(obj), obj);

  const zero = new EnumerationAvecDonnees.Zero();
  const one = new EnumerationAvecDonnees.Un(1);
  const two = new EnumerationAvecDonnees.Deux(2);
  Assert.notEqual(zero, one);
  Assert.notEqual(one, two);

  Assert.deepEqual(zero, new EnumerationAvecDonnees.Zero());
  Assert.deepEqual(one, new EnumerationAvecDonnees.Un(1));
  Assert.notDeepEqual(one, new EnumerationAvecDonnees.Un(4));

  Assert.ok(await switcheroo(false));
  // Test the roundtrip across the FFI.
  // This shows that the values we send come back in exactly the same state as we sent them.
  // i.e. it shows that lowering from JS and lifting into rust is symmetrical with
  //      lowering from rust and lifting into JS.

  const rt = await Retourneur.init();

  const affirmAllerRetour = async (arr, fn, equalFn) => {
    for (const member of arr) {
      if (equalFn) {
        equalFn(await fn(member), member);
      } else {
        Assert.equal(await fn(member), member);
      }
    }
  };

  // Booleans
  await affirmAllerRetour([true, false], rt.identiqueBoolean.bind(rt));

  // Bytes
  await affirmAllerRetour([-128, 127], rt.identiqueI8.bind(rt));
  await affirmAllerRetour([0, 0xff], rt.identiqueU8.bind(rt));

  // Shorts
  await affirmAllerRetour([-32768, 32767], rt.identiqueI16.bind(rt));
  await affirmAllerRetour([0, 0xffff], rt.identiqueU16.bind(rt));

  // Ints
  await affirmAllerRetour(
    [0, 1, -1, -2147483648, 2147483647],
    rt.identiqueI32.bind(rt)
  );
  await affirmAllerRetour([0, 0xffffffff], rt.identiqueU32.bind(rt));

  // Longs
  // NOTE: we cannot represent greater than `Number.MAX_SAFE_INTEGER`
  await affirmAllerRetour(
    [0, 1, -1, Number.MAX_SAFE_INTEGER, Number.MIN_SAFE_INTEGER],
    rt.identiqueI64.bind(rt)
  );
  await affirmAllerRetour(
    [0, Number.MAX_SAFE_INTEGER],
    rt.identiqueU64.bind(rt)
  );

  // Floats
  const equalFloats = (a, b) => Assert.ok(Math.abs(a - b) <= Number.EPSILON);
  await affirmAllerRetour(
    [0.0, 0.5, 0.25, 1.5],
    rt.identiqueFloat.bind(rt),
    equalFloats
  );
  //   Some float value's precision gets messed up, an example is 3.22, 100.223, etc
  //   await affirmAllerRetour([0.0, 0.5, 0.25, 1.5, 100.223], rt.identiqueFloat.bind(rt), equalFloats);

  // Double (although on the JS side doubles are limited since they are also represented by Number)
  await affirmAllerRetour(
    [0.0, 0.5, 0.25, 1.5],
    rt.identiqueDouble.bind(rt),
    equalFloats
  );

  // Strings
  await affirmAllerRetour(
    [
      "",
      "abc",
      "null\u0000byte",
      "été",
      "ښي لاس ته لوستلو لوستل",
      "😻emoji 👨‍👧‍👦multi-emoji, 🇨🇭a flag, a canal, panama",
    ],
    rt.identiqueString.bind(rt)
  );

  await affirmAllerRetour(
    [-1, 0, 1].map(n => new DictionnaireNombresSignes(n, n, n, n)),
    rt.identiqueNombresSignes.bind(rt),
    (a, b) => Assert.deepEqual(a, b)
  );

  await affirmAllerRetour(
    [0, 1].map(n => new DictionnaireNombres(n, n, n, n)),
    rt.identiqueNombres.bind(rt),
    (a, b) => Assert.deepEqual(a, b)
  );

  // Test one way across the FFI.
  //
  // We send one representation of a value to lib.rs, and it transforms it into another, a string.
  // lib.rs sends the string back, and then we compare here in js.
  //
  // This shows that the values are transformed into strings the same way in both js and rust.
  // i.e. if we assume that the string return works (we test this assumption elsewhere)
  //      we show that lowering from js and lifting into rust has values that both js and rust
  //      both stringify in the same way. i.e. the same values.
  //
  // If we roundtripping proves the symmetry of our lowering/lifting from here to rust, and lowering/lifting from rust to here,
  // and this convinces us that lowering/lifting from here to rust is correct, then
  // together, we've shown the correctness of the return leg.
  const st = await Stringifier.init();

  const affirmEnchaine = async (arr, fn) => {
    for (const member of arr) {
      Assert.equal(await fn(member), String(member));
    }
  };

  // Booleans
  await affirmEnchaine([true, false], st.toStringBoolean.bind(st));

  // Bytes
  await affirmEnchaine([-128, 127], st.toStringI8.bind(st));
  await affirmEnchaine([0, 0xff], st.toStringU8.bind(st));

  // Shorts
  await affirmEnchaine([-32768, 32767], st.toStringI16.bind(st));
  await affirmEnchaine([0, 0xffff], st.toStringU16.bind(st));

  // Ints
  await affirmEnchaine(
    [0, 1, -1, -2147483648, 2147483647],
    st.toStringI32.bind(st)
  );
  await affirmEnchaine([0, 0xffffffff], st.toStringU32.bind(st));

  // Longs
  // NOTE: we cannot represent greater than `Number.MAX_SAFE_INTEGER`
  await affirmEnchaine(
    [0, 1, -1, Number.MAX_SAFE_INTEGER, Number.MIN_SAFE_INTEGER],
    st.toStringI64.bind(st)
  );
  await affirmEnchaine([0, Number.MAX_SAFE_INTEGER], st.toStringU64.bind(st));

  // Floats
  await affirmEnchaine([0.0, 0.5, 0.25, 1.5], st.toStringFloat.bind(st));

  // Doubles
  await affirmEnchaine([0.0, 0.5, 0.25, 1.5], st.toStringDouble.bind(st));

  // Prove to ourselves that default arguments are being used.
  // Step 1: call the methods without arguments, and check against the UDL.
  const op = await Optionneur.init();

  Assert.equal(await op.sinonString(), "default");

  Assert.ok(!(await op.sinonBoolean()));

  Assert.deepEqual(await op.sinonSequence(), []);

  Assert.equal(await op.sinonNull(), null);
  Assert.equal(await op.sinonZero(), 0);

  // decimal integers
  Assert.equal(await op.sinonI8Dec(), -42);
  Assert.equal(await op.sinonU8Dec(), 42);
  Assert.equal(await op.sinonI16Dec(), 42);
  Assert.equal(await op.sinonU16Dec(), 42);
  Assert.equal(await op.sinonI32Dec(), 42);
  Assert.equal(await op.sinonU32Dec(), 42);
  Assert.equal(await op.sinonI64Dec(), 42);
  Assert.equal(await op.sinonU64Dec(), 42);

  // hexadecimal integers
  Assert.equal(await op.sinonI8Hex(), -0x7f);
  Assert.equal(await op.sinonU8Hex(), 0xff);
  Assert.equal(await op.sinonI16Hex(), 0x7f);
  Assert.equal(await op.sinonU16Hex(), 0xffff);
  Assert.equal(await op.sinonI32Hex(), 0x7fffffff);
  Assert.equal(await op.sinonU32Hex(), 0xffffffff);
  // The following are too big to be represented by js `Number`
  // Assert.equal(await op.sinonI64Hex(), 0x7fffffffffffffff);
  // Assert.equal(await op.sinonU64Hex(), 0xffffffffffffffff);

  // octal integers
  Assert.equal(await op.sinonU32Oct(), 0o755);

  // floats
  Assert.equal(await op.sinonF32(), 42.0);
  Assert.equal(await op.sinonF64(), 42.1);

  // enums
  Assert.equal(await op.sinonEnum(), Enumeration.TROIS);

  // Step 2. Convince ourselves that if we pass something else, then that changes the output.
  //         We have shown something coming out of the sinon methods, but without eyeballing the Rust
  //         we can't be sure that the arguments will change the return value.

  await affirmAllerRetour(["foo", "bar"], op.sinonString.bind(op));
  await affirmAllerRetour([true, false], op.sinonBoolean.bind(op));
  await affirmAllerRetour([["a", "b"], []], op.sinonSequence.bind(op), (a, b) =>
    Assert.deepEqual(a, b)
  );

  // Optionals
  await affirmAllerRetour(["0", "1"], op.sinonNull.bind(op));
  await affirmAllerRetour([0, 1], op.sinonZero.bind(op));

  // integers
  await affirmAllerRetour([0, 1], op.sinonU8Dec.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI8Dec.bind(op));
  await affirmAllerRetour([0, 1], op.sinonU16Dec.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI16Dec.bind(op));
  await affirmAllerRetour([0, 1], op.sinonU32Dec.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI32Dec.bind(op));
  await affirmAllerRetour([0, 1], op.sinonU64Dec.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI64Dec.bind(op));

  await affirmAllerRetour([0, 1], op.sinonU8Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI8Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonU16Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI16Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonU32Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI32Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonU64Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonI64Hex.bind(op));
  await affirmAllerRetour([0, 1], op.sinonU32Oct.bind(op));

  // Floats
  await affirmAllerRetour([0.0, 1.0], op.sinonF32.bind(op));
  await affirmAllerRetour([0.0, 1.0], op.sinonF64.bind(op));

  // enums
  await affirmAllerRetour(
    [Enumeration.UN, Enumeration.DEUX, Enumeration.TROIS],
    op.sinonEnum.bind(op)
  );

  // Testing defaulting properties in record types.
  const defaultes = new OptionneurDictionnaire();
  const explicite = new OptionneurDictionnaire(
    -8,
    8,
    -16,
    0x10,
    -32,
    32,
    -64,
    64,
    4.0,
    8.0,
    true,
    "default",
    [],
    Enumeration.DEUX,
    null
  );

  Assert.deepEqual(defaultes, explicite);

  // …and makes sure they travel across and back the FFI.

  await affirmAllerRetour(
    [defaultes, explicite],
    rt.identiqueOptionneurDictionnaire.bind(rt),
    (a, b) => Assert.deepEqual(a, b)
  );
});
