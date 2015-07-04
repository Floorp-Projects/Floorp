import os
import unittest

JSON_TYPE = None
try:
    import simplejson as json
    assert json
except ImportError:
    import json
    JSON_TYPE = 'json'
else:
    JSON_TYPE = 'simplejson'

import mozharness.base.config as config
from copy import deepcopy

MH_DIR = os.path.dirname(os.path.dirname(__file__))


class TestParseConfigFile(unittest.TestCase):
    def _get_json_config(self, filename=os.path.join(MH_DIR, "configs", "test", "test.json"),
                         output='dict'):
        fh = open(filename)
        contents = json.load(fh)
        fh.close()
        if 'output' == 'dict':
            return dict(contents)
        else:
            return contents

    def _get_python_config(self, filename=os.path.join(MH_DIR, "configs", "test", "test.py"),
                           output='dict'):
        global_dict = {}
        local_dict = {}
        execfile(filename, global_dict, local_dict)
        return local_dict['config']

    def test_json_config(self):
        c = config.BaseConfig(initial_config_file='test/test.json')
        content_dict = self._get_json_config()
        for key in content_dict.keys():
            self.assertEqual(content_dict[key], c._config[key])

    def test_python_config(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        config_dict = self._get_python_config()
        for key in config_dict.keys():
            self.assertEqual(config_dict[key], c._config[key])

    def test_illegal_config(self):
        self.assertRaises(IOError, config.parse_config_file, "this_file_does_not_exist.py", search_path="yadda")

    def test_illegal_suffix(self):
        self.assertRaises(RuntimeError, config.parse_config_file, "test/test.illegal_suffix")

    def test_malformed_json(self):
        if JSON_TYPE == 'simplejson':
            self.assertRaises(json.decoder.JSONDecodeError, config.parse_config_file, "test/test_malformed.json")
        else:
            self.assertRaises(ValueError, config.parse_config_file, "test/test_malformed.json")

    def test_malformed_python(self):
        self.assertRaises(SyntaxError, config.parse_config_file, "test/test_malformed.py")

    def test_multiple_config_files_override_string(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        c.parse_args(['--cfg', 'test/test_override.py,test/test_override2.py'])
        self.assertEqual(c._config['override_string'], 'yay')

    def test_multiple_config_files_override_list(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        c.parse_args(['--cfg', 'test/test_override.py,test/test_override2.py'])
        self.assertEqual(c._config['override_list'], ['yay', 'worked'])

    def test_multiple_config_files_override_dict(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        c.parse_args(['--cfg', 'test/test_override.py,test/test_override2.py'])
        self.assertEqual(c._config['override_dict'], {'yay': 'worked'})

    def test_multiple_config_files_keep_string(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        c.parse_args(['--cfg', 'test/test_override.py,test/test_override2.py'])
        self.assertEqual(c._config['keep_string'], "don't change me")

    def test_optional_config_files_override_value(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        c.parse_args(['--cfg', 'test/test_override.py,test/test_override2.py',
                      '--opt-cfg', 'test/test_optional.py'])
        self.assertEqual(c._config['opt_override'], "new stuff")

    def test_optional_config_files_missing_config(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        c.parse_args(['--cfg', 'test/test_override.py,test/test_override2.py',
                      '--opt-cfg', 'test/test_optional.py,does_not_exist.py'])
        self.assertEqual(c._config['opt_override'], "new stuff")

    def test_optional_config_files_keep_string(self):
        c = config.BaseConfig(initial_config_file='test/test.py')
        c.parse_args(['--cfg', 'test/test_override.py,test/test_override2.py',
                      '--opt-cfg', 'test/test_optional.py'])
        self.assertEqual(c._config['keep_string'], "don't change me")


class TestReadOnlyDict(unittest.TestCase):
    control_dict = {
        'b': '2',
        'c': {'d': '4'},
        'e': ['f', 'g'],
        'e': ['f', 'g', {'turtles': ['turtle1']}],
        'd': {
            'turtles': ['turtle1']
        }
    }

    def get_unlocked_ROD(self):
        r = config.ReadOnlyDict(self.control_dict)
        return r

    def get_locked_ROD(self):
        r = config.ReadOnlyDict(self.control_dict)
        r.lock()
        return r

    def test_create_ROD(self):
        r = self.get_unlocked_ROD()
        self.assertEqual(r, self.control_dict,
                         msg="can't transfer dict to ReadOnlyDict")

    def test_pop_item(self):
        r = self.get_unlocked_ROD()
        r.popitem()
        self.assertEqual(len(r), len(self.control_dict) - 1,
                         msg="can't popitem() ReadOnlyDict when unlocked")

    def test_pop(self):
        r = self.get_unlocked_ROD()
        r.pop('e')
        self.assertEqual(len(r), len(self.control_dict) - 1,
                         msg="can't pop() ReadOnlyDict when unlocked")

    def test_set(self):
        r = self.get_unlocked_ROD()
        r['e'] = 'yarrr'
        self.assertEqual(r['e'], 'yarrr',
                         msg="can't set var in ReadOnlyDict when unlocked")

    def test_del(self):
        r = self.get_unlocked_ROD()
        del r['e']
        self.assertEqual(len(r), len(self.control_dict) - 1,
                         msg="can't del in ReadOnlyDict when unlocked")

    def test_clear(self):
        r = self.get_unlocked_ROD()
        r.clear()
        self.assertEqual(r, {},
                         msg="can't clear() ReadOnlyDict when unlocked")

    def test_set_default(self):
        r = self.get_unlocked_ROD()
        for key in self.control_dict.keys():
            r.setdefault(key, self.control_dict[key])
        self.assertEqual(r, self.control_dict,
                         msg="can't setdefault() ReadOnlyDict when unlocked")

    def test_locked_set(self):
        r = self.get_locked_ROD()
        # TODO use |with self.assertRaises(AssertionError):| if/when we're
        # all on 2.7.
        try:
            r['e'] = 2
        except:
            pass
        else:
            self.assertEqual(0, 1, msg="can set r['e'] when locked")

    def test_locked_del(self):
        r = self.get_locked_ROD()
        try:
            del r['e']
        except:
            pass
        else:
            self.assertEqual(0, 1, "can del r['e'] when locked")

    def test_locked_popitem(self):
        r = self.get_locked_ROD()
        self.assertRaises(AssertionError, r.popitem)

    def test_locked_update(self):
        r = self.get_locked_ROD()
        self.assertRaises(AssertionError, r.update, {})

    def test_locked_set_default(self):
        r = self.get_locked_ROD()
        self.assertRaises(AssertionError, r.setdefault, {})

    def test_locked_pop(self):
        r = self.get_locked_ROD()
        self.assertRaises(AssertionError, r.pop)

    def test_locked_clear(self):
        r = self.get_locked_ROD()
        self.assertRaises(AssertionError, r.clear)

    def test_locked_second_level_dict_pop(self):
        r = self.get_locked_ROD()
        self.assertRaises(AssertionError, r['c'].update, {})

    def test_locked_second_level_list_pop(self):
        r = self.get_locked_ROD()
        with self.assertRaises(AttributeError):
            r['e'].pop()

    def test_locked_third_level_mutate(self):
        r = self.get_locked_ROD()
        with self.assertRaises(AttributeError):
            r['d']['turtles'].append('turtle2')

    def test_locked_object_in_tuple_mutate(self):
        r = self.get_locked_ROD()
        with self.assertRaises(AttributeError):
            r['e'][2]['turtles'].append('turtle2')

    def test_locked_second_level_dict_pop2(self):
        r = self.get_locked_ROD()
        self.assertRaises(AssertionError, r['c'].update, {})

    def test_locked_second_level_list_pop2(self):
        r = self.get_locked_ROD()
        with self.assertRaises(AttributeError):
            r['e'].pop()

    def test_locked_third_level_mutate2(self):
        r = self.get_locked_ROD()
        with self.assertRaises(AttributeError):
            r['d']['turtles'].append('turtle2')

    def test_locked_object_in_tuple_mutate2(self):
        r = self.get_locked_ROD()
        with self.assertRaises(AttributeError):
            r['e'][2]['turtles'].append('turtle2')

    def test_locked_deepcopy_set(self):
        r = self.get_locked_ROD()
        c = deepcopy(r)
        c['e'] = 'hey'
        self.assertEqual(c['e'], 'hey', "can't set var in ROD after deepcopy")


class TestActions(unittest.TestCase):
    all_actions = ['a', 'b', 'c', 'd', 'e']
    default_actions = ['b', 'c', 'd']

    def test_verify_actions(self):
        c = config.BaseConfig(initial_config_file='test/test.json')
        try:
            c.verify_actions(['not_a_real_action'])
        except:
            pass
        else:
            self.assertEqual(0, 1, msg="verify_actions() didn't die on invalid action")
        c = config.BaseConfig(initial_config_file='test/test.json')
        returned_actions = c.verify_actions(c.all_actions)
        self.assertEqual(c.all_actions, returned_actions,
                         msg="returned actions from verify_actions() changed")

    def test_default_actions(self):
        c = config.BaseConfig(default_actions=self.default_actions,
                              all_actions=self.all_actions,
                              initial_config_file='test/test.json')
        self.assertEqual(self.default_actions, c.get_actions(),
                         msg="default_actions broken")

    def test_no_action1(self):
        c = config.BaseConfig(default_actions=self.default_actions,
                              all_actions=self.all_actions,
                              initial_config_file='test/test.json')
        c.parse_args(args=['foo', '--no-action', 'a'])
        self.assertEqual(self.default_actions, c.get_actions(),
                         msg="--no-ACTION broken")

    def test_no_action2(self):
        c = config.BaseConfig(default_actions=self.default_actions,
                              all_actions=self.all_actions,
                              initial_config_file='test/test.json')
        c.parse_args(args=['foo', '--no-c'])
        self.assertEqual(['b', 'd'], c.get_actions(),
                         msg="--no-ACTION broken")

    def test_add_action(self):
        c = config.BaseConfig(default_actions=self.default_actions,
                              all_actions=self.all_actions,
                              initial_config_file='test/test.json')
        c.parse_args(args=['foo', '--add-action', 'e'])
        self.assertEqual(['b', 'c', 'd', 'e'], c.get_actions(),
                         msg="--add-action ACTION broken")

    def test_only_action(self):
        c = config.BaseConfig(default_actions=self.default_actions,
                              all_actions=self.all_actions,
                              initial_config_file='test/test.json')
        c.parse_args(args=['foo', '--a', '--e'])
        self.assertEqual(['a', 'e'], c.get_actions(),
                         msg="--ACTION broken")

if __name__ == '__main__':
    unittest.main()
