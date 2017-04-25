import colorsys  # It turns out Python already does HSL -> RGB!
trim = lambda s: s if not s.endswith('.0') else s[:-2]
print('[')
print(',\n'.join(
    function_format % tuple(
        [
            function_name,
            hue,
            trim(str(saturation / 10.)),
            trim(str(lightness / 10.)),
            alpha_format % round(alpha / 255., 2) if alpha is not None else ''
        ] + [
            trim(str(min(255, component * 256)))
            for component in colorsys.hls_to_rgb(
                hue / 360.,
                lightness / 1000.,
                saturation / 1000.
            )
        ] + [
            alpha if alpha is not None else 255
        ]
    )
    for function_format, alpha_format in [
        ('"%s(%s, %s%%, %s%%%s)", [%s, %s, %s, %s]', ', %s'),
        ('"%s(%s %s%% %s%%%s)", [%s, %s, %s, %s]', ' / %s')
    ]
    for function_name in ["hsl", "hsla"]
    for alpha in [None, 255, 64, 0]
    for lightness in range(0, 1001, 125)
    for saturation in range(0, 1001, 125)
    for hue in range(0, 360, 30)
))
print(']')
