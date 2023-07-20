
# Helper for structured reading of values from a RustBuffer.
class RustBufferStream

  def initialize(rbuf)
    @rbuf = rbuf
    @offset = 0
  end

  def remaining
    @rbuf.len - @offset
  end

  def read(size)
    raise InternalError, 'read past end of rust buffer' if @offset + size > @rbuf.len

    data = @rbuf.data.get_bytes @offset, size

    @offset += size

    data
  end

  {% for typ in ci.iter_types() -%}
  {%- let canonical_type_name = typ.canonical_name().borrow()|class_name_rb -%}
  {%- match typ -%}

  {% when Type::Int8 -%}

  def readI8
    unpack_from 1, 'c'
  end

  {% when Type::UInt8 -%}

  def readU8
    unpack_from 1, 'c'
  end

  {% when Type::Int16 -%}

  def readI16
    unpack_from 2, 's>'
  end

  {% when Type::UInt16 -%}

  def readU16
    unpack_from 2, 'S>'
  end

  {% when Type::Int32 -%}

  def readI32
    unpack_from 4, 'l>'
  end

  {% when Type::UInt32 -%}

  def readU32
    unpack_from 4, 'L>'
  end

  {% when Type::Int64 -%}

  def readI64
    unpack_from 8, 'q>'
  end

  {% when Type::UInt64 -%}

  def readU64
    unpack_from 8, 'Q>'
  end

  {% when Type::Float32 -%}

  def readF32
    unpack_from 4, 'g'
  end

  {% when Type::Float64 -%}

  def readF64
    unpack_from 8, 'G'
  end

  {% when Type::Boolean -%}

  def readBool
    v = unpack_from 1, 'c'

    return false if v == 0
    return true if v == 1

    raise InternalError, 'Unexpected byte for Boolean type'
  end

  {% when Type::String -%}

  def readString
    size = unpack_from 4, 'l>'

    raise InternalError, 'Unexpected negative string length' if size.negative?

    read(size).force_encoding(Encoding::UTF_8)
  end

  {% when Type::Bytes -%}

  def readBytes
    size = unpack_from 4, 'l>'

    raise InternalError, 'Unexpected negative byte string length' if size.negative?

    read(size).force_encoding(Encoding::BINARY)
  end

  {% when Type::Timestamp -%}
  # The Timestamp type.
  ONE_SECOND_IN_NANOSECONDS = 10**9

  def read{{ canonical_type_name }}
    seconds = unpack_from 8, 'q>'
    nanoseconds = unpack_from 4, 'L>'

    # UniFFi conventions assume that nanoseconds part has to represent nanoseconds portion of
    # duration between epoch and the timestamp moment. Ruby `Time#tv_nsec` returns the number of
    # nanoseconds for the subsecond part, which is sort of opposite to "duration" meaning.
    # Hence we need to convert value returned by `Time#tv_nsec` back and forth with the following
    # logic:
    if seconds < 0 && nanoseconds != 0
      # In order to get duration nsec we shift by 1 second:
      nanoseconds = ONE_SECOND_IN_NANOSECONDS - nanoseconds

      # Then we compensate 1 second shift:
      seconds -= 1
    end

    Time.at(seconds, nanoseconds, :nanosecond, in: '+00:00').utc
  end

  {% when Type::Duration -%}
  # The Duration type.

  def read{{ canonical_type_name }}
    seconds = unpack_from 8, 'q>'
    nanoseconds = unpack_from 4, 'L>'

    Time.at(seconds, nanoseconds, :nanosecond, in: '+00:00').utc
  end

  {% when Type::Object with { name: object_name, imp } -%}
  # The Object type {{ object_name }}.

  def read{{ canonical_type_name }}
    pointer = FFI::Pointer.new unpack_from 8, 'Q>'
    return {{ object_name|class_name_rb }}._uniffi_allocate(pointer)
  end

  {% when Type::Enum with (name) -%}
  {%- let e = ci|get_enum_definition(name) -%}
  {% if !ci.is_name_used_as_error(name) %}
  {% let enum_name = name %}
  # The Enum type {{ enum_name }}.

  def read{{ canonical_type_name }}
    variant = unpack_from 4, 'l>'
    {% if e.is_flat() -%}
    {%- for variant in e.variants() %}
    if variant == {{ loop.index }}
      return {{ enum_name|class_name_rb }}::{{ variant.name()|enum_name_rb }}
    end
    {%- endfor %}

    raise InternalError, 'Unexpected variant tag for {{ canonical_type_name }}'
    {%- else -%}
    {%- for variant in e.variants() %}
    if variant == {{ loop.index }}
        {%- if variant.has_fields() %}
        return {{ enum_name|class_name_rb }}::{{ variant.name()|enum_name_rb }}.new(
            {%- for field in variant.fields() %}
            self.read{{ field.as_type().canonical_name().borrow()|class_name_rb }}(){% if loop.last %}{% else %},{% endif %}
            {%- endfor %}
        )
        {%- else %}
        return {{ enum_name|class_name_rb }}::{{ variant.name()|enum_name_rb }}.new
        {% endif %}
    end
    {%- endfor %}
    raise InternalError, 'Unexpected variant tag for {{ canonical_type_name }}'
    {%- endif %}
  end

  {% else %}

  {% let error_name = name %}

  # The Error type {{ error_name }}

  def read{{ canonical_type_name }}
    variant = unpack_from 4, 'l>'
    {% if e.is_flat() -%}
    {%- for variant in e.variants() %}
    if variant == {{ loop.index }}
      return {{ error_name|class_name_rb }}::{{ variant.name()|class_name_rb }}.new(
        readString()
      )
    end
    {%- endfor %}

    raise InternalError, 'Unexpected variant tag for {{ canonical_type_name }}'
    {%- else -%}
    {%- for variant in e.variants() %}
    if variant == {{ loop.index }}
        {%- if variant.has_fields() %}
        return {{ error_name|class_name_rb }}::{{ variant.name()|class_name_rb }}.new(
            {%- for field in variant.fields() %}
            read{{ field.as_type().canonical_name().borrow()|class_name_rb }}(){% if loop.last %}{% else %},{% endif %}
            {%- endfor %}
        )
        {%- else %}
        return {{ error_name|class_name_rb }}::{{ variant.name()|class_name_rb }}.new
        {%- endif %}
    end
    {%- endfor %}

    raise InternalError, 'Unexpected variant tag for {{ canonical_type_name }}'
    {%- endif %}
  end
  {% endif %}

  {% when Type::Record with (record_name) -%}
  {%- let rec = ci|get_record_definition(record_name) -%}
  # The Record type {{ record_name }}.

  def read{{ canonical_type_name }}
    {{ rec.name()|class_name_rb }}.new(
      {%- for field in rec.fields() %}
      read{{ field.as_type().canonical_name().borrow()|class_name_rb }}{% if loop.last %}{% else %},{% endif %}
      {%- endfor %}
    )
  end

  {% when Type::Optional with (inner_type) -%}
  # The Optional<T> type for {{ inner_type.canonical_name() }}.

  def read{{ canonical_type_name }}
    flag = unpack_from 1, 'c'

    if flag == 0
      return nil
    elsif flag == 1
      return read{{ inner_type.canonical_name().borrow()|class_name_rb }}
    else
      raise InternalError, 'Unexpected flag byte for {{ canonical_type_name }}'
    end
  end

  {% when Type::Sequence with (inner_type) -%}
  # The Sequence<T> type for {{ inner_type.canonical_name() }}.

  def read{{ canonical_type_name }}
    count = unpack_from 4, 'l>'

    raise InternalError, 'Unexpected negative sequence length' if count.negative?

    items = []

    count.times do
      items.append read{{ inner_type.canonical_name().borrow()|class_name_rb }}
    end

    items
  end

  {% when Type::Map with (k, inner_type) -%}
  # The Map<T> type for {{ inner_type.canonical_name() }}.

  def read{{ canonical_type_name }}
    count = unpack_from 4, 'l>'
    raise InternalError, 'Unexpected negative map size' if count.negative?

    items = {}
    count.times do
      key = readString
      items[key] = read{{ inner_type.canonical_name().borrow()|class_name_rb }}
    end

    items
  end
  {%- else -%}
  # This type is not yet supported in the Ruby backend.
  def read{{ canonical_type_name }}
    raise InternalError, 'RustBufferStream.read not implemented yet for {{ canonical_type_name }}'
  end

  {%- endmatch -%}
  {%- endfor %}

  def unpack_from(size, format)
    raise InternalError, 'read past end of rust buffer' if @offset + size > @rbuf.len

    value = @rbuf.data.get_bytes(@offset, size).unpack format

    @offset += size

    # TODO: verify this
    raise 'more than one element!!!' if value.size > 1

    value[0]
  end
end

private_constant :RustBufferStream
