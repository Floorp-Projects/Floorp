def self.uniffi_in_range(i, type_name, min, max)
  raise TypeError, "no implicit conversion of #{i} into Integer" unless i.respond_to?(:to_int)
  i = i.to_int
  raise RangeError, "#{type_name} requires #{min} <= value < #{max}" unless (min <= i && i < max)
  i
end

def self.uniffi_utf8(v)
  raise TypeError, "no implicit conversion of #{v} into String" unless v.respond_to?(:to_str)
  v = v.to_str.encode(Encoding::UTF_8)
  raise Encoding::InvalidByteSequenceError, "not a valid UTF-8 encoded string" unless v.valid_encoding?
  v
end

def self.uniffi_bytes(v)
  raise TypeError, "no implicit conversion of #{v} into String" unless v.respond_to?(:to_str)
  v.to_str
end
