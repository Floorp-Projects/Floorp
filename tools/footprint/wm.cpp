/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This program tracks a process's working memory usage using the
 * ``performance'' entries in the Win32 registry. It borrows from
 * the ``pviewer'' source code in the MS SDK.
 */

#include <assert.h>
#include <windows.h>
#include <winperf.h>
#include <stdio.h>
#include <stdlib.h>

#define PN_PROCESS                          1
#define PN_PROCESS_CPU                      2
#define PN_PROCESS_PRIV                     3
#define PN_PROCESS_USER                     4
#define PN_PROCESS_WORKING_SET              5
#define PN_PROCESS_PEAK_WS                  6
#define PN_PROCESS_PRIO                     7
#define PN_PROCESS_ELAPSE                   8
#define PN_PROCESS_ID                       9
#define PN_PROCESS_PRIVATE_PAGE            10
#define PN_PROCESS_VIRTUAL_SIZE            11
#define PN_PROCESS_PEAK_VS                 12
#define PN_PROCESS_FAULT_COUNT             13
#define PN_THREAD                          14
#define PN_THREAD_CPU                      15
#define PN_THREAD_PRIV                     16
#define PN_THREAD_USER                     17
#define PN_THREAD_START                    18
#define PN_THREAD_SWITCHES                 19
#define PN_THREAD_PRIO                     20
#define PN_THREAD_BASE_PRIO                21
#define PN_THREAD_ELAPSE                   22
#define PN_THREAD_DETAILS                  23
#define PN_THREAD_PC                       24
#define PN_IMAGE                           25
#define PN_IMAGE_NOACCESS                  26
#define PN_IMAGE_READONLY                  27
#define PN_IMAGE_READWRITE                 28
#define PN_IMAGE_WRITECOPY                 29
#define PN_IMAGE_EXECUTABLE                30
#define PN_IMAGE_EXE_READONLY              31
#define PN_IMAGE_EXE_READWRITE             32
#define PN_IMAGE_EXE_WRITECOPY             33
#define PN_PROCESS_ADDRESS_SPACE           34
#define PN_PROCESS_PRIVATE_NOACCESS        35
#define PN_PROCESS_PRIVATE_READONLY        36
#define PN_PROCESS_PRIVATE_READWRITE       37
#define PN_PROCESS_PRIVATE_WRITECOPY       38
#define PN_PROCESS_PRIVATE_EXECUTABLE      39
#define PN_PROCESS_PRIVATE_EXE_READONLY    40
#define PN_PROCESS_PRIVATE_EXE_READWRITE   41
#define PN_PROCESS_PRIVATE_EXE_WRITECOPY   42
#define PN_PROCESS_MAPPED_NOACCESS         43
#define PN_PROCESS_MAPPED_READONLY         44
#define PN_PROCESS_MAPPED_READWRITE        45
#define PN_PROCESS_MAPPED_WRITECOPY        46
#define PN_PROCESS_MAPPED_EXECUTABLE       47
#define PN_PROCESS_MAPPED_EXE_READONLY     48
#define PN_PROCESS_MAPPED_EXE_READWRITE    49
#define PN_PROCESS_MAPPED_EXE_WRITECOPY    50
#define PN_PROCESS_IMAGE_NOACCESS          51
#define PN_PROCESS_IMAGE_READONLY          52
#define PN_PROCESS_IMAGE_READWRITE         53
#define PN_PROCESS_IMAGE_WRITECOPY         54
#define PN_PROCESS_IMAGE_EXECUTABLE        55
#define PN_PROCESS_IMAGE_EXE_READONLY      56
#define PN_PROCESS_IMAGE_EXE_READWRITE     57
#define PN_PROCESS_IMAGE_EXE_WRITECOPY     58

struct entry_t {
    int   e_key;
    int   e_index;
    char* e_title;
};

