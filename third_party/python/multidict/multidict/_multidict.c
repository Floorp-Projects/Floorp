#include "Python.h"
#include "structmember.h"

// Include order important
#include "_multilib/defs.h"
#include "_multilib/istr.h"
#include "_multilib/pair_list.h"
#include "_multilib/dict.h"
#include "_multilib/iter.h"
#include "_multilib/views.h"

static PyObject *collections_abc_mapping;
static PyObject *collections_abc_mut_mapping;
static PyObject *collections_abc_mut_multi_mapping;

static PyTypeObject multidict_type;
static PyTypeObject cimultidict_type;
static PyTypeObject multidict_proxy_type;
static PyTypeObject cimultidict_proxy_type;

static PyObject *repr_func;

#define MultiDict_CheckExact(o) (Py_TYPE(o) == &multidict_type)
#define CIMultiDict_CheckExact(o) (Py_TYPE(o) == &cimultidict_type)
#define MultiDictProxy_CheckExact(o) (Py_TYPE(o) == &multidict_proxy_type)
#define CIMultiDictProxy_CheckExact(o) (Py_TYPE(o) == &cimultidict_proxy_type)

/* Helper macro for something like isinstance(obj, Base) */
#define _MultiDict_Check(o)              \
    ((MultiDict_CheckExact(o)) ||        \
     (CIMultiDict_CheckExact(o)) ||      \
     (MultiDictProxy_CheckExact(o)) ||   \
     (CIMultiDictProxy_CheckExact(o)))

/******************** Internal Methods ********************/

/* Forward declaration */
static PyObject *multidict_items(MultiDictObject *self);

static inline PyObject *
_multidict_getone(MultiDictObject *self, PyObject *key, PyObject *_default)
{
    PyObject *val = pair_list_get_one(&self->pairs, key);

    if (val == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        Py_INCREF(_default);
        return _default;
    }

    return val;
}

static inline int
_multidict_eq(MultiDictObject *self, MultiDictObject *other)
{
    Py_ssize_t pos1 = 0,
               pos2 = 0;

    Py_hash_t h1 = 0,
              h2 = 0;

    PyObject *identity1 = NULL,
             *identity2 = NULL,
             *value1    = NULL,
             *value2    = NULL;

    int cmp_identity = 0,
        cmp_value    = 0;

    if (self == other) {
        return 1;
    }

    if (pair_list_len(&self->pairs) != pair_list_len(&other->pairs)) {
        return 0;
    }

    while (_pair_list_next(&self->pairs, &pos1, &identity1, NULL, &value1, &h1) &&
           _pair_list_next(&other->pairs, &pos2, &identity2, NULL, &value2, &h2))
    {
        if (h1 != h2) {
            return 0;
        }
        cmp_identity = PyObject_RichCompareBool(identity1, identity2, Py_NE);
        if (cmp_identity < 0) {
            return -1;
        }
        cmp_value = PyObject_RichCompareBool(value1, value2, Py_NE);
        if (cmp_value < 0) {
            return -1;
        }
        if (cmp_identity || cmp_value) {
            return 0;
        }
    }

    return 1;
}

static inline int
_multidict_update_items(MultiDictObject *self, pair_list_t *pairs)
{
    return pair_list_update(&self->pairs, pairs);
}

static inline int
_multidict_append_items(MultiDictObject *self, pair_list_t *pairs)
{
    PyObject *key   = NULL,
             *value = NULL;

    Py_ssize_t pos = 0;

    while (_pair_list_next(pairs, &pos, NULL, &key, &value, NULL)) {
        if (pair_list_add(&self->pairs, key, value) < 0) {
            return -1;
        }
    }

    return 0;
}

static inline int
_multidict_append_items_seq(MultiDictObject *self, PyObject *arg,
                            const char *name)
{
    PyObject *key   = NULL,
             *value = NULL,
             *item  = NULL,
             *iter  = PyObject_GetIter(arg);

    if (iter == NULL) {
        return -1;
    }

    while ((item = PyIter_Next(iter)) != NULL) {
        if (PyTuple_CheckExact(item)) {
            if (PyTuple_GET_SIZE(item) != 2) {
                goto invalid_type;
            }
            key = PyTuple_GET_ITEM(item, 0);
            Py_INCREF(key);
            value = PyTuple_GET_ITEM(item, 1);
            Py_INCREF(value);
        }
        else if (PyList_CheckExact(item)) {
            if (PyList_GET_SIZE(item) != 2) {
                goto invalid_type;
            }
            key = PyList_GET_ITEM(item, 0);
            Py_INCREF(key);
            value = PyList_GET_ITEM(item, 1);
            Py_INCREF(value);
        }
        else if (PySequence_Check(item)) {
            if (PySequence_Size(item) != 2) {
                goto invalid_type;
            }
            key = PySequence_GetItem(item, 0);
            value = PySequence_GetItem(item, 1);
        } else {
            goto invalid_type;
        }

        if (pair_list_add(&self->pairs, key, value) < 0) {
            goto fail;
        }
        Py_CLEAR(key);
        Py_CLEAR(value);
        Py_CLEAR(item);
    }

    Py_DECREF(iter);

    if (PyErr_Occurred()) {
        return -1;
    }

    return 0;
invalid_type:
    PyErr_Format(
        PyExc_TypeError,
        "%s takes either dict or list of (key, value) pairs",
        name,
        NULL
    );
    goto fail;
fail:
    Py_XDECREF(key);
    Py_XDECREF(value);
    Py_XDECREF(item);
    Py_DECREF(iter);
    return -1;
}

