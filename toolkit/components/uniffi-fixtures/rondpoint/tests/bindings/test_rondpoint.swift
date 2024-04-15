import rondpoint

let dico = Dictionnaire(un: .deux, deux: false, petitNombre: 0, grosNombre: 123456789)
let copyDico = copieDictionnaire(d: dico)
assert(dico == copyDico)

assert(copieEnumeration(e: .deux) == .deux)
assert(copieEnumerations(e: [.un, .deux]) == [.un, .deux])
assert(copieCarte(c:
    ["0": .zero,
    "1": .un(premier: 1),
    "2": .deux(premier: 2, second: "deux")
]) == [
    "0": .zero,
    "1": .un(premier: 1),
    "2": .deux(premier: 2, second: "deux")
])

assert(EnumerationAvecDonnees.zero != EnumerationAvecDonnees.un(premier: 1))
assert(EnumerationAvecDonnees.un(premier: 1) == EnumerationAvecDonnees.un(premier: 1))
assert(EnumerationAvecDonnees.un(premier: 1) != EnumerationAvecDonnees.un(premier: 2))


assert(switcheroo(b: false))

// Test the roundtrip across the FFI.
// This shows that the values we send come back in exactly the same state as we sent them.
// i.e. it shows that lowering from swift and lifting into rust is symmetrical with 
//      lowering from rust and lifting into swift.
let rt = Retourneur()

// Booleans
[true, false].affirmAllerRetour(rt.identiqueBoolean)

// Bytes.
[.min, .max].affirmAllerRetour(rt.identiqueI8)
[0x00, 0xFF].map { $0 as UInt8 }.affirmAllerRetour(rt.identiqueU8)

// Shorts
[.min, .max].affirmAllerRetour(rt.identiqueI16)
[0x0000, 0xFFFF].map { $0 as UInt16 }.affirmAllerRetour(rt.identiqueU16)

// Ints
[0, 1, -1, .min, .max].affirmAllerRetour(rt.identiqueI32)
[0x00000000, 0xFFFFFFFF].map { $0 as UInt32 }.affirmAllerRetour(rt.identiqueU32)

// Longs
[.zero, 1, -1, .min, .max].affirmAllerRetour(rt.identiqueI64)
[.zero, 1, .min, .max].affirmAllerRetour(rt.identiqueU64)

// Floats
[.zero, 1, 0.25, .leastNonzeroMagnitude, .greatestFiniteMagnitude].affirmAllerRetour(rt.identiqueFloat)

// Doubles
[0.0, 1.0, .leastNonzeroMagnitude, .greatestFiniteMagnitude].affirmAllerRetour(rt.identiqueDouble)

// Strings
["", "abc", "null\0byte", "été", "ښي لاس ته لوستلو لوستل", "😻emoji 👨‍👧‍👦multi-emoji, 🇨🇭a flag, a canal, panama"]
    .affirmAllerRetour(rt.identiqueString)

// Test one way across the FFI.
//
// We send one representation of a value to lib.rs, and it transforms it into another, a string.
// lib.rs sends the string back, and then we compare here in swift.
//
// This shows that the values are transformed into strings the same way in both swift and rust.
// i.e. if we assume that the string return works (we test this assumption elsewhere)
//      we show that lowering from swift and lifting into rust has values that both swift and rust
//      both stringify in the same way. i.e. the same values.
//
// If we roundtripping proves the symmetry of our lowering/lifting from here to rust, and lowering/lifting from rust t here,
// and this convinces us that lowering/lifting from here to rust is correct, then 
// together, we've shown the correctness of the return leg.
let st = Stringifier()

// Test the effigacy of the string transport from rust. If this fails, but everything else 
// works, then things are very weird.
let wellKnown = st.wellKnownString(value: "swift")
assert("uniffi 💚 swift!" == wellKnown, "wellKnownString 'uniffi 💚 swift!' == '\(wellKnown)'")

// Booleans
[true, false].affirmEnchaine(st.toStringBoolean)

// Bytes.
[.min, .max].affirmEnchaine(st.toStringI8)
[.min, .max].affirmEnchaine(st.toStringU8)

// Shorts
[.min, .max].affirmEnchaine(st.toStringI16)
[.min, .max].affirmEnchaine(st.toStringU16)

// Ints
[0, 1, -1, .min, .max].affirmEnchaine(st.toStringI32)
[0, 1, .min, .max].affirmEnchaine(st.toStringU32)

// Longs
[.zero, 1, -1, .min, .max].affirmEnchaine(st.toStringI64)
[.zero, 1, .min, .max].affirmEnchaine(st.toStringU64)

// Floats
[.zero, 1, -1, .leastNonzeroMagnitude, .greatestFiniteMagnitude].affirmEnchaine(st.toStringFloat) { Float.init($0) == $1 }

// Doubles
[.zero, 1, -1, .leastNonzeroMagnitude, .greatestFiniteMagnitude].affirmEnchaine(st.toStringDouble) { Double.init($0) == $1 }

