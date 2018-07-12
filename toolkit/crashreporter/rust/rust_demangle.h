#ifndef __RUST_DEMANGLE_H__
#define __RUST_DEMANGLE_H__

#ifdef __cplusplus
extern "C" {
#endif

extern char *rust_demangle(const char *);
extern void free_rust_demangled_name(char *);

#ifdef __cplusplus
}
#endif

#endif /* __RUST_DEMANGLE_H__ */
