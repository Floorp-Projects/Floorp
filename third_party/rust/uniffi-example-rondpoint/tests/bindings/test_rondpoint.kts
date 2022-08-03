import uniffi.rondpoint.*

val dico = Dictionnaire(Enumeration.DEUX, true, 0u, 123456789u)
val copyDico = copieDictionnaire(dico)
assert(dico == copyDico)

assert(copieEnumeration(Enumeration.DEUX) == Enumeration.DEUX)
assert(copieEnumerations(listOf(Enumeration.UN, Enumeration.DEUX)) == listOf(Enumeration.UN, Enumeration.DEUX))
assert(copieCarte(mapOf(
    "0" to EnumerationAvecDonnees.Zero,
    "1" to EnumerationAvecDonnees.Un(1u),
    "2" to EnumerationAvecDonnees.Deux(2u, "deux")
)) == mapOf(
    "0" to EnumerationAvecDonnees.Zero,
    "1" to EnumerationAvecDonnees.Un(1u),
    "2" to EnumerationAvecDonnees.Deux(2u, "deux")
))

val var1: EnumerationAvecDonnees = EnumerationAvecDonnees.Zero
val var2: EnumerationAvecDonnees = EnumerationAvecDonnees.Un(1u)
val var3: EnumerationAvecDonnees = EnumerationAvecDonnees.Un(2u)
assert(var1 != var2)
assert(var2 != var3)
assert(var1 == EnumerationAvecDonnees.Zero)
assert(var1 != EnumerationAvecDonnees.Un(1u))
assert(var2 == EnumerationAvecDonnees.Un(1u))

assert(switcheroo(false))

// Test the roundtrip across the FFI.
// This shows that the values we send come back in exactly the same state as we sent them.
// i.e. it shows that lowering from kotlin and lifting into rust is symmetrical with 
//      lowering from rust and lifting into kotlin.
val rt = Retourneur()

fun <T> List<T>.affirmAllerRetour(fn: (T) -> T) {
    this.forEach { v ->
        assert(fn.invoke(v) == v) { "$fn($v)" }
    }
}

// Booleans
listOf(true, false).affirmAllerRetour(rt::identiqueBoolean)

// Bytes.
listOf(Byte.MIN_VALUE, Byte.MAX_VALUE).affirmAllerRetour(rt::identiqueI8)
listOf(0x00, 0xFF).map { it.toUByte() }.affirmAllerRetour(rt::identiqueU8)

// Shorts
listOf(Short.MIN_VALUE, Short.MAX_VALUE).affirmAllerRetour(rt::identiqueI16)
listOf(0x0000, 0xFFFF).map { it.toUShort() }.affirmAllerRetour(rt::identiqueU16)

// Ints
listOf(0, 1, -1, Int.MIN_VALUE, Int.MAX_VALUE).affirmAllerRetour(rt::identiqueI32)
listOf(0x00000000, 0xFFFFFFFF).map { it.toUInt() }.affirmAllerRetour(rt::identiqueU32)

// Longs
listOf(0L, 1L, -1L, Long.MIN_VALUE, Long.MAX_VALUE).affirmAllerRetour(rt::identiqueI64)
listOf(0u, 1u, ULong.MIN_VALUE, ULong.MAX_VALUE).affirmAllerRetour(rt::identiqueU64)

// Floats
listOf(0.0F, 0.5F, 0.25F, Float.MIN_VALUE, Float.MAX_VALUE).affirmAllerRetour(rt::identiqueFloat)

// Doubles
listOf(0.0, 1.0, Double.MIN_VALUE, Double.MAX_VALUE).affirmAllerRetour(rt::identiqueDouble)

// Strings
listOf("", "abc", "null\u0000byte", "été", "ښي لاس ته لوستلو لوستل", "😻emoji 👨‍👧‍👦multi-emoji, 🇨🇭a flag, a canal, panama")
    .affirmAllerRetour(rt::identiqueString)

listOf(-1, 0, 1).map { DictionnaireNombresSignes(it.toByte(), it.toShort(), it.toInt(), it.toLong()) }
    .affirmAllerRetour(rt::identiqueNombresSignes)

listOf(0, 1).map { DictionnaireNombres(it.toUByte(), it.toUShort(), it.toUInt(), it.toULong()) }
    .affirmAllerRetour(rt::identiqueNombres)


