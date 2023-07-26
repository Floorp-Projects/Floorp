class RustBuffer < FFI::Struct
  layout :capacity, :int32,
         :len,      :int32,
         :data,     :pointer

  def self.alloc(size)
    return {{ ci.namespace()|class_name_rb }}.rust_call(:{{ ci.ffi_rustbuffer_alloc().name() }}, size)
  end

  def self.reserve(rbuf, additional)
    return {{ ci.namespace()|class_name_rb }}.rust_call(:{{ ci.ffi_rustbuffer_reserve().name() }}, rbuf, additional)
  end

  def free
    {{ ci.namespace()|class_name_rb }}.rust_call(:{{ ci.ffi_rustbuffer_free().name() }}, self)
  end

  def capacity
    self[:capacity]
  end

  def len
    self[:len]
  end

  def len=(value)
    self[:len] = value
  end

  def data
    self[:data]
  end

  def to_s
    "RustBuffer(capacity=#{capacity}, len=#{len}, data=#{data.read_bytes len})"
  end

  # The allocated buffer will be automatically freed if an error occurs, ensuring that
  # we don't accidentally leak it.
  def self.allocWithBuilder
    builder = RustBufferBuilder.new

    begin
      yield builder
    rescue => e
      builder.discard
      raise e
    end
  end

  # The RustBuffer will be freed once the context-manager exits, ensuring that we don't
  # leak it even if an error occurs.
  def consumeWithStream
    stream = RustBufferStream.new self

    yield stream

    raise RuntimeError, 'junk data left in buffer after consuming' if stream.remaining != 0
  ensure
    free
  end

  {%- for typ in ci.iter_types() -%}
  {%- let canonical_type_name = typ.canonical_name() -%}
  {%- match typ -%}

  {% when Type::String -%}
  # The primitive String type.

  def self.allocFromString(value)
    RustBuffer.allocWithBuilder do |builder|
      builder.write value.encode('utf-8')
      return builder.finalize
    end
  end

  def consumeIntoString
    consumeWithStream do |stream|
      return stream.read(stream.remaining).force_encoding(Encoding::UTF_8)
    end
  end

  {% when Type::Bytes -%}
  # The primitive Bytes type.

  def self.allocFromBytes(value)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_Bytes(value)
      return builder.finalize
    end
  end

  def consumeIntoBytes
    consumeWithStream do |stream|
      return stream.readBytes
    end
  end

  {% when Type::Timestamp -%}
  def self.alloc_from_{{ canonical_type_name }}(v)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_{{ canonical_type_name }}(v)
      return builder.finalize
    end
  end

  def consumeInto{{ canonical_type_name }}
    consumeWithStream do |stream|
      return stream.read{{ canonical_type_name }}
    end
  end

  {% when Type::Duration -%}
  def self.alloc_from_{{ canonical_type_name }}(v)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_{{ canonical_type_name }}(v)
      return builder.finalize
    end
  end

  def consumeInto{{ canonical_type_name }}
    consumeWithStream do |stream|
      return stream.read{{ canonical_type_name }}
    end
  end

  {% when Type::Record with (record_name) -%}
  {%- let rec = ci|get_record_definition(record_name) -%}
  # The Record type {{ record_name }}.

  def self.alloc_from_{{ canonical_type_name }}(v)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_{{ canonical_type_name }}(v)
      return builder.finalize
    end
  end

  def consumeInto{{ canonical_type_name }}
    consumeWithStream do |stream|
      return stream.read{{ canonical_type_name }}
    end
  end

  {% when Type::Enum with (enum_name) -%}
  {% if !ci.is_name_used_as_error(enum_name) %}
  {%- let e = ci|get_enum_definition(enum_name) -%}
  # The Enum type {{ enum_name }}.

  def self.alloc_from_{{ canonical_type_name }}(v)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_{{ canonical_type_name }}(v)
      return builder.finalize
    end
  end

  def consumeInto{{ canonical_type_name }}
    consumeWithStream do |stream|
      return stream.read{{ canonical_type_name }}
    end
  end
  {% endif %}

  {% when Type::Optional with (inner_type) -%}
  # The Optional<T> type for {{ inner_type.canonical_name() }}.

  def self.alloc_from_{{ canonical_type_name }}(v)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_{{ canonical_type_name }}(v)
      return builder.finalize()
    end
  end

  def consumeInto{{ canonical_type_name }}
    consumeWithStream do |stream|
      return stream.read{{ canonical_type_name }}
    end
  end

  {% when Type::Sequence with (inner_type) -%}
  # The Sequence<T> type for {{ inner_type.canonical_name() }}.

  def self.alloc_from_{{ canonical_type_name }}(v)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_{{ canonical_type_name }}(v)
      return builder.finalize()
    end
  end

  def consumeInto{{ canonical_type_name }}
    consumeWithStream do |stream|
      return stream.read{{ canonical_type_name }}
    end
  end

  {% when Type::Map with (k, inner_type) -%}
  # The Map<T> type for {{ inner_type.canonical_name() }}.

  def self.alloc_from_{{ canonical_type_name }}(v)
    RustBuffer.allocWithBuilder do |builder|
      builder.write_{{ canonical_type_name }}(v)
      return builder.finalize
    end
  end

  def consumeInto{{ canonical_type_name }}
    consumeWithStream do |stream|
      return stream.read{{ canonical_type_name }}
    end
  end

  {%- else -%}
  {#- No code emitted for types that don't lower into a RustBuffer -#}
  {%- endmatch -%}
  {%- endfor %}
end

module UniFFILib
  class ForeignBytes < FFI::Struct
    layout :len,      :int32,
           :data,     :pointer

    def len
      self[:len]
    end

    def data
      self[:data]
    end

    def to_s
      "ForeignBytes(len=#{len}, data=#{data.read_bytes(len)})"
    end
  end
end

private_constant :UniFFILib
