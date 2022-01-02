#ifndef _MULTIDICT_ITER_H
#define _MULTIDICT_ITER_H

#ifdef __cplusplus
extern "C" {
#endif

static PyTypeObject multidict_items_iter_type;
static PyTypeObject multidict_values_iter_type;
static PyTypeObject multidict_keys_iter_type;

typedef struct multidict_iter {
    PyObject_HEAD
    MultiDictObject *md;  // MultiDict or CIMultiDict
    Py_ssize_t current;
    uint64_t version;
} MultidictIter;

static inline void
_init_iter(MultidictIter *it, MultiDictObject *md)
{
    Py_INCREF(md);

    it->md = md;
    it->current = 0;
    it->version = pair_list_version(&md->pairs);
}

static inline PyObject *
multidict_items_iter_new(MultiDictObject *md)
{
    MultidictIter *it = PyObject_GC_New(
        MultidictIter, &multidict_items_iter_type);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md);

    PyObject_GC_Track(it);
    return (PyObject *)it;
}

static inline PyObject *
multidict_keys_iter_new(MultiDictObject *md)
{
    MultidictIter *it = PyObject_GC_New(
        MultidictIter, &multidict_keys_iter_type);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md);

    PyObject_GC_Track(it);
    return (PyObject *)it;
}

static inline PyObject *
multidict_values_iter_new(MultiDictObject *md)
{
    MultidictIter *it = PyObject_GC_New(
        MultidictIter, &multidict_values_iter_type);
    if (it == NULL) {
        return NULL;
    }

    _init_iter(it, md);

    PyObject_GC_Track(it);
    return (PyObject *)it;
}

static inline PyObject *
multidict_items_iter_iternext(MultidictIter *self)
{
    PyObject *key = NULL;
    PyObject *value = NULL;
    PyObject *ret = NULL;

    if (self->version != pair_list_version(&self->md->pairs)) {
        PyErr_SetString(PyExc_RuntimeError, "Dictionary changed during iteration");
        return NULL;
    }

    if (!_pair_list_next(&self->md->pairs, &self->current, NULL, &key, &value, NULL)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    ret = PyTuple_Pack(2, key, value);
    if (ret == NULL) {
        return NULL;
    }

    return ret;
}

static inline PyObject *
multidict_values_iter_iternext(MultidictIter *self)
{
    PyObject *value = NULL;

    if (self->version != pair_list_version(&self->md->pairs)) {
        PyErr_SetString(PyExc_RuntimeError, "Dictionary changed during iteration");
        return NULL;
    }

    if (!pair_list_next(&self->md->pairs, &self->current, NULL, NULL, &value)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    Py_INCREF(value);

    return value;
}

static inline PyObject *
multidict_keys_iter_iternext(MultidictIter *self)
{
    PyObject *key = NULL;

    if (self->version != pair_list_version(&self->md->pairs)) {
        PyErr_SetString(PyExc_RuntimeError, "Dictionary changed during iteration");
        return NULL;
    }

    if (!pair_list_next(&self->md->pairs, &self->current, NULL, &key, NULL)) {
        PyErr_SetNone(PyExc_StopIteration);
        return NULL;
    }

    Py_INCREF(key);

    return key;
}

static inline void
multidict_iter_dealloc(MultidictIter *self)
{
    PyObject_GC_UnTrack(self);
    Py_XDECREF(self->md);
    PyObject_GC_Del(self);
}

static inline int
multidict_iter_traverse(MultidictIter *self, visitproc visit, void *arg)
{
    Py_VISIT(self->md);
    return 0;
}

static inline int
multidict_iter_clear(MultidictIter *self)
{
    Py_CLEAR(self->md);
    return 0;
}

static inline PyObject *
multidict_iter_len(MultidictIter *self)
{
    return PyLong_FromLong(pair_list_len(&self->md->pairs));
}

PyDoc_STRVAR(length_hint_doc,
             "Private method returning an estimate of len(list(it)).");

static PyMethodDef multidict_iter_methods[] = {
    {
        "__length_hint__",
        (PyCFunction)(void(*)(void))multidict_iter_len,
        METH_NOARGS,
        length_hint_doc
    },
    {
        NULL,
        NULL
    }   /* sentinel */
};

/***********************************************************************/

static PyTypeObject multidict_items_iter_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict._itemsiter",         /* tp_name */
    sizeof(MultidictIter),                     /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_iter_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)multidict_iter_traverse,
    .tp_clear = (inquiry)multidict_iter_clear,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc)multidict_items_iter_iternext,
    .tp_methods = multidict_iter_methods,
};

static PyTypeObject multidict_values_iter_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict._valuesiter",         /* tp_name */
    sizeof(MultidictIter),                      /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_iter_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)multidict_iter_traverse,
    .tp_clear = (inquiry)multidict_iter_clear,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc)multidict_values_iter_iternext,
    .tp_methods = multidict_iter_methods,
};

static PyTypeObject multidict_keys_iter_type = {
    PyVarObject_HEAD_INIT(DEFERRED_ADDRESS(&PyType_Type), 0)
    "multidict._multidict._keysiter",         /* tp_name */
    sizeof(MultidictIter),                    /* tp_basicsize */
    .tp_dealloc = (destructor)multidict_iter_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_traverse = (traverseproc)multidict_iter_traverse,
    .tp_clear = (inquiry)multidict_iter_clear,
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = (iternextfunc)multidict_keys_iter_iternext,
    .tp_methods = multidict_iter_methods,
};

static inline int
multidict_iter_init()
{
    if (PyType_Ready(&multidict_items_iter_type) < 0 ||
        PyType_Ready(&multidict_values_iter_type) < 0 ||
        PyType_Ready(&multidict_keys_iter_type) < 0) {
        return -1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