entry_t entries[] = {
{ PN_PROCESS,                          0, TEXT("Process") },
{ PN_PROCESS_CPU,                      0, TEXT("% Processor Time") },
{ PN_PROCESS_PRIV,                     0, TEXT("% Privileged Time") },
{ PN_PROCESS_USER,                     0, TEXT("% User Time") },
{ PN_PROCESS_WORKING_SET,              0, TEXT("Working Set") },
{ PN_PROCESS_PEAK_WS,                  0, TEXT("Working Set Peak") },
{ PN_PROCESS_PRIO,                     0, TEXT("Priority Base") },
{ PN_PROCESS_ELAPSE,                   0, TEXT("Elapsed Time") },
{ PN_PROCESS_ID,                       0, TEXT("ID Process") },
{ PN_PROCESS_PRIVATE_PAGE,             0, TEXT("Private Bytes") },
{ PN_PROCESS_VIRTUAL_SIZE,             0, TEXT("Virtual Bytes") },
{ PN_PROCESS_PEAK_VS,                  0, TEXT("Virtual Bytes Peak") },
{ PN_PROCESS_FAULT_COUNT,              0, TEXT("Page Faults/sec") },
{ PN_THREAD,                           0, TEXT("Thread") },
{ PN_THREAD_CPU,                       0, TEXT("% Processor Time") },
{ PN_THREAD_PRIV,                      0, TEXT("% Privileged Time") },
{ PN_THREAD_USER,                      0, TEXT("% User Time") },
{ PN_THREAD_START,                     0, TEXT("Start Address") },
{ PN_THREAD_SWITCHES,                  0, TEXT("Con0, TEXT Switches/sec") },
{ PN_THREAD_PRIO,                      0, TEXT("Priority Current") },
{ PN_THREAD_BASE_PRIO,                 0, TEXT("Priority Base") },
{ PN_THREAD_ELAPSE,                    0, TEXT("Elapsed Time") },
{ PN_THREAD_DETAILS,                   0, TEXT("Thread Details") },
{ PN_THREAD_PC,                        0, TEXT("User PC") },
{ PN_IMAGE,                            0, TEXT("Image") },
{ PN_IMAGE_NOACCESS,                   0, TEXT("No Access") },
{ PN_IMAGE_READONLY,                   0, TEXT("Read Only") },
{ PN_IMAGE_READWRITE,                  0, TEXT("Read/Write") },
{ PN_IMAGE_WRITECOPY,                  0, TEXT("Write Copy") },
{ PN_IMAGE_EXECUTABLE,                 0, TEXT("Executable") },
{ PN_IMAGE_EXE_READONLY,               0, TEXT("Exec Read Only") },
{ PN_IMAGE_EXE_READWRITE,              0, TEXT("Exec Read/Write") },
{ PN_IMAGE_EXE_WRITECOPY,              0, TEXT("Exec Write Copy") },
{ PN_PROCESS_ADDRESS_SPACE,            0, TEXT("Process Address Space") },
{ PN_PROCESS_PRIVATE_NOACCESS,         0, TEXT("Reserved Space No Access") },
{ PN_PROCESS_PRIVATE_READONLY,         0, TEXT("Reserved Space Read Only") },
{ PN_PROCESS_PRIVATE_READWRITE,        0, TEXT("Reserved Space Read/Write") },
{ PN_PROCESS_PRIVATE_WRITECOPY,        0, TEXT("Reserved Space Write Copy") },
{ PN_PROCESS_PRIVATE_EXECUTABLE,       0, TEXT("Reserved Space Executable") },
{ PN_PROCESS_PRIVATE_EXE_READONLY,     0, TEXT("Reserved Space Exec Read Only") },
{ PN_PROCESS_PRIVATE_EXE_READWRITE,    0, TEXT("Reserved Space Exec Read/Write") },
{ PN_PROCESS_PRIVATE_EXE_WRITECOPY,    0, TEXT("Reserved Space Exec Write Copy") },
{ PN_PROCESS_MAPPED_NOACCESS,          0, TEXT("Mapped Space No Access") },
{ PN_PROCESS_MAPPED_READONLY,          0, TEXT("Mapped Space Read Only") },
{ PN_PROCESS_MAPPED_READWRITE,         0, TEXT("Mapped Space Read/Write") },
{ PN_PROCESS_MAPPED_WRITECOPY,         0, TEXT("Mapped Space Write Copy") },
{ PN_PROCESS_MAPPED_EXECUTABLE,        0, TEXT("Mapped Space Executable") },
{ PN_PROCESS_MAPPED_EXE_READONLY,      0, TEXT("Mapped Space Exec Read Only") },
{ PN_PROCESS_MAPPED_EXE_READWRITE,     0, TEXT("Mapped Space Exec Read/Write") },
{ PN_PROCESS_MAPPED_EXE_WRITECOPY,     0, TEXT("Mapped Space Exec Write Copy") },
{ PN_PROCESS_IMAGE_NOACCESS,           0, TEXT("Image Space No Access") },
{ PN_PROCESS_IMAGE_READONLY,           0, TEXT("Image Space Read Only") },
{ PN_PROCESS_IMAGE_READWRITE,          0, TEXT("Image Space Read/Write") },
{ PN_PROCESS_IMAGE_WRITECOPY,          0, TEXT("Image Space Write Copy") },
{ PN_PROCESS_IMAGE_EXECUTABLE,         0, TEXT("Image Space Executable") },
{ PN_PROCESS_IMAGE_EXE_READONLY,       0, TEXT("Image Space Exec Read Only") },
{ PN_PROCESS_IMAGE_EXE_READWRITE,      0, TEXT("Image Space Exec Read/Write") },
{ PN_PROCESS_IMAGE_EXE_WRITECOPY,      0, TEXT("Image Space Exec Write Copy") },
{ 0,                                   0, 0 },
};