static inline int
_multidict_list_extend(PyObject *list, PyObject *target_list)
{
    PyObject *item = NULL,
             *iter = PyObject_GetIter(target_list);

    if (iter == NULL) {
        return -1;
    }

    while ((item = PyIter_Next(iter)) != NULL) {
        if (PyList_Append(list, item) < 0) {
            Py_DECREF(item);
            Py_DECREF(iter);
            return -1;
        }
        Py_DECREF(item);
    }

    Py_DECREF(iter);

    if (PyErr_Occurred()) {
        return -1;
    }

    return 0;
}

static inline int
_multidict_extend_with_args(MultiDictObject *self, PyObject *arg,
                            PyObject *kwds, const char *name, int do_add)
{
    PyObject *arg_items  = NULL, /* tracked by GC */
             *kwds_items = NULL; /* new reference */
    pair_list_t *pairs = NULL;

    int err = 0;

    if (kwds && !PyArg_ValidateKeywordArguments(kwds)) {
        return -1;
    }

    // TODO: mb can be refactored more clear
    if (_MultiDict_Check(arg) && kwds == NULL) {
        if (MultiDict_CheckExact(arg) || CIMultiDict_CheckExact(arg)) {
            pairs = &((MultiDictObject*)arg)->pairs;
        } else if (MultiDictProxy_CheckExact(arg) || CIMultiDictProxy_CheckExact(arg)) {
            pairs = &((MultiDictProxyObject*)arg)->md->pairs;
        }

        if (do_add) {
            return _multidict_append_items(self, pairs);
        }

        return _multidict_update_items(self, pairs);
    }

    if (PyObject_HasAttrString(arg, "items")) {
        if (_MultiDict_Check(arg)) {
            arg_items = multidict_items((MultiDictObject*)arg);
        } else {
            arg_items = PyMapping_Items(arg);
        }
        if (arg_items == NULL) {
            return -1;
        }
    } else {
        arg_items = arg;
        Py_INCREF(arg_items);
    }

    if (kwds) {
        PyObject *tmp = PySequence_List(arg_items);
        Py_DECREF(arg_items);
        arg_items = tmp;
        if (arg_items == NULL) {
            return -1;
        }

        kwds_items = PyDict_Items(kwds);
        if (kwds_items == NULL) {
            Py_DECREF(arg_items);
            return -1;
        }
        err = _multidict_list_extend(arg_items, kwds_items);
        Py_DECREF(kwds_items);
        if (err < 0) {
            Py_DECREF(arg_items);
            return -1;
        }
    }

    if (do_add) {
        err = _multidict_append_items_seq(self, arg_items, name);
    } else {
        err =  pair_list_update_from_seq(&self->pairs, arg_items);
    }

    Py_DECREF(arg_items);

    return err;
}

static inline int
_multidict_extend_with_kwds(MultiDictObject *self, PyObject *kwds,
                                 const char *name, int do_add)
{
    PyObject *arg = NULL;

    int err = 0;

    if (!PyArg_ValidateKeywordArguments(kwds)) {
        return -1;
    }

    arg = PyDict_Items(kwds);
    if (do_add) {
        err = _multidict_append_items_seq(self, arg, name);
    } else {
        err = pair_list_update_from_seq(&self->pairs, arg);
    }

    Py_DECREF(arg);
    return err;
}

static inline int
_multidict_extend(MultiDictObject *self, PyObject *args, PyObject *kwds,
                  const char *name, int do_add)
{
    PyObject *arg = NULL;

    if (args && PyObject_Length(args) > 1)  {
        PyErr_Format(
            PyExc_TypeError,
            "%s takes at most 1 positional argument (%zd given)",
            name, PyObject_Length(args), NULL
        );
        return -1;
    }

    if (args && PyObject_Length(args) > 0) {
        if (!PyArg_UnpackTuple(args, name, 0, 1, &arg)) {
            return -1;
        }
        if (_multidict_extend_with_args(self, arg, kwds, name, do_add) < 0) {
            return -1;
        }
    } else if (kwds && PyObject_Length(kwds) > 0) {
        if (_multidict_extend_with_kwds(self, kwds, name, do_add) < 0) {
            return -1;
        }
    }

    return 0;
}

static inline PyObject *
_multidict_copy(MultiDictObject *self, PyTypeObject *multidict_tp_object)
{
    MultiDictObject *new_multidict = NULL;

    PyObject *arg_items = NULL,
             *items     = NULL;

    new_multidict = (MultiDictObject*)PyType_GenericNew(
        multidict_tp_object, NULL, NULL);
    if (new_multidict == NULL) {
        return NULL;
    }

    if (multidict_tp_object->tp_init(
        (PyObject*)new_multidict, NULL, NULL) < 0)
    {
        return NULL;
    }

    items = multidict_items(self);
    if (items == NULL) {
        goto fail;
    }

    // TODO: "Implementation looks as slow as possible ..."
    arg_items = PyTuple_New(1);
    if (arg_items == NULL) {
        goto fail;
    }

    Py_INCREF(items);
    PyTuple_SET_ITEM(arg_items, 0, items);

    if (_multidict_extend(
        new_multidict, arg_items, NULL, "copy", 1) < 0)
    {
        goto fail;
    }

    Py_DECREF(items);
    Py_DECREF(arg_items);

    return (PyObject*)new_multidict;

fail:
    Py_XDECREF(items);
    Py_XDECREF(arg_items);

    Py_DECREF(new_multidict);

    return NULL;
}

