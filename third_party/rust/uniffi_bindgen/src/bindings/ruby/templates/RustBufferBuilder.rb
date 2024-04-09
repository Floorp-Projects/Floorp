
# Helper for structured writing of values into a RustBuffer.
class RustBufferBuilder
  def initialize
    @rust_buf = RustBuffer.alloc 16
    @rust_buf.len = 0
  end

  def finalize
    rbuf = @rust_buf

    @rust_buf = nil

    rbuf
  end

  def discard
    return if @rust_buf.nil?

    rbuf = finalize
    rbuf.free
  end

  def write(value)
    reserve(value.bytes.size) do
      @rust_buf.data.put_array_of_char @rust_buf.len, value.bytes
    end
  end

  {% for typ in ci.iter_types() -%}
  {%- let canonical_type_name = canonical_name(typ).borrow()|class_name_rb -%}
  {%- match typ -%}

  {% when Type::Int8 -%}

  def write_I8(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "i8", -2**7, 2**7)
    pack_into(1, 'c', v)
  end

  {% when Type::UInt8 -%}

  def write_U8(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "u8", 0, 2**8)
    pack_into(1, 'c', v)
  end

  {% when Type::Int16 -%}

  def write_I16(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "i16", -2**15, 2**15)
    pack_into(2, 's>', v)
  end

  {% when Type::UInt16 -%}

  def write_U16(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "u16", 0, 2**16)
    pack_into(2, 'S>', v)
  end

  {% when Type::Int32 -%}

  def write_I32(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "i32", -2**31, 2**31)
    pack_into(4, 'l>', v)
  end

  {% when Type::UInt32 -%}

  def write_U32(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "u32", 0, 2**32)
    pack_into(4, 'L>', v)
  end

  {% when Type::Int64 -%}

  def write_I64(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "i64", -2**63, 2**63)
    pack_into(8, 'q>', v)
  end

  {% when Type::UInt64 -%}

  def write_U64(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_in_range(v, "u64", 0, 2**64)
    pack_into(8, 'Q>', v)
  end

  {% when Type::Float32 -%}

  def write_F32(v)
    pack_into(4, 'g', v)
  end

  {% when Type::Float64 -%}

  def write_F64(v)
    pack_into(8, 'G', v)
  end

  {% when Type::Boolean -%}

  def write_Bool(v)
    pack_into(1, 'c', v ? 1 : 0)
  end

  {% when Type::String -%}

  def write_String(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_utf8(v)
    pack_into 4, 'l>', v.bytes.size
    write v
  end

  {% when Type::Bytes -%}

  def write_Bytes(v)
    v = {{ ci.namespace()|class_name_rb }}::uniffi_bytes(v)
    pack_into 4, 'l>', v.bytes.size
    write v
  end

  {% when Type::Timestamp -%}
  # The Timestamp type.
  ONE_SECOND_IN_NANOSECONDS = 10**9

  def write_{{ canonical_type_name }}(v)
    seconds = v.tv_sec
    nanoseconds = v.tv_nsec

    # UniFFi conventions assume that nanoseconds part has to represent nanoseconds portion of
    # duration between epoch and the timestamp moment. Ruby `Time#tv_nsec` returns the number of
    # nanoseconds for the subsecond part, which is sort of opposite to "duration" meaning.
    # Hence we need to convert value returned by `Time#tv_nsec` back and forth with the following
    # logic:
    if seconds < 0 && nanoseconds != 0
      # In order to get duration nsec we shift by 1 second:
      nanoseconds = ONE_SECOND_IN_NANOSECONDS - nanoseconds

      # Then we compensate 1 second shift:
      seconds += 1
    end

    pack_into 8, 'q>', seconds
    pack_into 4, 'L>', nanoseconds
  end

  {% when Type::Duration -%}
  # The Duration type.

  def write_{{ canonical_type_name }}(v)
    seconds = v.tv_sec
    nanoseconds = v.tv_nsec

    raise ArgumentError, 'Invalid duration, must be non-negative' if seconds < 0

    pack_into 8, 'Q>', seconds
    pack_into 4, 'L>', nanoseconds
  end

  {% when Type::Object with { name: object_name, module_path, imp } -%}
  # The Object type {{ object_name }}.

  def write_{{ canonical_type_name }}(obj)
    pointer = {{ object_name|class_name_rb}}.uniffi_lower obj
    pack_into(8, 'Q>', pointer.address)
  end

  {% when Type::Enum { name: enum_name, module_path } -%}
  {% if !ci.is_name_used_as_error(enum_name) %}
  {%- let e = ci|get_enum_definition(enum_name) -%}
  # The Enum type {{ enum_name }}.

  def write_{{ canonical_type_name }}(v)
    {%- if e.is_flat() %}
    pack_into(4, 'l>', v)
    {%- else -%}
    {%- for variant in e.variants() %}
    if v.{{ variant.name()|var_name_rb }}?
      pack_into(4, 'l>', {{ loop.index }})
      {%- for field in variant.fields() %}
      self.write_{{ canonical_name(field.as_type().borrow()).borrow()|class_name_rb }}(v.{{ field.name() }})
      {%- endfor %}
    end
    {%- endfor %}
    {%- endif %}
 end
   {% endif %}

  {% when Type::Record { name: record_name, module_path } -%}
  {%- let rec = ci|get_record_definition(record_name) -%}
  # The Record type {{ record_name }}.

  def write_{{ canonical_type_name }}(v)
    {%- for field in rec.fields() %}
    self.write_{{ canonical_name(field.as_type().borrow()).borrow()|class_name_rb }}(v.{{ field.name()|var_name_rb }})
    {%- endfor %}
  end

  {% when Type::Optional { inner_type } -%}
  # The Optional<T> type for {{ canonical_name(inner_type) }}.

  def write_{{ canonical_type_name }}(v)
    if v.nil?
      pack_into(1, 'c', 0)
    else
      pack_into(1, 'c', 1)
      self.write_{{ canonical_name(inner_type).borrow()|class_name_rb }}(v)
    end
  end

  {% when Type::Sequence { inner_type } -%}
  # The Sequence<T> type for {{ canonical_name(inner_type) }}.

  def write_{{ canonical_type_name }}(items)
    pack_into(4, 'l>', items.size)

    items.each do |item|
      self.write_{{ canonical_name(inner_type).borrow()|class_name_rb }}(item)
    end
  end

  {% when Type::Map { key_type: k, value_type: inner_type } -%}
  # The Map<T> type for {{ canonical_name(inner_type) }}.

  def write_{{ canonical_type_name }}(items)
    pack_into(4, 'l>', items.size)

    items.each do |k, v|
      write_String(k)
      self.write_{{ canonical_name(inner_type).borrow()|class_name_rb }}(v)
    end
  end

  {%- else -%}
  # This type is not yet supported in the Ruby backend.
  def write_{{ canonical_type_name }}(v)
    raise InternalError('RustBufferStream.write() not implemented yet for {{ canonical_type_name }}')
  end

  {%- endmatch -%}
  {%- endfor %}

  private

  def reserve(num_bytes)
    if @rust_buf.len + num_bytes > @rust_buf.capacity
      @rust_buf = RustBuffer.reserve(@rust_buf, num_bytes)
    end

    yield

    @rust_buf.len += num_bytes
  end

  def pack_into(size, format, value)
    reserve(size) do
      @rust_buf.data.put_array_of_char @rust_buf.len, [value].pack(format).bytes
    end
  end
end

private_constant :RustBufferBuilder
