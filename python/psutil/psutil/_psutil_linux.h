/*
 * $Id: _psutil_linux.h 1498 2012-07-24 21:41:28Z g.rodola $
 *
 * Copyright (c) 2009, Jay Loden, Giampaolo Rodola'. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * LINUX specific module methods for _psutil_linux
 */

#include <Python.h>

static PyObject* linux_ioprio_get(PyObject* self, PyObject* args);
static PyObject* linux_ioprio_set(PyObject* self, PyObject* args);
static PyObject* get_disk_partitions(PyObject* self, PyObject* args);
static PyObject* get_sysinfo(PyObject* self, PyObject* args);
static PyObject* get_process_cpu_affinity(PyObject* self, PyObject* args);
static PyObject* set_process_cpu_affinity(PyObject* self, PyObject* args);
static PyObject* get_system_users(PyObject* self, PyObject* args);
