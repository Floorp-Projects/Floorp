# This config is for `mach try release` to support beta simulations.

config = {
    'replacements': [
        ('build/defines.sh',
         'EARLY_BETA_OR_EARLIER=1',
         'EARLY_BETA_OR_EARLIER='),
    ],
}
