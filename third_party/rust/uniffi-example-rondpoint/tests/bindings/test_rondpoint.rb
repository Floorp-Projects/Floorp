# frozen_string_literal: true

require 'test/unit'
require 'rondpoint'

include Test::Unit::Assertions
include Rondpoint

dico = Dictionnaire.new Enumeration::DEUX, true, 0, 123_456_789

assert_equal dico, Rondpoint.copie_dictionnaire(dico)

assert_equal Rondpoint.copie_enumeration(Enumeration::DEUX), Enumeration::DEUX

assert_equal Rondpoint.copie_enumerations([
                                            Enumeration::UN,
                                            Enumeration::DEUX
                                          ]), [Enumeration::UN, Enumeration::DEUX]

assert_equal Rondpoint.copie_carte({
                                     '0' => EnumerationAvecDonnees::ZERO.new,
                                     '1' => EnumerationAvecDonnees::UN.new(1),
                                     '2' => EnumerationAvecDonnees::DEUX.new(2, 'deux')
                                   }), {
                                     '0' => EnumerationAvecDonnees::ZERO.new,
                                     '1' => EnumerationAvecDonnees::UN.new(1),
                                     '2' => EnumerationAvecDonnees::DEUX.new(2, 'deux')
                                   }

assert Rondpoint.switcheroo(false)

assert_not_equal EnumerationAvecDonnees::ZERO.new, EnumerationAvecDonnees::UN.new(1)
assert_equal EnumerationAvecDonnees::UN.new(1), EnumerationAvecDonnees::UN.new(1)
assert_not_equal EnumerationAvecDonnees::UN.new(1), EnumerationAvecDonnees::UN.new(2)

# Test the roundtrip across the FFI.
# This shows that the values we send come back in exactly the same state as we sent them.
# i.e. it shows that lowering from ruby and lifting into rust is symmetrical with
#      lowering from rust and lifting into ruby.
RT = Retourneur.new

def affirm_aller_retour(vals, fn_name)
  vals.each do |v|
    id_v = RT.public_send fn_name, v

    assert_equal id_v, v, "Round-trip failure: #{v} => #{id_v}"
  end
end

MIN_I8 = -1 * 2**7
MAX_I8 = 2**7 - 1
MIN_I16 = -1 * 2**15
MAX_I16 = 2**15 - 1
MIN_I32 = -1 * 2**31
MAX_I32 = 2**31 - 1
MIN_I64 = -1 * 2**31
MAX_I64 = 2**31 - 1

# Ruby floats are always doubles, so won't round-trip through f32 correctly.
# This truncates them appropriately.
F32_ONE_THIRD = [1.0 / 3].pack('f').unpack('f')[0]

# Booleans
affirm_aller_retour([true, false], :identique_boolean)

# Bytes.
affirm_aller_retour([MIN_I8, -1, 0, 1, MAX_I8], :identique_i8)
affirm_aller_retour([0x00, 0x12, 0xFF], :identique_u8)

# Shorts
affirm_aller_retour([MIN_I16, -1, 0, 1, MAX_I16], :identique_i16)
affirm_aller_retour([0x0000, 0x1234, 0xFFFF], :identique_u16)

# Ints
affirm_aller_retour([MIN_I32, -1, 0, 1, MAX_I32], :identique_i32)
affirm_aller_retour([0x00000000, 0x12345678, 0xFFFFFFFF], :identique_u32)

# Longs
affirm_aller_retour([MIN_I64, -1, 0, 1, MAX_I64], :identique_i64)
affirm_aller_retour([0x0000000000000000, 0x1234567890ABCDEF, 0xFFFFFFFFFFFFFFFF], :identique_u64)

# Floats
affirm_aller_retour([0.0, 0.5, 0.25, 1.0, F32_ONE_THIRD], :identique_float)

# Doubles
affirm_aller_retour(
  [0.0, 0.5, 0.25, 1.0, 1.0 / 3, Float::MAX, Float::MIN],
  :identique_double
)

# Strings
affirm_aller_retour(
  ['', 'abc', 'été', 'ښي لاس ته لوستلو لوستل',
   '😻emoji 👨‍👧‍👦multi-emoji, 🇨🇭a flag, a canal, panama'],
  :identique_string
)

# Test one way across the FFI.
#
# We send one representation of a value to lib.rs, and it transforms it into another, a string.
# lib.rs sends the string back, and then we compare here in ruby.
#
# This shows that the values are transformed into strings the same way in both ruby and rust.
# i.e. if we assume that the string return works (we test this assumption elsewhere)
#      we show that lowering from ruby and lifting into rust has values that both ruby and rust
#      both stringify in the same way. i.e. the same values.
#
# If we roundtripping proves the symmetry of our lowering/lifting from here to rust, and lowering/lifting from rust to here,
# and this convinces us that lowering/lifting from here to rust is correct, then
# together, we've shown the correctness of the return leg.
ST = Stringifier.new

def affirm_enchaine(vals, fn_name)
  vals.each do |v|
    str_v = ST.public_send fn_name, v

    assert_equal v.to_s, str_v, "String compare error #{v} => #{str_v}"
  end
end

# Test the efficacy of the string transport from rust. If this fails, but everything else
# works, then things are very weird.
assert_equal ST.well_known_string('ruby'), 'uniffi 💚 ruby!'

# Booleans
affirm_enchaine([true, false], :to_string_boolean)

# Bytes.
affirm_enchaine([MIN_I8, -1, 0, 1, MAX_I8], :to_string_i8)
affirm_enchaine([0x00, 0x12, 0xFF], :to_string_u8)

# Shorts
affirm_enchaine([MIN_I16, -1, 0, 1, MAX_I16], :to_string_i16)
affirm_enchaine([0x0000, 0x1234, 0xFFFF], :to_string_u16)

# Ints
affirm_enchaine([MIN_I32, -1, 0, 1, MAX_I32], :to_string_i32)
affirm_enchaine([0x00000000, 0x12345678, 0xFFFFFFFF], :to_string_u32)

# Longs
affirm_enchaine([MIN_I64, -1, 0, 1, MAX_I64], :to_string_i64)
affirm_enchaine([0x0000000000000000, 0x1234567890ABCDEF, 0xFFFFFFFFFFFFFFFF], :to_string_u64)
