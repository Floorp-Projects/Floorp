{%- macro call_scaffolding_function(func) %}
{%- call _call_scaffolding_function(func, func.return_type(), "", func.is_js_async(config)) -%}
{%- endmacro %}

{%- macro call_constructor(cons, object_type, is_async) %}
{%- call _call_scaffolding_function(cons, Some(object_type), "", is_async) -%}
{%- endmacro %}

{%- macro call_method(method, object_type, is_async) %}
{%- call _call_scaffolding_function(method, method.return_type(), object_type.ffi_converter(), is_async) -%}
{%- endmacro %}

{%- macro _call_scaffolding_function(func, return_type, receiver_ffi_converter, is_async) %}
        {%- match return_type %}
        {%- when Some with (return_type) %}
        const liftResult = (result) => {{ return_type.ffi_converter() }}.lift(result);
        {%- else %}
        const liftResult = (result) => undefined;
        {%- endmatch %}
        {%- match func.throws_type() %}
        {%- when Some with (err_type) %}
        const liftError = (data) => {{ err_type.ffi_converter() }}.lift(data);
        {%- else %}
        const liftError = null;
        {%- endmatch %}
        const functionCall = () => {
            {%- for arg in func.arguments() %}
            try {
                {{ arg.ffi_converter() }}.checkType({{ arg.nm() }})
            } catch (e) {
                if (e instanceof UniFFITypeError) {
                    e.addItemDescriptionPart("{{ arg.nm() }}");
                }
                throw e;
            }
            {%- endfor %}

            {%- if is_async %}
            return UniFFIScaffolding.callAsync(
            {%- else %}
            return UniFFIScaffolding.callSync(
            {%- endif %}
                {{ function_ids.get(ci, func.ffi_func()) }}, // {{ function_ids.name(ci, func.ffi_func()) }}
                {%- if receiver_ffi_converter != "" %}
                {{ receiver_ffi_converter }}.lower(this),
                {%- endif %}
                {%- for arg in func.arguments() %}
                {{ arg.lower_fn() }}({{ arg.nm() }}),
                {%- endfor %}
            )
        }

        {%- if is_async %}
        try {
            return functionCall().then((result) => handleRustResult(result, liftResult, liftError));
        }  catch (error) {
            return Promise.reject(error)
        }
        {%- else %}
        return handleRustResult(functionCall(), liftResult, liftError);
        {%- endif %}
{%- endmacro %}
