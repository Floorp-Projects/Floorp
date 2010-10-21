# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Corporation Code.
#
# The Initial Developer of the Original Code is
# Mikeal Rogers.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mikeal Rogers <mikeal.rogers@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

def init_jsobject(cls, bridge, name, value, description=None):
    """Initialize a js object that is a subclassed base type; int, str, unicode, float."""
    obj = cls(value)
    obj._bridge_ = bridge
    obj._name_ = name
    obj._description_ = description
    return obj

def create_jsobject(bridge, fullname, value=None, obj_type=None, override_set=False):
    """Create a single JSObject for named object on other side of the bridge.
    
    Handles various initization cases for different JSObjects."""
    description = bridge.describe(fullname)
    obj_type = description['type']
    value = description.get('data', None)
    
    if value is True or value is False:
        return value

    if js_type_cases.has_key(obj_type):
        cls, needs_init = js_type_cases[obj_type]
        # Objects that requires initialization are base types that have "values".
        if needs_init:
            obj = init_jsobject(cls, bridge, fullname, value, description=description)
        else:
            obj = cls(bridge, fullname, description=description, override_set=override_set)
        return obj
    else:
        # Something very bad happened, we don't have a representation for the given type.
        raise TypeError("Don't have a JSObject for javascript type "+obj_type)
    
class JSObject(object):
    """Base javascript object representation."""
    _loaded_ = False
    
    def __init__(self, bridge, name, override_set=False, description=None, *args, **kwargs):
        self._bridge_ = bridge
        if not override_set:
            name = bridge.set(name)['data']
        self._name_ = name
        self._description_ = description
    
    def __jsget__(self, name):
        """Abstraction for final step in get events; __getitem__ and __getattr__.
        """
        result = create_jsobject(self._bridge_, name, override_set=True)
        return result
    
    def __getattr__(self, name):
        """Get the object from jsbridge. 
        
        Handles lazy loading of all attributes of self."""
        # A little hack so that ipython returns all the names.
        if name == '_getAttributeNames':
            return lambda : self._bridge_.describe(self._name_)['attributes']
            
        attributes = self._bridge_.describe(self._name_)['attributes']
        if name in attributes:
            return self.__jsget__(self._name_+'["'+name+'"]')
        else:
            raise AttributeError(name+" is undefined.")
    
    __getitem__ = __getattr__
        
    def __setattr__(self, name, value):
        """Set the given JSObject as an attribute of this JSObject and make proper javascript
        assignment on the other side of the bridge."""
        if name.startswith('_') and name.endswith('_'):
            return object.__setattr__(self, name, value)

        response = self._bridge_.setAttribute(self._name_, name, value)
        object.__setattr__(self, name, create_jsobject(self._bridge_, response['data'], override_set=True))
    
    __setitem__ = __setattr__

class JSFunction(JSObject):
    """Javascript function represenation.
    
    Returns a JSObject instance for the serialized js type with 
    name set to the full javascript call for this function. 
    """    
    
    def __init__(self, bridge, name, override_set=False, description=None, *args, **kwargs):
        self._bridge_ = bridge
        self._name_ = name
        self._description_ = description
    
    def __call__(self, *args):
        response = self._bridge_.execFunction(self._name_, args)
        if response['data'] is not None:
            return create_jsobject(self._bridge_, response['data'], override_set=True)


class JSString(JSObject, unicode):
    "Javascript string representation."
    __init__ = unicode.__init__

class JSInt(JSObject, int): 
    """Javascript number representation for Python int."""
    __init__ = int.__init__

class JSFloat(JSObject, float):
    """Javascript number representation for Python float."""
    __init__ = float.__init__
    
class JSUndefined(JSObject):
    """Javascript undefined representation."""    
    __str__ = lambda self : "undefined"

    def __cmp__(self, other):
        if isinstance(other, JSUndefined):
            return True
        else:
            return False

    __nonzero__ = lambda self: False

js_type_cases = {'function'  :(JSFunction, False,), 
                  'object'   :(JSObject, False,), 
                  'array'    :(JSObject, False,),
                  'string'   :(JSString, True,), 
                  'number'   :(JSFloat, True,),
                  'undefined':(JSUndefined, False,),
                  'null'     :(JSObject, False,),
                  }
py_type_cases = {unicode  :JSString,
                  str     :JSString,
                  int     :JSInt,
                  float   :JSFloat,
                  }
