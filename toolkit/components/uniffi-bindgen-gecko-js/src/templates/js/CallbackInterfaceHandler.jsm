const {{ cbi.handler() }} = new UniFFICallbackHandler(
    "{{ callback_ids.name(ci, cbi) }}",
    {{ callback_ids.get(ci, cbi) }},
    [
        {%- for method in cbi.methods() %}
        new UniFFICallbackMethodHandler(
            "{{ method.nm() }}",
            [
                {%- for arg in method.arguments() %}
                {{ arg.ffi_converter() }},
                {%- endfor %}
            ],
        ),
        {%- endfor %}
    ]
);

// Allow the shutdown-related functionality to be tested in the unit tests
UnitTestObjs.{{ cbi.handler() }} = {{ cbi.handler() }};