static inline PyObject *
_multidict_proxy_copy(MultiDictProxyObject *self, PyTypeObject *type)
{
    PyObject *new_multidict = PyType_GenericNew(type, NULL, NULL);
    if (new_multidict == NULL) {
        goto fail;
    }
    if (type->tp_init(new_multidict, NULL, NULL) < 0) {
        goto fail;
    }
    if (_multidict_extend_with_args(
        (MultiDictObject*)new_multidict, (PyObject*)self, NULL, "copy", 1) < 0)
    {
        goto fail;
    }

    return new_multidict;

fail:
    Py_XDECREF(new_multidict);
    return NULL;
}


/******************** Base Methods ********************/

static inline PyObject *
multidict_getall(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *list     = NULL,
             *key      = NULL,
             *_default = NULL;

    static char *getall_keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getall",
                                     getall_keywords, &key, &_default))
    {
        return NULL;
    }

    list = pair_list_get_all(&self->pairs, key);

    if (list == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        Py_INCREF(_default);
        return _default;
    }

    return list;
}

static inline PyObject *
multidict_getone(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL;

    static char *getone_keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getone",
                                     getone_keywords, &key, &_default))
    {
        return NULL;
    }

    return _multidict_getone(self, key, _default);
}

static inline PyObject *
multidict_get(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = Py_None,
             *ret;

    static char *getone_keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:getone",
                                     getone_keywords, &key, &_default))
    {
        return NULL;
    }
    ret = _multidict_getone(self, key, _default);
    return ret;
}

static inline PyObject *
multidict_keys(MultiDictObject *self)
{
    return multidict_keysview_new((PyObject*)self);
}

static inline PyObject *
multidict_items(MultiDictObject *self)
{
    return multidict_itemsview_new((PyObject*)self);
}

static inline PyObject *
multidict_values(MultiDictObject *self)
{
    return multidict_valuesview_new((PyObject*)self);
}

static inline PyObject *
multidict_reduce(MultiDictObject *self)
{
    PyObject *items      = NULL,
             *items_list = NULL,
             *args       = NULL,
             *result     = NULL;

    items = multidict_items(self);
    if (items == NULL) {
        goto ret;
    }

    items_list = PySequence_List(items);
    if (items_list == NULL) {
        goto ret;
    }

    args = PyTuple_Pack(1, items_list);
    if (args == NULL) {
        goto ret;
    }

    result = PyTuple_Pack(2, Py_TYPE(self), args);

ret:
    Py_XDECREF(args);
    Py_XDECREF(items_list);
    Py_XDECREF(items);

    return result;
}

static inline PyObject *
multidict_repr(PyObject *self)
{
    return PyObject_CallFunctionObjArgs(
        repr_func, self, NULL);
}

static inline Py_ssize_t
multidict_mp_len(MultiDictObject *self)
{
    return pair_list_len(&self->pairs);
}

static inline PyObject *
multidict_mp_subscript(MultiDictObject *self, PyObject *key)
{
    return _multidict_getone(self, key, NULL);
}

static inline int
multidict_mp_as_subscript(MultiDictObject *self, PyObject *key, PyObject *val)
{
    if (val == NULL) {
        return pair_list_del(&self->pairs, key);
    } else {
        return pair_list_replace(&self->pairs, key, val);
    }
}

static inline int
multidict_sq_contains(MultiDictObject *self, PyObject *key)
{
    return pair_list_contains(&self->pairs, key);
}

static inline PyObject *
multidict_tp_iter(MultiDictObject *self)
{
    return multidict_keys_iter_new(self);
}

static inline PyObject *
multidict_tp_richcompare(PyObject *self, PyObject *other, int op)
{
    // TODO: refactoring me with love

    int cmp = 0;

    if (op != Py_EQ && op != Py_NE) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    if (MultiDict_CheckExact(other) || CIMultiDict_CheckExact(other)) {
        cmp = _multidict_eq(
            (MultiDictObject*)self,
            (MultiDictObject*)other
        );
        if (cmp < 0) {
            return NULL;
        }
        if (op == Py_NE) {
            cmp = !cmp;
        }
        return PyBool_FromLong(cmp);
    }

    if (MultiDictProxy_CheckExact(other) || CIMultiDictProxy_CheckExact(other)) {
        cmp = _multidict_eq(
            (MultiDictObject*)self,
            ((MultiDictProxyObject*)other)->md
        );
        if (cmp < 0) {
            return NULL;
        }
        if (op == Py_NE) {
            cmp = !cmp;
        }
        return PyBool_FromLong(cmp);
    }

    cmp = PyObject_IsInstance(other, (PyObject*)collections_abc_mapping);
    if (cmp < 0) {
        return NULL;
    }

    if (cmp) {
        cmp = pair_list_eq_to_mapping(&((MultiDictObject*)self)->pairs, other);
        if (cmp < 0) {
            return NULL;
        }
        if (op == Py_NE) {
            cmp = !cmp;
        }
        return PyBool_FromLong(cmp);
    }

    Py_RETURN_NOTIMPLEMENTED;
}

