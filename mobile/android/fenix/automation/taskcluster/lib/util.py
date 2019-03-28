import re


def convert_camel_case_into_kebab_case(string):
    # Inspired from https://stackoverflow.com/questions/1175208/elegant-python-function-to-convert-camelcase-to-snake-case  # noqa: E501
    first_pass = re.sub('(.)([A-Z][a-z]+)', r'\1-\2', string)
    return re.sub('([a-z0-9])([A-Z])', r'\1-\2', first_pass).lower()