#define NENTRIES ((sizeof(entries) / sizeof(entry_t)) - 1)

static int
key_for_index(int key)
{
    entry_t* entry = entries + NENTRIES / 2;
    unsigned int step = 64 / 4; // XXX

    while (step) {
        if (key < entry->e_key)
            entry -= step;
        else if (key > entry->e_key)
            entry += step;

        if (key == entry->e_key)
            return entry->e_index;

        step >>= 1;
    }

    assert(false);
    return 0;
}


class auto_hkey {
protected:
    HKEY hkey;

    HKEY* begin_assignment() {
        if (hkey) {
            ::RegCloseKey(hkey);
            hkey = 0;
        }
        return &hkey;
    }

public:
    auto_hkey() : hkey(0) {}
    ~auto_hkey() { ::RegCloseKey(hkey); }
    
    HKEY get() const { return hkey; }
    operator HKEY() const { return get(); }

    friend HKEY*
    getter_Acquires(auto_hkey& hkey);
};

static HKEY*
getter_Acquires(auto_hkey& hkey)
{
    return hkey.begin_assignment();
}


static int
get_perf_titles(char*& buffer, char**& titles, int& last_title_index)
{
    DWORD result;

    // Open the perflib key to find out the last counter's index and
    // system version.
    auto_hkey perflib_hkey;
    result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            TEXT("software\\microsoft\\windows nt\\currentversion\\perflib"),
                            0,
                            KEY_READ,
                            getter_Acquires(perflib_hkey));

    if (result != ERROR_SUCCESS)
        return result;

    // Get the last counter's index so we know how much memory to
    // allocate for titles
    DWORD data_size = sizeof(DWORD);
    DWORD type;
    result = ::RegQueryValueEx(perflib_hkey,
                               TEXT("Last Counter"),
                               0,
                               &type,
                               reinterpret_cast<BYTE*>(&last_title_index),
                               &data_size);

    if (result != ERROR_SUCCESS)
        return result;

    // Find system version, for system earlier than 1.0a, there's no
    // version value.
    int version;
    result = ::RegQueryValueEx(perflib_hkey,
                               TEXT("Version"),
                               0,
                               &type,
                               reinterpret_cast<BYTE*>(&version),
                               &data_size);

    bool is_nt_10 = (result == ERROR_SUCCESS);

    // Now, get ready for the counter names and indexes.
    char* counter_value_name;
    auto_hkey counter_autohkey;
    HKEY counter_hkey;
    if (is_nt_10) {
        // NT 1.0, so make hKey2 point to ...\perflib\009 and get
        //  the counters from value "Counters"
        counter_value_name = TEXT("Counters");
        result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                TEXT("software\\microsoft\\windows nt\\currentversion\\perflib\\009"),
                                0,
                                KEY_READ,
                                getter_Acquires(counter_autohkey));

        if (result != ERROR_SUCCESS)
            return result;

        counter_hkey = counter_autohkey;
    }
    else {
        // NT 1.0a or later.  Get the counters in key HKEY_PERFORMANCE_KEY
        //  and from value "Counter 009"
        counter_value_name = TEXT("Counter 009");
        counter_hkey = HKEY_PERFORMANCE_DATA;
    }

    // Find out the size of the data.
    result = ::RegQueryValueEx(counter_hkey,
                               counter_value_name,
                               0,
                               &type,
                               0,
                               &data_size);

    if (result != ERROR_SUCCESS)
        return result;

    // Allocate memory
    buffer = new char[data_size];
    titles = new char*[last_title_index + 1];
    for (int i = 0; i <= last_title_index; ++i)
        titles[i] = 0;

    // Query the data
    result = ::RegQueryValueEx(counter_hkey,
                               counter_value_name,
                               0,
                               &type,
                               reinterpret_cast<BYTE*>(buffer),
                               &data_size);
    if (result != ERROR_SUCCESS)
        return result;

    // Setup the titles array of pointers to point to beginning of
    // each title string.
    char* title = buffer;
    int len;

    while (len = lstrlen(title)) {
        int index = atoi(title);
        title += len + 1;

        if (index <= last_title_index)
            titles[index] = title;

#ifdef DEBUG
        printf("%d=%s\n", index, title);
#endif

        title += lstrlen(title) + 1;
    }

    return ERROR_SUCCESS;
}