static inline void
multidict_tp_dealloc(MultiDictObject *self)
{
    PyObject_GC_UnTrack(self);
    Py_TRASHCAN_SAFE_BEGIN(self);
    if (self->weaklist != NULL) {
        PyObject_ClearWeakRefs((PyObject *)self);
    };
    pair_list_dealloc(&self->pairs);
    Py_TYPE(self)->tp_free((PyObject *)self);
    Py_TRASHCAN_SAFE_END(self);
}

static inline int
multidict_tp_traverse(MultiDictObject *self, visitproc visit, void *arg)
{
    return pair_list_traverse(&self->pairs, visit, arg);
}

static inline int
multidict_tp_clear(MultiDictObject *self)
{
    return pair_list_clear(&self->pairs);
}

PyDoc_STRVAR(multidict_getall_doc,
"Return a list of all values matching the key.");

PyDoc_STRVAR(multidict_getone_doc,
"Get first value matching the key.");

PyDoc_STRVAR(multidict_get_doc,
"Get first value matching the key.\n\nThe method is alias for .getone().");

PyDoc_STRVAR(multidict_keys_doc,
"Return a new view of the dictionary's keys.");

PyDoc_STRVAR(multidict_items_doc,
"Return a new view of the dictionary's items *(key, value) pairs).");

PyDoc_STRVAR(multidict_values_doc,
"Return a new view of the dictionary's values.");

/******************** MultiDict ********************/

static inline int
multidict_tp_init(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    if (pair_list_init(&self->pairs) < 0) {
        return -1;
    }
    if (_multidict_extend(self, args, kwds, "MultiDict", 1) < 0) {
        return -1;
    }
    return 0;
}

static inline PyObject *
multidict_add(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key = NULL,
             *val = NULL;

    static char *kwlist[] = {"key", "value", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO:add",
                                     kwlist, &key, &val))
    {
        return NULL;
    }

    if (pair_list_add(&self->pairs, key, val) < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static inline PyObject *
multidict_copy(MultiDictObject *self)
{
    return _multidict_copy(self, &multidict_type);
}

static inline PyObject *
multidict_extend(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    if (_multidict_extend(self, args, kwds, "extend", 1) < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static inline PyObject *
multidict_clear(MultiDictObject *self)
{
    if (pair_list_clear(&self->pairs) < 0) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static inline PyObject *
multidict_setdefault(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL;

    static char *setdefault_keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:setdefault",
                                     setdefault_keywords, &key, &_default))
    {
        return NULL;
    }
    return pair_list_set_default(&self->pairs, key, _default);
}

static inline PyObject *
multidict_popone(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL,
             *ret_val  = NULL;

    static char *popone_keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:popone",
                                     popone_keywords, &key, &_default))
    {
        return NULL;
    }

    ret_val = pair_list_pop_one(&self->pairs, key);

    if (ret_val == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        Py_INCREF(_default);
        return _default;
    }

    return ret_val;
}

static inline PyObject *
multidict_popall(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *key      = NULL,
             *_default = NULL,
             *ret_val  = NULL;

    static char *popall_keywords[] = {"key", "default", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:popall",
                                     popall_keywords, &key, &_default))
    {
        return NULL;
    }

    ret_val = pair_list_pop_all(&self->pairs, key);

    if (ret_val == NULL &&
        PyErr_ExceptionMatches(PyExc_KeyError) &&
        _default != NULL)
    {
        PyErr_Clear();
        Py_INCREF(_default);
        return _default;
    }

    return ret_val;
}

static inline PyObject *
multidict_popitem(MultiDictObject *self)
{
    return pair_list_pop_item(&self->pairs);
}