rt.destroy()

// Test one way across the FFI.
//
// We send one representation of a value to lib.rs, and it transforms it into another, a string.
// lib.rs sends the string back, and then we compare here in kotlin.
//
// This shows that the values are transformed into strings the same way in both kotlin and rust.
// i.e. if we assume that the string return works (we test this assumption elsewhere)
//      we show that lowering from kotlin and lifting into rust has values that both kotlin and rust
//      both stringify in the same way. i.e. the same values.
//
// If we roundtripping proves the symmetry of our lowering/lifting from here to rust, and lowering/lifting from rust t here,
// and this convinces us that lowering/lifting from here to rust is correct, then 
// together, we've shown the correctness of the return leg.
val st = Stringifier()

typealias StringyEquals<T> = (observed: String, expected: T) -> Boolean
fun <T> List<T>.affirmEnchaine(
    fn: (T) -> String,
    equals: StringyEquals<T> = { obs, exp -> obs == exp.toString() }
) {
    this.forEach { exp ->
        val obs = fn.invoke(exp)
        assert(equals(obs, exp)) { "$fn($exp): observed=$obs, expected=$exp" }
    }
}

// Test the efficacy of the string transport from rust. If this fails, but everything else 
// works, then things are very weird.
val wellKnown = st.wellKnownString("kotlin")
assert("uniffi 💚 kotlin!" == wellKnown) { "wellKnownString 'uniffi 💚 kotlin!' == '$wellKnown'" }

// Booleans
listOf(true, false).affirmEnchaine(st::toStringBoolean)

// Bytes.
listOf(Byte.MIN_VALUE, Byte.MAX_VALUE).affirmEnchaine(st::toStringI8)
listOf(UByte.MIN_VALUE, UByte.MAX_VALUE).affirmEnchaine(st::toStringU8)

// Shorts
listOf(Short.MIN_VALUE, Short.MAX_VALUE).affirmEnchaine(st::toStringI16)
listOf(UShort.MIN_VALUE, UShort.MAX_VALUE).affirmEnchaine(st::toStringU16)

// Ints
listOf(0, 1, -1, Int.MIN_VALUE, Int.MAX_VALUE).affirmEnchaine(st::toStringI32)
listOf(0u, 1u, UInt.MIN_VALUE, UInt.MAX_VALUE).affirmEnchaine(st::toStringU32)

// Longs
listOf(0L, 1L, -1L, Long.MIN_VALUE, Long.MAX_VALUE).affirmEnchaine(st::toStringI64)
listOf(0u, 1u, ULong.MIN_VALUE, ULong.MAX_VALUE).affirmEnchaine(st::toStringU64)

// Floats
// MIN_VAUE is 1.4E-45. Accuracy and formatting get weird at small sizes.
listOf(0.0F, 1.0F, -1.0F, Float.MIN_VALUE, Float.MAX_VALUE).affirmEnchaine(st::toStringFloat) { s, n -> s.toFloat() == n }

// Doubles
// MIN_VALUE is 4.9E-324. Accuracy and formatting get weird at small sizes.
listOf(0.0, 1.0, -1.0, Double.MIN_VALUE, Double.MAX_VALUE).affirmEnchaine(st::toStringDouble)  { s, n -> s.toDouble() == n }

st.destroy()

// Prove to ourselves that default arguments are being used.
// Step 1: call the methods without arguments, and check against the UDL.
val op = Optionneur()

assert(op.sinonString() == "default")

assert(op.sinonBoolean() == false)

assert(op.sinonSequence() == listOf<String>())

// optionals
assert(op.sinonNull() == null)
assert(op.sinonZero() == 0)

// decimal integers
assert(op.sinonI8Dec() == (-42).toByte())
assert(op.sinonU8Dec() == 42.toUByte())
assert(op.sinonI16Dec() == 42.toShort())
assert(op.sinonU16Dec() == 42.toUShort())
assert(op.sinonI32Dec() == 42)
assert(op.sinonU32Dec() == 42.toUInt())
assert(op.sinonI64Dec() == 42L)
assert(op.sinonU64Dec() == 42uL)

