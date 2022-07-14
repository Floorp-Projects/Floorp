class RustCallStatus < FFI::Struct
  layout :code,    :int8,
         :error_buf, RustBuffer

  def code
    self[:code]
  end

  def error_buf
    self[:error_buf]
  end

  def to_s
    "RustCallStatus(code=#{self[:code]})"
  end
end

# These match the values from the uniffi::rustcalls module
CALL_SUCCESS = 0
CALL_ERROR = 1
CALL_PANIC = 2
{%- for e in ci.error_definitions() %}
{% if e.is_flat() %}
class {{ e.name()|class_name_rb }}
    {%- for variant in e.variants() %}
    {{ variant.name()|class_name_rb }} = Class.new StandardError
    {%- endfor %}
{% else %}
module {{ e.name()|class_name_rb }}
  {%- for variant in e.variants() %}
  class {{ variant.name()|class_name_rb }} < StandardError
    def initialize({% for field in variant.fields() %}{{ field.name()|var_name_rb }}{% if !loop.last %}, {% endif %}{% endfor %})
        {%- for field in variant.fields() %}
        @{{ field.name()|var_name_rb }} = {{ field.name()|var_name_rb }}
        {%- endfor %}
        super()
      end
    {%- if variant.has_fields() %}

    attr_reader {% for field in variant.fields() %}:{{ field.name() }}{% if !loop.last %}, {% endif %}{% endfor %}
    {% endif %}
  end
  {%- endfor %}
{% endif %}
end
{%- endfor %}

# Map error modules to the RustBuffer method name that reads them
ERROR_MODULE_TO_READER_METHOD = {
{%- for e in ci.error_definitions() %}
{%- let typ=ci.get_type(e.name()).unwrap() %}
{%- let canonical_type_name = typ.canonical_name().borrow()|class_name_rb %}
  {{ e.name()|class_name_rb }} => :read{{ canonical_type_name }},
{%- endfor %}
}

private_constant :ERROR_MODULE_TO_READER_METHOD, :CALL_SUCCESS, :CALL_ERROR, :CALL_PANIC,
                 :RustCallStatus

def self.consume_buffer_into_error(error_module, rust_buffer)
  rust_buffer.consumeWithStream do |stream|
    reader_method = ERROR_MODULE_TO_READER_METHOD[error_module]
    return stream.send(reader_method)
  end
end

class InternalError < StandardError
end

def self.rust_call(fn_name, *args)
  # Call a rust function
  rust_call_with_error(nil, fn_name, *args)
end

def self.rust_call_with_error(error_module, fn_name, *args)
  # Call a rust function and handle errors
  #
  # Use this when the rust function returns a Result<>.  error_module must be the error_module that corresponds to that Result.


  # Note: RustCallStatus.new zeroes out the struct, which is exactly what we
  # want to pass to Rust (code=0, error_buf=RustBuffer(len=0, capacity=0,
  # data=NULL))
  status = RustCallStatus.new
  args << status

  result = UniFFILib.public_send(fn_name, *args)

  case status.code
  when CALL_SUCCESS
    result
  when CALL_ERROR
    if error_module.nil?
      status.error_buf.free
      raise InternalError, "CALL_ERROR with no error_module set"
    else
      raise consume_buffer_into_error(error_module, status.error_buf)
    end
  when CALL_PANIC
    # When the rust code sees a panic, it tries to construct a RustBuffer
    # with the message.  But if that code panics, then it just sends back
    # an empty buffer.
    if status.error_buf.len > 0
      raise InternalError, status.error_buf.consumeIntoString()
    else
      raise InternalError, "Rust panic"
    end
  else
    raise InternalError, "Unknown call status: #{status.code}"
  end
end

private_class_method :consume_buffer_into_error