static void
init_entries()
{
    char* buffer;
    char** titles;
    int last = 0;

    DWORD result = get_perf_titles(buffer, titles, last);

    assert(result == ERROR_SUCCESS);

    for (entry_t* entry = entries; entry->e_key != 0; ++entry) {
        for (int index = 0; index <= last; ++index) {
            if (titles[index] && 0 == lstrcmpi(titles[index], entry->e_title)) {
                entry->e_index = index;
                break;
            }
        }

        if (entry->e_index == 0) {
            fprintf(stderr, "warning: unable to find index for ``%s''\n", entry->e_title);
        }
    }

    delete[] buffer;
    delete[] titles;
}



static DWORD
get_perf_data(HKEY perf_hkey, char* object_index, PERF_DATA_BLOCK** data, DWORD* size)
{
    if (! *data)
        *data = reinterpret_cast<PERF_DATA_BLOCK*>(new char[*size]);

    DWORD result;

    while (1) {
        DWORD type;
        DWORD real_size = *size;

        result = ::RegQueryValueEx(perf_hkey,
                                   object_index,
                                   0,
                                   &type,
                                   reinterpret_cast<BYTE*>(*data),
                                   &real_size);

        if (result != ERROR_MORE_DATA)
            break;

        delete[] *data;
        *size += 1024;
        *data = reinterpret_cast<PERF_DATA_BLOCK*>(new char[*size]);

        if (! *data)
            return ERROR_NOT_ENOUGH_MEMORY;
    }

    return result;
}


static const PERF_OBJECT_TYPE*
first_object(const PERF_DATA_BLOCK* data)
{
    return data
        ? reinterpret_cast<const PERF_OBJECT_TYPE*>(reinterpret_cast<const char*>(data) + data->HeaderLength)
        : 0;
}

static const PERF_OBJECT_TYPE*
next_object(const PERF_OBJECT_TYPE* object)
{
    return object
        ? reinterpret_cast<const PERF_OBJECT_TYPE*>(reinterpret_cast<const char*>(object) + object->TotalByteLength)
        : 0;
}

const PERF_OBJECT_TYPE*
find_object(const PERF_DATA_BLOCK* data, DWORD index)
{
    const PERF_OBJECT_TYPE* object = first_object(data);
    if (! object)
        return 0;

    for (int i = 0; i < data->NumObjectTypes; ++i) {
        if (object->ObjectNameTitleIndex == index)
            return object;

        object = next_object(object);
    }

    return 0;
}


static const PERF_COUNTER_DEFINITION*
first_counter(const PERF_OBJECT_TYPE* object)
{
    return object
        ? reinterpret_cast<const PERF_COUNTER_DEFINITION*>(reinterpret_cast<const char*>(object) + object->HeaderLength)
        : 0;
}

static const PERF_COUNTER_DEFINITION*
next_counter(const PERF_COUNTER_DEFINITION* counter)
{
    return counter ?
        reinterpret_cast<const PERF_COUNTER_DEFINITION*>(reinterpret_cast<const char*>(counter) + counter->ByteLength)
        : 0;
}


static const PERF_COUNTER_DEFINITION*
find_counter(const PERF_OBJECT_TYPE* object, int index)
{
    const PERF_COUNTER_DEFINITION* counter =
        first_counter(object);

    if (! counter)
        return 0;

    for (int i; i < object->NumCounters; ++i) {
        if (counter->CounterNameTitleIndex == index)
            return counter;

        counter = next_counter(counter);
    }

    return 0;
}


static const PERF_INSTANCE_DEFINITION*
first_instance(const PERF_OBJECT_TYPE* object)
{
    return object
        ? reinterpret_cast<const PERF_INSTANCE_DEFINITION*>(reinterpret_cast<const char*>(object) + object->DefinitionLength)
        : 0;
}


static const PERF_INSTANCE_DEFINITION*
next_instance(const PERF_INSTANCE_DEFINITION* instance)
{
    if (instance) {
        const PERF_COUNTER_BLOCK* counter_block =
            reinterpret_cast<const PERF_COUNTER_BLOCK*>(reinterpret_cast<const char*>(instance) + instance->ByteLength);

        return reinterpret_cast<const PERF_INSTANCE_DEFINITION*>(reinterpret_cast<const char*>(counter_block) + counter_block->ByteLength);
    }
    else {
        return 0;
    }
}


static const wchar_t*
instance_name(const PERF_INSTANCE_DEFINITION* instance)
{
    return instance
        ? reinterpret_cast<const wchar_t*>(reinterpret_cast<const char*>(instance) + instance->NameOffset)
        : 0;
}