// hexadecimal integers
assert(op.sinonI8Hex() == (-0x7f).toByte())
assert(op.sinonU8Hex() == 0xff.toUByte())
assert(op.sinonI16Hex() == 0x7f.toShort())
assert(op.sinonU16Hex() == 0xffff.toUShort())
assert(op.sinonI32Hex() == 0x7fffffff)
assert(op.sinonU32Hex() == 0xffffffff.toUInt())
assert(op.sinonI64Hex() == 0x7fffffffffffffffL)
assert(op.sinonU64Hex() == 0xffffffffffffffffuL)

// octal integers
assert(op.sinonU32Oct() == 493u) // 0o755

// floats
assert(op.sinonF32() == 42.0f)
assert(op.sinonF64() == 42.1)

// enums
assert(op.sinonEnum() == Enumeration.TROIS)

// Step 2. Convince ourselves that if we pass something else, then that changes the output.
//         We have shown something coming out of the sinon methods, but without eyeballing the Rust
//         we can't be sure that the arguments will change the return value.
listOf("foo", "bar").affirmAllerRetour(op::sinonString)
listOf(true, false).affirmAllerRetour(op::sinonBoolean)
listOf(listOf("a", "b"), listOf()).affirmAllerRetour(op::sinonSequence)

// optionals
listOf("0", "1").affirmAllerRetour(op::sinonNull)
listOf(0, 1).affirmAllerRetour(op::sinonZero)

// integers
listOf(0, 1).map { it.toUByte() }.affirmAllerRetour(op::sinonU8Dec)
listOf(0, 1).map { it.toByte() }.affirmAllerRetour(op::sinonI8Dec)
listOf(0, 1).map { it.toUShort() }.affirmAllerRetour(op::sinonU16Dec)
listOf(0, 1).map { it.toShort() }.affirmAllerRetour(op::sinonI16Dec)
listOf(0, 1).map { it.toUInt() }.affirmAllerRetour(op::sinonU32Dec)
listOf(0, 1).map { it.toInt() }.affirmAllerRetour(op::sinonI32Dec)
listOf(0, 1).map { it.toULong() }.affirmAllerRetour(op::sinonU64Dec)
listOf(0, 1).map { it.toLong() }.affirmAllerRetour(op::sinonI64Dec)

listOf(0, 1).map { it.toUByte() }.affirmAllerRetour(op::sinonU8Hex)
listOf(0, 1).map { it.toByte() }.affirmAllerRetour(op::sinonI8Hex)
listOf(0, 1).map { it.toUShort() }.affirmAllerRetour(op::sinonU16Hex)
listOf(0, 1).map { it.toShort() }.affirmAllerRetour(op::sinonI16Hex)
listOf(0, 1).map { it.toUInt() }.affirmAllerRetour(op::sinonU32Hex)
listOf(0, 1).map { it.toInt() }.affirmAllerRetour(op::sinonI32Hex)
listOf(0, 1).map { it.toULong() }.affirmAllerRetour(op::sinonU64Hex)
listOf(0, 1).map { it.toLong() }.affirmAllerRetour(op::sinonI64Hex)

listOf(0, 1).map { it.toUInt() }.affirmAllerRetour(op::sinonU32Oct)

// floats
listOf(0.0f, 1.0f).affirmAllerRetour(op::sinonF32)
listOf(0.0, 1.0).affirmAllerRetour(op::sinonF64)

// enums
Enumeration.values().toList().affirmAllerRetour(op::sinonEnum)

op.destroy()

// Testing defaulting properties in record types.
val defaultes = OptionneurDictionnaire()
val explicite = OptionneurDictionnaire(
    i8Var = -8,
    u8Var = 8u,
    i16Var = -16,
    u16Var = 0x10u,
    i32Var = -32,
    u32Var = 32u,
    i64Var = -64L,
    u64Var = 64uL,
    floatVar = 4.0f,
    doubleVar = 8.0,
    booleanVar = true,
    stringVar = "default",
    listVar = listOf(),
    enumerationVar = Enumeration.DEUX,
    dictionnaireVar = null
)
assert(defaultes == explicite)

// …and makes sure they travel across and back the FFI.
val rt2 = Retourneur()
listOf(defaultes).affirmAllerRetour(rt2::identiqueOptionneurDictionnaire)

rt2.destroy()
