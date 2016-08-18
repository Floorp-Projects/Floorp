import os

import pystache
import yaml

# Key used in template inheritance...
INHERITS_KEY = '$inherits'


def merge_to(source, dest):
    '''
    Merge dict and arrays (override scalar values)

    :param dict source: to copy from
    :param dict dest: to copy to.
    '''

    for key, value in source.items():
        # Override mismatching or empty types
        if type(value) != type(dest.get(key)):  # noqa
            dest[key] = source[key]
            continue

        # Merge dict
        if isinstance(value, dict):
            merge_to(value, dest[key])
            continue

        if isinstance(value, list):
            dest[key] = dest[key] + source[key]
            continue

        dest[key] = source[key]

    return dest


class TemplatesException(Exception):
    pass


class Templates():
    '''
    The taskcluster integration makes heavy use of yaml to describe tasks this
    class handles the loading/rendering.
    '''

    def __init__(self, root):
        '''
        Initialize the template render.

        :param str root: Root path where to load yaml files.
        '''
        if not root:
            raise TemplatesException('Root is required')

        if not os.path.isdir(root):
            raise TemplatesException('Root must be a directory')

        self.root = root

    def _inherits(self, path, obj, properties, seen):
        blueprint = obj.pop(INHERITS_KEY)
        seen.add(path)

        # Resolve the path here so we can detect circular references.
        template = self.resolve_path(blueprint.get('from'))
        variables = blueprint.get('variables', {})

        # Passed parameters override anything in the task itself.
        for key in properties:
            variables[key] = properties[key]

        if not template:
            msg = '"{}" inheritance template missing'.format(path)
            raise TemplatesException(msg)

        if template in seen:
            msg = 'Error while handling "{}" in "{}" circular template' + \
                  'inheritance seen \n  {}'
            raise TemplatesException(msg.format(path, template, seen))

        try:
            out = self.load(template, variables, seen)
        except TemplatesException as e:
            msg = 'Error expanding parent ("{}") of "{}" original error {}'
            raise TemplatesException(msg.format(template, path, str(e)))

        # Anything left in obj is merged into final results (and overrides)
        return merge_to(obj, out)

    def render(self, path, content, parameters, seen):
        '''
        Renders a given yaml string.

        :param str path:  used to prevent infinite recursion in inheritance.
        :param str content: Of yaml file.
        :param dict parameters: For mustache templates.
        :param set seen: Seen files (used for inheritance)
        '''
        content = pystache.render(content, parameters)
        result = yaml.load(content)

        # In addition to the usual template logic done by mustache we also
        # handle special '$inherit' dict keys.
        if isinstance(result, dict) and INHERITS_KEY in result:
            return self._inherits(path, result, parameters, seen)

        return result

    def resolve_path(self, path):
        return os.path.join(self.root, path)

    def load(self, path, parameters=None, seen=None):
        '''
        Load an render the given yaml path.

        :param str path: Location of yaml file to load (relative to root).
        :param dict parameters: To template yaml file with.
        '''
        seen = seen or set()

        if not path:
            raise TemplatesException('path is required')

        path = self.resolve_path(path)

        if not os.path.isfile(path):
            raise TemplatesException('"{}" is not a file'.format(path))

        content = open(path).read()
        return self.render(path, content, parameters, seen)
