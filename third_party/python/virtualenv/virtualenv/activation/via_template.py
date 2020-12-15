from __future__ import absolute_import, unicode_literals

import os
import sys
from abc import ABCMeta, abstractmethod

from six import add_metaclass

from virtualenv.util.six import ensure_text

from .activator import Activator

if sys.version_info >= (3, 7):
    from importlib.resources import read_text
else:
    from importlib_resources import read_text


@add_metaclass(ABCMeta)
class ViaTemplateActivator(Activator):
    @abstractmethod
    def templates(self):
        raise NotImplementedError

    def generate(self, creator):
        dest_folder = creator.bin_dir
        replacements = self.replacements(creator, dest_folder)
        generated = self._generate(replacements, self.templates(), dest_folder, creator)
        if self.flag_prompt is not None:
            creator.pyenv_cfg["prompt"] = self.flag_prompt
        return generated

    def replacements(self, creator, dest_folder):
        return {
            "__VIRTUAL_PROMPT__": "" if self.flag_prompt is None else self.flag_prompt,
            "__VIRTUAL_ENV__": ensure_text(str(creator.dest)),
            "__VIRTUAL_NAME__": creator.env_name,
            "__BIN_NAME__": ensure_text(str(creator.bin_dir.relative_to(creator.dest))),
            "__PATH_SEP__": ensure_text(os.pathsep),
        }

    def _generate(self, replacements, templates, to_folder, creator):
        generated = []
        for template in templates:
            text = self.instantiate_template(replacements, template, creator)
            dest = to_folder / self.as_name(template)
            dest.write_text(text, encoding="utf-8")
            generated.append(dest)
        return generated

    def as_name(self, template):
        return template.name

    def instantiate_template(self, replacements, template, creator):
        # read text and do replacements
        text = read_text(self.__module__, str(template), encoding="utf-8", errors="strict")
        for key, value in replacements.items():
            value = self._repr_unicode(creator, value)
            text = text.replace(key, value)
        return text

    @staticmethod
    def _repr_unicode(creator, value):
        # by default we just let it be unicode
        return value
