#include <stdint.h>

void cpuid(uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
#ifdef _MSC_VER
  uint32_t regs[4];
  __cpuidex((int *)regs, *eax, *ecx);
  *eax = regs[0], *ebx = regs[1], *ecx = regs[2], *edx = regs[3];
#else
  asm volatile(
#if defined(__i386__) && defined(__PIC__)
      // The reason for this ebx juggling is the -fPIC rust compilation mode.
      // On 32-bit to locate variables it uses a global offset table whose
      // pointer is stored in ebx. Without temporary storing ebx in edi, the
      // compiler will complain about inconsistent operand constraints in an
      // 'asm'.
      // Also note that this is only an issue on older compiler versions.
      "mov %%ebx, %%edi;"
      "cpuid;"
      "xchgl %%ebx, %%edi;"
      :
      "+a" (*eax),
      "=D" (*ebx),
#else
      "cpuid"
      :
      "+a"(*eax),
      "=b"(*ebx),
#endif
      "+c"(*ecx),
      "=d"(*edx));
#endif
}