static inline PyObject *
multidict_update(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    if (_multidict_extend(self, args, kwds, "update", 0) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(multidict_add_doc,
"Add the key and value, not overwriting any previous value.");

PyDoc_STRVAR(multidict_copy_doc,
"Return a copy of itself.");

PyDoc_STRVAR(multdicit_method_extend_doc,
"Extend current MultiDict with more values.\n\
This method must be used instead of update.");

PyDoc_STRVAR(multidict_clear_doc,
"Remove all items from MultiDict");

PyDoc_STRVAR(multidict_setdefault_doc,
"Return value for key, set value to default if key is not present.");

PyDoc_STRVAR(multidict_popone_doc,
"Remove the last occurrence of key and return the corresponding value.\n\n\
If key is not found, default is returned if given, otherwise KeyError is \
raised.\n");

PyDoc_STRVAR(multidict_popall_doc,
"Remove all occurrences of key and return the list of corresponding values.\n\n\
If key is not found, default is returned if given, otherwise KeyError is \
raised.\n");

PyDoc_STRVAR(multidict_popitem_doc,
"Remove and return an arbitrary (key, value) pair.");

PyDoc_STRVAR(multidict_update_doc,
"Update the dictionary from *other*, overwriting existing keys.");


#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 9
#define multidict_class_getitem Py_GenericAlias
#else
static inline PyObject *
multidict_class_getitem(PyObject *self, PyObject *arg)
{
    Py_INCREF(self);
    return self;
}
#endif


PyDoc_STRVAR(sizeof__doc__,
"D.__sizeof__() -> size of D in memory, in bytes");

static inline PyObject *
_multidict_sizeof(MultiDictObject *self)
{
    Py_ssize_t size = sizeof(MultiDictObject);
    if (self->pairs.pairs != self->pairs.buffer) {
        size += (Py_ssize_t)sizeof(pair_t) * self->pairs.capacity;
    }
    return PyLong_FromSsize_t(size);
}


static PySequenceMethods multidict_sequence = {
    .sq_contains = (objobjproc)multidict_sq_contains,
};

static PyMappingMethods multidict_mapping = {
    .mp_length = (lenfunc)multidict_mp_len,
    .mp_subscript = (binaryfunc)multidict_mp_subscript,
    .mp_ass_subscript = (objobjargproc)multidict_mp_as_subscript,
};

static PyMethodDef multidict_methods[] = {
    {
        "getall",
        (PyCFunction)multidict_getall,
        METH_VARARGS | METH_KEYWORDS,
        multidict_getall_doc
    },
    {
        "getone",
        (PyCFunction)multidict_getone,
        METH_VARARGS | METH_KEYWORDS,
        multidict_getone_doc
    },
    {
        "get",
        (PyCFunction)multidict_get,
        METH_VARARGS | METH_KEYWORDS,
        multidict_get_doc
    },
    {
        "keys",
        (PyCFunction)multidict_keys,
        METH_NOARGS,
        multidict_keys_doc
    },
    {
        "items",
        (PyCFunction)multidict_items,
        METH_NOARGS,
        multidict_items_doc
    },
    {
        "values",
        (PyCFunction)multidict_values,
        METH_NOARGS,
        multidict_values_doc
    },
    {
        "add",
        (PyCFunction)multidict_add,
        METH_VARARGS | METH_KEYWORDS,
        multidict_add_doc
    },
    {
        "copy",
        (PyCFunction)multidict_copy,
        METH_NOARGS,
        multidict_copy_doc
    },
    {
        "extend",
        (PyCFunction)multidict_extend,
        METH_VARARGS | METH_KEYWORDS,
        multdicit_method_extend_doc
    },
    {
        "clear",
        (PyCFunction)multidict_clear,
        METH_NOARGS,
        multidict_clear_doc
    },
    {
        "setdefault",
        (PyCFunction)multidict_setdefault,
        METH_VARARGS | METH_KEYWORDS,
        multidict_setdefault_doc
    },
    {
        "popone",
        (PyCFunction)multidict_popone,
        METH_VARARGS | METH_KEYWORDS,
        multidict_popone_doc
    },
    {
        "pop",
        (PyCFunction)multidict_popone,
        METH_VARARGS | METH_KEYWORDS,
        multidict_popone_doc
    },
    {
        "popall",
        (PyCFunction)multidict_popall,
        METH_VARARGS | METH_KEYWORDS,
        multidict_popall_doc
    },
    {
        "popitem",
        (PyCFunction)multidict_popitem,
        METH_NOARGS,
        multidict_popitem_doc
    },
    {
        "update",
        (PyCFunction)multidict_update,
        METH_VARARGS | METH_KEYWORDS,
        multidict_update_doc
    },
    {
        "__reduce__",
        (PyCFunction)multidict_reduce,
        METH_NOARGS,
        NULL,
    },
    {
        "__class_getitem__",
        (PyCFunction)multidict_class_getitem,
        METH_O | METH_CLASS,
        NULL
    },
    {
        "__sizeof__",
        (PyCFunction)_multidict_sizeof,
        METH_NOARGS,
        sizeof__doc__,
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};


PyDoc_STRVAR(MultDict_doc,
"Dictionary with the support for duplicate keys.");


static PyTypeObject multidict_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "multidict._multidict.MultiDict",                /* tp_name */
    sizeof(MultiDictObject),                         /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_tp_dealloc,
    .tp_repr = (reprfunc)multidict_repr,
    .tp_as_sequence = &multidict_sequence,
    .tp_as_mapping = &multidict_mapping,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_doc = MultDict_doc,
    .tp_traverse = (traverseproc)multidict_tp_traverse,
    .tp_clear = (inquiry)multidict_tp_clear,
    .tp_richcompare = (richcmpfunc)multidict_tp_richcompare,
    .tp_weaklistoffset = offsetof(MultiDictObject, weaklist),
    .tp_iter = (getiterfunc)multidict_tp_iter,
    .tp_methods = multidict_methods,
    .tp_init = (initproc)multidict_tp_init,
    .tp_alloc = PyType_GenericAlloc,
    .tp_new = PyType_GenericNew,
    .tp_free = PyObject_GC_Del,
};

/******************** CIMultiDict ********************/

static inline int
cimultidict_tp_init(MultiDictObject *self, PyObject *args, PyObject *kwds)
{
    if (ci_pair_list_init(&self->pairs) < 0) {
        return -1;
    }
    if (_multidict_extend(self, args, kwds, "CIMultiDict", 1) < 0) {
        return -1;
    }
    return 0;
}

static inline PyObject *
cimultidict_copy(MultiDictObject *self)
{
    return _multidict_copy(self, &cimultidict_type);
}

PyDoc_STRVAR(cimultidict_copy_doc,
"Return a copy of itself.");

static PyMethodDef cimultidict_methods[] = {
    {
        "copy",
        (PyCFunction)cimultidict_copy,
        METH_NOARGS,
        cimultidict_copy_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

PyDoc_STRVAR(CIMultDict_doc,
"Dictionary with the support for duplicate case-insensitive keys.");


static PyTypeObject cimultidict_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "multidict._multidict.CIMultiDict",              /* tp_name */
    sizeof(MultiDictObject),                         /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_doc = CIMultDict_doc,
    .tp_traverse = (traverseproc)multidict_tp_traverse,
    .tp_clear = (inquiry)multidict_tp_clear,
    .tp_weaklistoffset = offsetof(MultiDictObject, weaklist),
    .tp_methods = cimultidict_methods,
    .tp_base = &multidict_type,
    .tp_init = (initproc)cimultidict_tp_init,
    .tp_alloc = PyType_GenericAlloc,
    .tp_new = PyType_GenericNew,
    .tp_free = PyObject_GC_Del,
};

/******************** MultiDictProxy ********************/

static inline int
multidict_proxy_tp_init(MultiDictProxyObject *self, PyObject *args,
                        PyObject *kwds)
{
    PyObject        *arg = NULL;
    MultiDictObject *md  = NULL;

    if (!PyArg_UnpackTuple(args, "multidict._multidict.MultiDictProxy",
                           0, 1, &arg))
    {
        return -1;
    }
    if (arg == NULL) {
        PyErr_Format(
            PyExc_TypeError,
            "__init__() missing 1 required positional argument: 'arg'"
        );
        return -1;
    }
    if (!MultiDictProxy_CheckExact(arg) &&
        !CIMultiDict_CheckExact(arg) &&
        !MultiDict_CheckExact(arg))
    {
        PyErr_Format(
            PyExc_TypeError,
            "ctor requires MultiDict or MultiDictProxy instance, "
            "not <classs '%s'>",
            Py_TYPE(arg)->tp_name
        );
        return -1;
    }

    md = (MultiDictObject*)arg;
    if (MultiDictProxy_CheckExact(arg)) {
        md = ((MultiDictProxyObject*)arg)->md;
    }
    Py_INCREF(md);
    self->md = md;

    return 0;
}

static inline PyObject *
multidict_proxy_getall(MultiDictProxyObject *self, PyObject *args,
                       PyObject *kwds)
{
    return multidict_getall(self->md, args, kwds);
}

static inline PyObject *
multidict_proxy_getone(MultiDictProxyObject *self, PyObject *args,
                       PyObject *kwds)
{
    return multidict_getone(self->md, args, kwds);
}

static inline PyObject *
multidict_proxy_get(MultiDictProxyObject *self, PyObject *args,
                       PyObject *kwds)
{
    return multidict_get(self->md, args, kwds);
}

static inline PyObject *
multidict_proxy_keys(MultiDictProxyObject *self)
{
    return multidict_keys(self->md);
}

static inline PyObject *
multidict_proxy_items(MultiDictProxyObject *self)
{
    return multidict_items(self->md);
}

static inline PyObject *
multidict_proxy_values(MultiDictProxyObject *self)
{
    return multidict_values(self->md);
}

static inline PyObject *
multidict_proxy_copy(MultiDictProxyObject *self)
{
    return _multidict_proxy_copy(self, &multidict_type);
}

static inline PyObject *
multidict_proxy_reduce(MultiDictProxyObject *self)
{
    PyErr_Format(
        PyExc_TypeError,
        "can't pickle %s objects", Py_TYPE(self)->tp_name
    );

    return NULL;
}

static inline Py_ssize_t
multidict_proxy_mp_len(MultiDictProxyObject *self)
{
    return multidict_mp_len(self->md);
}

static inline PyObject *
multidict_proxy_mp_subscript(MultiDictProxyObject *self, PyObject *key)
{
    return multidict_mp_subscript(self->md, key);
}

static inline int
multidict_proxy_sq_contains(MultiDictProxyObject *self, PyObject *key)
{
    return multidict_sq_contains(self->md, key);
}

static inline PyObject *
multidict_proxy_tp_iter(MultiDictProxyObject *self)
{
    return multidict_tp_iter(self->md);
}

static inline PyObject *
multidict_proxy_tp_richcompare(MultiDictProxyObject *self, PyObject *other,
                               int op)
{
    return multidict_tp_richcompare((PyObject*)self->md, other, op);
}

static inline void
multidict_proxy_tp_dealloc(MultiDictProxyObject *self)
{
    PyObject_GC_UnTrack(self);
    if (self->weaklist != NULL) {
        PyObject_ClearWeakRefs((PyObject *)self);
    };
    Py_XDECREF(self->md);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static inline int
multidict_proxy_tp_traverse(MultiDictProxyObject *self, visitproc visit,
                            void *arg)
{
    Py_VISIT(self->md);
    return 0;
}

static inline int
multidict_proxy_tp_clear(MultiDictProxyObject *self)
{
    Py_CLEAR(self->md);
    return 0;
}

static PySequenceMethods multidict_proxy_sequence = {
    .sq_contains = (objobjproc)multidict_proxy_sq_contains,
};

static PyMappingMethods multidict_proxy_mapping = {
    .mp_length = (lenfunc)multidict_proxy_mp_len,
    .mp_subscript = (binaryfunc)multidict_proxy_mp_subscript,
};

static PyMethodDef multidict_proxy_methods[] = {
    {
        "getall",
        (PyCFunction)multidict_proxy_getall,
        METH_VARARGS | METH_KEYWORDS,
        multidict_getall_doc
    },
    {
        "getone",
        (PyCFunction)multidict_proxy_getone,
        METH_VARARGS | METH_KEYWORDS,
        multidict_getone_doc
    },
    {
        "get",
        (PyCFunction)multidict_proxy_get,
        METH_VARARGS | METH_KEYWORDS,
        multidict_get_doc
    },
    {
        "keys",
        (PyCFunction)multidict_proxy_keys,
        METH_NOARGS,
        multidict_keys_doc
    },
    {
        "items",
        (PyCFunction)multidict_proxy_items,
        METH_NOARGS,
        multidict_items_doc
    },
    {
        "values",
        (PyCFunction)multidict_proxy_values,
        METH_NOARGS,
        multidict_values_doc
    },
    {
        "copy",
        (PyCFunction)multidict_proxy_copy,
        METH_NOARGS,
        multidict_copy_doc
    },
    {
        "__reduce__",
        (PyCFunction)multidict_proxy_reduce,
        METH_NOARGS,
        NULL
    },
    {
        "__class_getitem__",
        (PyCFunction)multidict_class_getitem,
        METH_O | METH_CLASS,
        NULL
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};


PyDoc_STRVAR(MultDictProxy_doc,
"Read-only proxy for MultiDict instance.");


static PyTypeObject multidict_proxy_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "multidict._multidict.MultiDictProxy",           /* tp_name */
    sizeof(MultiDictProxyObject),                    /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_proxy_tp_dealloc,
    .tp_repr = (reprfunc)multidict_repr,
    .tp_as_sequence = &multidict_proxy_sequence,
    .tp_as_mapping = &multidict_proxy_mapping,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_doc = MultDictProxy_doc,
    .tp_traverse = (traverseproc)multidict_proxy_tp_traverse,
    .tp_clear = (inquiry)multidict_proxy_tp_clear,
    .tp_richcompare = (richcmpfunc)multidict_proxy_tp_richcompare,
    .tp_weaklistoffset = offsetof(MultiDictProxyObject, weaklist),
    .tp_iter = (getiterfunc)multidict_proxy_tp_iter,
    .tp_methods = multidict_proxy_methods,
    .tp_init = (initproc)multidict_proxy_tp_init,
    .tp_alloc = PyType_GenericAlloc,
    .tp_new = PyType_GenericNew,
    .tp_free = PyObject_GC_Del,
};

/******************** CIMultiDictProxy ********************/

static inline int
cimultidict_proxy_tp_init(MultiDictProxyObject *self, PyObject *args,
                          PyObject *kwds)
{
    PyObject        *arg = NULL;
    MultiDictObject *md  = NULL;

    if (!PyArg_UnpackTuple(args, "multidict._multidict.CIMultiDictProxy",
                           1, 1, &arg))
    {
        return -1;
    }
    if (arg == NULL) {
        PyErr_Format(
            PyExc_TypeError,
            "__init__() missing 1 required positional argument: 'arg'"
        );
        return -1;
    }
    if (!CIMultiDictProxy_CheckExact(arg) && !CIMultiDict_CheckExact(arg)) {
        PyErr_Format(
            PyExc_TypeError,
            "ctor requires CIMultiDict or CIMultiDictProxy instance, "
            "not <class '%s'>",
            Py_TYPE(arg)->tp_name
        );
        return -1;
    }

    md = (MultiDictObject*)arg;
    if (CIMultiDictProxy_CheckExact(arg)) {
        md = ((MultiDictProxyObject*)arg)->md;
    }
    Py_INCREF(md);
    self->md = md;

    return 0;
}

static inline PyObject *
cimultidict_proxy_copy(MultiDictProxyObject *self)
{
    return _multidict_proxy_copy(self, &cimultidict_type);
}


PyDoc_STRVAR(CIMultDictProxy_doc,
"Read-only proxy for CIMultiDict instance.");

PyDoc_STRVAR(cimultidict_proxy_copy_doc,
"Return copy of itself");

static PyMethodDef cimultidict_proxy_methods[] = {
    {
        "copy",
        (PyCFunction)cimultidict_proxy_copy,
        METH_NOARGS,
        cimultidict_proxy_copy_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

static PyTypeObject cimultidict_proxy_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "multidict._multidict.CIMultiDictProxy",         /* tp_name */
    sizeof(MultiDictProxyObject),                    /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_proxy_tp_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
    .tp_doc = CIMultDictProxy_doc,
    .tp_traverse = (traverseproc)multidict_proxy_tp_traverse,
    .tp_clear = (inquiry)multidict_proxy_tp_clear,
    .tp_richcompare = (richcmpfunc)multidict_proxy_tp_richcompare,
    .tp_weaklistoffset = offsetof(MultiDictProxyObject, weaklist),
    .tp_methods = cimultidict_proxy_methods,
    .tp_base = &multidict_proxy_type,
    .tp_init = (initproc)cimultidict_proxy_tp_init,
    .tp_alloc = PyType_GenericAlloc,
    .tp_new = PyType_GenericNew,
    .tp_free = PyObject_GC_Del,
};

/******************** Other functions ********************/

static inline PyObject *
getversion(PyObject *self, PyObject *md)
{
    pair_list_t *pairs = NULL;
    if (MultiDict_CheckExact(md) || CIMultiDict_CheckExact(md)) {
        pairs = &((MultiDictObject*)md)->pairs;
    } else if (MultiDictProxy_CheckExact(md) || CIMultiDictProxy_CheckExact(md)) {
        pairs = &((MultiDictProxyObject*)md)->md->pairs;
    } else {
        PyErr_Format(PyExc_TypeError, "unexpected type");
        return NULL;
    }
    return PyLong_FromUnsignedLong(pair_list_version(pairs));
}

/******************** Module ********************/

static inline void
module_free(void *m)
{
    Py_CLEAR(collections_abc_mapping);
    Py_CLEAR(collections_abc_mut_mapping);
    Py_CLEAR(collections_abc_mut_multi_mapping);
}

static PyMethodDef multidict_module_methods[] = {
    {
        "getversion",
        (PyCFunction)getversion,
        METH_O
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

static PyModuleDef multidict_module = {
    PyModuleDef_HEAD_INIT,      /* m_base */
    "_multidict",               /* m_name */
    .m_size = -1,
    .m_methods = multidict_module_methods,
    .m_free = (freefunc)module_free,
};

PyMODINIT_FUNC
PyInit__multidict()
{
    PyObject *module = NULL,
             *reg_func_call_result = NULL;

#define WITH_MOD(NAME)                      \
    Py_CLEAR(module);                       \
    module = PyImport_ImportModule(NAME);   \
    if (module == NULL) {                   \
        goto fail;                          \
    }

#define GET_MOD_ATTR(VAR, NAME)                 \
    VAR = PyObject_GetAttrString(module, NAME); \
    if (VAR == NULL) {                          \
        goto fail;                              \
    }

    if (multidict_views_init() < 0) {
        goto fail;
    }

    if (multidict_iter_init() < 0) {
        goto fail;
    }

    if (istr_init() < 0) {
        goto fail;
    }

    if (PyType_Ready(&multidict_type) < 0 ||
        PyType_Ready(&cimultidict_type) < 0 ||
        PyType_Ready(&multidict_proxy_type) < 0 ||
        PyType_Ready(&cimultidict_proxy_type) < 0)
    {
        goto fail;
    }

    WITH_MOD("collections.abc");
    GET_MOD_ATTR(collections_abc_mapping, "Mapping");

    WITH_MOD("multidict._abc");
    GET_MOD_ATTR(collections_abc_mut_mapping, "MultiMapping");

    WITH_MOD("multidict._abc");
    GET_MOD_ATTR(collections_abc_mut_multi_mapping, "MutableMultiMapping");

    WITH_MOD("multidict._multidict_base");
    GET_MOD_ATTR(repr_func, "_mdrepr");

    /* Register in _abc mappings (CI)MultiDict and (CI)MultiDictProxy */
    reg_func_call_result = PyObject_CallMethod(
        collections_abc_mut_mapping,
        "register", "O",
        (PyObject*)&multidict_proxy_type
    );
    if (reg_func_call_result == NULL) {
        goto fail;
    }
    Py_DECREF(reg_func_call_result);

    reg_func_call_result = PyObject_CallMethod(
        collections_abc_mut_mapping,
        "register", "O",
        (PyObject*)&cimultidict_proxy_type
    );
    if (reg_func_call_result == NULL) {
        goto fail;
    }
    Py_DECREF(reg_func_call_result);

    reg_func_call_result = PyObject_CallMethod(
        collections_abc_mut_multi_mapping,
        "register", "O",
        (PyObject*)&multidict_type
    );
    if (reg_func_call_result == NULL) {
        goto fail;
    }
    Py_DECREF(reg_func_call_result);

    reg_func_call_result = PyObject_CallMethod(
        collections_abc_mut_multi_mapping,
        "register", "O",
        (PyObject*)&cimultidict_type
    );
    if (reg_func_call_result == NULL) {
        goto fail;
    }
    Py_DECREF(reg_func_call_result);

    /* Instantiate this module */
    module = PyModule_Create(&multidict_module);

    Py_INCREF(&istr_type);
    if (PyModule_AddObject(
            module, "istr", (PyObject*)&istr_type) < 0)
    {
        goto fail;
    }

    Py_INCREF(&multidict_type);
    if (PyModule_AddObject(
            module, "MultiDict", (PyObject*)&multidict_type) < 0)
    {
        goto fail;
    }

    Py_INCREF(&cimultidict_type);
    if (PyModule_AddObject(
            module, "CIMultiDict", (PyObject*)&cimultidict_type) < 0)
    {
        goto fail;
    }

    Py_INCREF(&multidict_proxy_type);
    if (PyModule_AddObject(
            module, "MultiDictProxy", (PyObject*)&multidict_proxy_type) < 0)
    {
        goto fail;
    }

    Py_INCREF(&cimultidict_proxy_type);
    if (PyModule_AddObject(
            module, "CIMultiDictProxy", (PyObject*)&cimultidict_proxy_type) < 0)
    {
        goto fail;
    }

    return module;

fail:
    Py_XDECREF(collections_abc_mapping);
    Py_XDECREF(collections_abc_mut_mapping);
    Py_XDECREF(collections_abc_mut_multi_mapping);

    return NULL;

#undef WITH_MOD
#undef GET_MOD_ATTR
}