static const void*
counter_data(const PERF_INSTANCE_DEFINITION* instance,
             const PERF_COUNTER_DEFINITION* counter)
{
    if (counter && instance) {
        const PERF_COUNTER_BLOCK* counter_block; 
        counter_block = reinterpret_cast<const PERF_COUNTER_BLOCK*>(reinterpret_cast<const char*>(instance) + instance->ByteLength);
        return reinterpret_cast<const char*>(counter_block) + counter->CounterOffset;
    }
    else {
        return 0;
    }
}


static bool
list_process(PERF_DATA_BLOCK* perf_data, wchar_t* process_name)
{
    const PERF_OBJECT_TYPE* process = find_object(perf_data, key_for_index(PN_PROCESS));
    const PERF_COUNTER_DEFINITION* working_set      = find_counter(process, key_for_index(PN_PROCESS_WORKING_SET));
    const PERF_COUNTER_DEFINITION* peak_working_set = find_counter(process, key_for_index(PN_PROCESS_PEAK_WS));
    const PERF_COUNTER_DEFINITION* private_page     = find_counter(process, key_for_index(PN_PROCESS_PRIVATE_PAGE));
    const PERF_COUNTER_DEFINITION* virtual_size     = find_counter(process, key_for_index(PN_PROCESS_VIRTUAL_SIZE));

    const PERF_INSTANCE_DEFINITION* instance = first_instance(process);
    int index = 0;

    bool found = false;

    while (instance && index < process->NumInstances) {
        const wchar_t* name = instance_name(instance);
        if (lstrcmpW(process_name, name) == 0) {
            printf("%d %d %d %d\n",
                   *(static_cast<const int*>(counter_data(instance, working_set))),
                   *(static_cast<const int*>(counter_data(instance, peak_working_set))),
                   *(static_cast<const int*>(counter_data(instance, private_page))),
                   *(static_cast<const int*>(counter_data(instance, virtual_size))));

            found = true;
        }

        instance = next_instance(instance);
        ++index;
    }

    if (found) {
#if 0
        // Dig up address space data.
        PERF_OBJECT_TYPE* address_space = FindObject(costly_data, PX_PROCESS_ADDRESS_SPACE);
        PERF_COUNTER_DEFINITION* image_executable = FindCounter(process, PX_PROCESS_IMAGE_EXECUTABLE);
        PERF_COUNTER_DEFINITION* image_exe_readonly = FindCounter(process, PX_PROCESS_IMAGE_EXE_READONLY);
        PERF_COUNTER_DEFINITION* image_exe_readwrite = FindCounter(process, PX_PROCESS_IMAGE_EXE_READWRITE);
        PERF_COUNTER_DEFINITION* image_exe_writecopy = FindCounter(process, PX_PROCESS_IMAGE_EXE_WRITECOPY);
#endif
    }

    return found;
}


int
main(int argc, char* argv[])
{
    wchar_t process_name[32];

    int interval = 10000; // msec

    int i = 0;
    while (++i < argc) {
        if (argv[i][0] != '-')
            break;

        switch (argv[i][1]) {
        case 'i':
            interval = atoi(argv[++i]) * 1000;
            break;
            
        default:
            fprintf(stderr, "unknown option `%c'\n", argv[i][1]);
            exit(1);
        }
    }

    if (argv[i]) {
        char* p = argv[i];
        wchar_t* q = process_name;
        while (*q++ = wchar_t(*p++))
            continue;
    }
    else {
        fprintf(stderr, "no image name specified\n");
        exit(1);
    }

    init_entries();

    PERF_DATA_BLOCK* perf_data = 0;
    PERF_DATA_BLOCK* costly_data = 0;
    DWORD perf_data_size = 50 * 1024;
    DWORD costly_data_size = 100 * 1024;

    do {
        char buf[64];
        sprintf(buf, "%ld %ld",
                key_for_index(PN_PROCESS),
                key_for_index(PN_THREAD));

        get_perf_data(HKEY_PERFORMANCE_DATA, buf, &perf_data, &perf_data_size);

#if 0
        sprintf(buf, "%ld %ld %ld",
                key_for_index(PN_PROCESS_ADDRESS_SPACE),
                key_for_index(PN_IMAGE),
                key_for_index(PN_THREAD_DETAILS));

        get_perf_data(HKEY_PERFORMANCE_DATA, buf, &costly_data, &costly_data_size);
#endif

        if (! list_process(perf_data, process_name))
            break;

        _sleep(interval);
    } while (1);

    return 0;
}