// Some extension functions for testing the results of roundtripping and stringifying
extension Array where Element: Equatable {
    static func defaultEquals(_ observed: String, expected: Element) -> Bool {
        let exp = "\(expected)"
        return observed == exp
    }

    func affirmEnchaine(_ fn: (Element) -> String, equals: (String, Element) -> Bool = defaultEquals) {
        self.forEach { v in
            let obs = fn(v)
            assert(equals(obs, v), "toString_\(type(of:v))(\(v)): observed=\(obs), expected=\(v)")
        }
    }

    func affirmAllerRetour(_ fn: (Element) -> Element) {
        self.forEach { v in
            assert(fn(v) == v, "identique_\(type(of:v))(\(v))")
        }
    }
}

// Prove to ourselves that default arguments are being used.
// Step 1: call the methods without arguments, and check against the UDL.
let op = Optionneur()

assert(op.sinonString() == "default")

assert(op.sinonBoolean() == false)

assert(op.sinonSequence() == [])

// optionals
assert(op.sinonNull() == nil)
assert(op.sinonZero() == 0)

// decimal integers
assert(op.sinonU8Dec() == UInt8(42))
assert(op.sinonI8Dec() == Int8(-42))
assert(op.sinonU16Dec() == UInt16(42))
assert(op.sinonI16Dec() == Int16(42))
assert(op.sinonU32Dec() == UInt32(42))
assert(op.sinonI32Dec() == Int32(42))
assert(op.sinonU64Dec() == UInt64(42))
assert(op.sinonI64Dec() == Int64(42))

// hexadecimal integers
assert(op.sinonU8Hex() == UInt8(0xff))
assert(op.sinonI8Hex() == Int8(-0x7f))
assert(op.sinonU16Hex() == UInt16(0xffff))
assert(op.sinonI16Hex() == Int16(0x7f))
assert(op.sinonU32Hex() == UInt32(0xffffffff))
assert(op.sinonI32Hex() == Int32(0x7fffffff))
assert(op.sinonU64Hex() == UInt64(0xffffffffffffffff))
assert(op.sinonI64Hex() == Int64(0x7fffffffffffffff))

// octal integers
assert(op.sinonU32Oct() == UInt32(0o755))

// floats
assert(op.sinonF32() == 42.0)
assert(op.sinonF64() == Double(42.1))

// enums
assert(op.sinonEnum() == .trois)

// Step 2. Convince ourselves that if we pass something else, then that changes the output.
//         We have shown something coming out of the sinon methods, but without eyeballing the Rust
//         we can't be sure that the arguments will change the return value.
["foo", "bar"].affirmAllerRetour(op.sinonString)
[true, false].affirmAllerRetour(op.sinonBoolean)
[["a", "b"], []].affirmAllerRetour(op.sinonSequence)

// optionals
["0", "1"].affirmAllerRetour(op.sinonNull)
[0, 1].affirmAllerRetour(op.sinonZero)

// integers
[0, 1].affirmAllerRetour(op.sinonU8Dec)
[0, 1].affirmAllerRetour(op.sinonI8Dec)
[0, 1].affirmAllerRetour(op.sinonU16Dec)
[0, 1].affirmAllerRetour(op.sinonI16Dec)
[0, 1].affirmAllerRetour(op.sinonU32Dec)
[0, 1].affirmAllerRetour(op.sinonI32Dec)
[0, 1].affirmAllerRetour(op.sinonU64Dec)
[0, 1].affirmAllerRetour(op.sinonI64Dec)

[0, 1].affirmAllerRetour(op.sinonU8Hex)
[0, 1].affirmAllerRetour(op.sinonI8Hex)
[0, 1].affirmAllerRetour(op.sinonU16Hex)
[0, 1].affirmAllerRetour(op.sinonI16Hex)
[0, 1].affirmAllerRetour(op.sinonU32Hex)
[0, 1].affirmAllerRetour(op.sinonI32Hex)
[0, 1].affirmAllerRetour(op.sinonU64Hex)
[0, 1].affirmAllerRetour(op.sinonI64Hex)

[0, 1].affirmAllerRetour(op.sinonU32Oct)

// floats
[0.0, 1.0].affirmAllerRetour(op.sinonF32)
[0.0, 1.0].affirmAllerRetour(op.sinonF64)

// enums
[.un, .deux, .trois].affirmAllerRetour(op.sinonEnum)

// Testing defaulting properties in record types.
let defaultes = OptionneurDictionnaire()
let explicites = OptionneurDictionnaire(
    i8Var: Int8(-8),
    u8Var: UInt8(8),
    i16Var: Int16(-16),
    u16Var: UInt16(0x10),
    i32Var: -32,
    u32Var: UInt32(32),
    i64Var: Int64(-64),
    u64Var: UInt64(64),
    floatVar: Float(4.0),
    doubleVar: Double(8.0),
    booleanVar: true,
    stringVar: "default",
    listVar: [],
    enumerationVar: .deux,
    dictionnaireVar: nil
)  

// …and makes sure they travel across and back the FFI.
assert(defaultes == explicites)
[defaultes].affirmAllerRetour(rt.identiqueOptionneurDictionnaire)
