#if defined(_WIN32)
    // Remove warnings for strcat, strcpy as they are safely used here
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wasm-rt.h"

#if defined(_WIN32)
    // Ensure the min/max macro in the header doesn't collide with functions in std::
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <Windows.h>
    #define LINETERM "\r\n"
#else
    #include <dlfcn.h>
    #define LINETERM "\n"
#endif

void* open_lib(char const * wasm2c_module_path) {
    #if defined(_WIN32)
        void* library = LoadLibraryA(wasm2c_module_path);
    #else
        void* library = dlopen(wasm2c_module_path, RTLD_LAZY);
    #endif

    if (!library) {
      #if defined(_WIN32)
        DWORD errorMessageID  = GetLastError();
        if (errorMessageID != 0) {
          LPSTR messageBuffer = 0;
          //The api creates the buffer that holds the message
          size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

          const size_t str_buff_size = size + 1;
          char* error_msg = malloc(str_buff_size);
          memcpy(error_msg, messageBuffer, str_buff_size - 1);
          error_msg[str_buff_size - 1] = '\0';
          printf("Could not load wasm2c dynamic library: %s" LINETERM, error_msg);
          free(error_msg);

          LocalFree(messageBuffer);
        }
      #else
        const char* error_msg = dlerror();
        printf("Could not load wasm2c dynamic library: %s" LINETERM, error_msg);
      #endif

      exit(1);
    }

    return library;
}

void close_lib(void* library)
{
    #if defined(_WIN32)
      FreeLibrary((HMODULE) library);
    #else
      dlclose(library);
    #endif
}

void* symbol_lookup(void* library, const char * name) {
#if defined(_WIN32)
    void* ret = GetProcAddress((HMODULE) library, name);
#else
    void* ret = dlsym(library, name);
#endif
    if (!ret) {
        printf("Could not find symbol %s in wasm2c shared library" LINETERM, name);
    }
    return ret;
}

char* get_info_func_name(char const * wasm_module_name) {
    const char* info_func_suffix = "get_wasm2c_sandbox_info";
    char* info_func_str = malloc(strlen(wasm_module_name) + strlen(info_func_suffix) + 1);

    strcpy(info_func_str, wasm_module_name);
    strcat(info_func_str, info_func_suffix);
    return info_func_str;
}

typedef wasm2c_sandbox_funcs_t(*get_info_func_t)();
typedef void (*wasm2c_start_func_t)(void* sbx);

int main(int argc, char const *argv[])
{
    if (argc < 2) {
        printf("Expected argument: %s <path_to_shared_library> [optional_module_prefix]" LINETERM, argv[0]);
        exit(1);
    }

    char const * wasm2c_module_path = argv[1];
    char const * wasm_module_name = "";

    if (argc >= 3) {
        wasm_module_name = argv[2];
    }

    void* library = open_lib(wasm2c_module_path);
    char* info_func_name = get_info_func_name(wasm_module_name);

    get_info_func_t get_info_func = (get_info_func_t) symbol_lookup(library, info_func_name);
    wasm2c_sandbox_funcs_t sandbox_info = get_info_func();

    void* sandbox = sandbox_info.create_wasm2c_sandbox();
    if (!sandbox) {
        printf("Error: Could not create sandbox" LINETERM);
        exit(1);
    }

    wasm2c_start_func_t start_func = (wasm2c_start_func_t) symbol_lookup(library, "w2c__start");
    start_func(sandbox);

    free(info_func_name);
    close_lib(library);
    return 0;
}
