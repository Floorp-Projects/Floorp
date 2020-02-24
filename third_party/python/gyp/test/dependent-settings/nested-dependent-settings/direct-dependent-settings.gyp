{
    "targets": [
        {
            "target_name": "settings",
            "type": "none",
            "all_dependent_settings": {
                "target_conditions": [
                    ["'library' in _type", {"direct_dependent_settings": {}}]
                ]
            },
        },
        {
            "target_name": "library",
            "type": "static_library",
            "dependencies": ["settings"],
        },
    ]
}

