//   bool WriteCPUInformation(MDRawSystemInfo* sys_info) {
//     // The CPUID value is broken up in several entries in /proc/cpuinfo.
//     // This table is used to rebuild it from the entries.
//     const struct CpuIdEntry {
//       const char* field;
//       char        format;
//       char        bit_lshift;
//       char        bit_length;
//     } cpu_id_entries[] = {
//       { "CPU implementer", 'x', 24, 8 },
//       { "CPU variant", 'x', 20, 4 },
//       { "CPU part", 'x', 4, 12 },
//       { "CPU revision", 'd', 0, 4 },
//     };

//     // The ELF hwcaps are listed in the "Features" entry as textual tags.
//     // This table is used to rebuild them.
//     const struct CpuFeaturesEntry {
//       const char* tag;
//       uint32_t hwcaps;
//     } cpu_features_entries[] = {
// #if defined(__arm__)
//       { "swp",  MD_CPU_ARM_ELF_HWCAP_SWP },
//       { "half", MD_CPU_ARM_ELF_HWCAP_HALF },
//       { "thumb", MD_CPU_ARM_ELF_HWCAP_THUMB },
//       { "26bit", MD_CPU_ARM_ELF_HWCAP_26BIT },
//       { "fastmult", MD_CPU_ARM_ELF_HWCAP_FAST_MULT },
//       { "fpa", MD_CPU_ARM_ELF_HWCAP_FPA },
//       { "vfp", MD_CPU_ARM_ELF_HWCAP_VFP },
//       { "edsp", MD_CPU_ARM_ELF_HWCAP_EDSP },
//       { "java", MD_CPU_ARM_ELF_HWCAP_JAVA },
//       { "iwmmxt", MD_CPU_ARM_ELF_HWCAP_IWMMXT },
//       { "crunch", MD_CPU_ARM_ELF_HWCAP_CRUNCH },
//       { "thumbee", MD_CPU_ARM_ELF_HWCAP_THUMBEE },
//       { "neon", MD_CPU_ARM_ELF_HWCAP_NEON },
//       { "vfpv3", MD_CPU_ARM_ELF_HWCAP_VFPv3 },
//       { "vfpv3d16", MD_CPU_ARM_ELF_HWCAP_VFPv3D16 },
//       { "tls", MD_CPU_ARM_ELF_HWCAP_TLS },
//       { "vfpv4", MD_CPU_ARM_ELF_HWCAP_VFPv4 },
//       { "idiva", MD_CPU_ARM_ELF_HWCAP_IDIVA },
//       { "idivt", MD_CPU_ARM_ELF_HWCAP_IDIVT },
//       { "idiv", MD_CPU_ARM_ELF_HWCAP_IDIVA | MD_CPU_ARM_ELF_HWCAP_IDIVT },
// #elif defined(__aarch64__)
//       // No hwcaps on aarch64.
// #endif
//     };

//     // processor_architecture should always be set, do this first
//     sys_info->processor_architecture =
// #if defined(__aarch64__)
//         MD_CPU_ARCHITECTURE_ARM64_OLD;
// #else
//         MD_CPU_ARCHITECTURE_ARM;
// #endif

//     // /proc/cpuinfo is not readable under various sandboxed environments
//     // (e.g. Android services with the android:isolatedProcess attribute)
//     // prepare for this by setting default values now, which will be
//     // returned when this happens.
//     //
//     // Note: Bogus values are used to distinguish between failures (to
//     //       read /sys and /proc files) and really badly configured kernels.
//     sys_info->number_of_processors = 0;
//     sys_info->processor_level = 1U;  // There is no ARMv1
//     sys_info->processor_revision = 42;
//     sys_info->cpu.arm_cpu_info.cpuid = 0;
//     sys_info->cpu.arm_cpu_info.elf_hwcaps = 0;

//     // Counting the number of CPUs involves parsing two sysfs files,
//     // because the content of /proc/cpuinfo will only mirror the number
//     // of 'online' cores, and thus will vary with time.
//     // See http://www.kernel.org/doc/Documentation/cputopology.txt
//     {
//       CpuSet cpus_present;
//       CpuSet cpus_possible;

//       int fd = sys_open("/sys/devices/system/cpu/present", O_RDONLY, 0);
//       if (fd >= 0) {
//         cpus_present.ParseSysFile(fd);
//         sys_close(fd);

//         fd = sys_open("/sys/devices/system/cpu/possible", O_RDONLY, 0);
//         if (fd >= 0) {
//           cpus_possible.ParseSysFile(fd);
//           sys_close(fd);

//           cpus_present.IntersectWith(cpus_possible);
//           int cpu_count = cpus_present.GetCount();
//           if (cpu_count > 255)
//             cpu_count = 255;
//           sys_info->number_of_processors = static_cast<uint8_t>(cpu_count);
//         }
//       }
//     }

//     // Parse /proc/cpuinfo to reconstruct the CPUID value, as well
//     // as the ELF hwcaps field. For the latter, it would be easier to
//     // read /proc/self/auxv but unfortunately, this file is not always
//     // readable from regular Android applications on later versions
//     // (>= 4.1) of the Android platform.
//     const int fd = sys_open("/proc/cpuinfo", O_RDONLY, 0);
//     if (fd < 0) {
//       // Do not return false here to allow the minidump generation
//       // to happen properly.
//       return true;
//     }

//     {
//       PageAllocator allocator;
//       ProcCpuInfoReader* const reader =
//           new(allocator) ProcCpuInfoReader(fd);
//       const char* field;
//       while (reader->GetNextField(&field)) {
//         for (const CpuIdEntry& entry : cpu_id_entries) {
//           if (my_strcmp(entry.field, field) != 0)
//             continue;
//           uintptr_t result = 0;
//           const char* value = reader->GetValue();
//           const char* p = value;
//           if (value[0] == '0' && value[1] == 'x') {
//             p = my_read_hex_ptr(&result, value+2);
//           } else if (entry.format == 'x') {
//             p = my_read_hex_ptr(&result, value);
//           } else {
//             p = my_read_decimal_ptr(&result, value);
//           }
//           if (p == value)
//             continue;

//           result &= (1U << entry.bit_length)-1;
//           result <<= entry.bit_lshift;
//           sys_info->cpu.arm_cpu_info.cpuid |=
//               static_cast<uint32_t>(result);
//         }
// #if defined(__arm__)
//         // Get the architecture version from the "Processor" field.
//         // Note that it is also available in the "CPU architecture" field,
//         // however, some existing kernels are misconfigured and will report
//         // invalid values here (e.g. 6, while the CPU is ARMv7-A based).
//         // The "Processor" field doesn't have this issue.
//         if (!my_strcmp(field, "Processor")) {
//           size_t value_len;
//           const char* value = reader->GetValueAndLen(&value_len);
//           // Expected format: <text> (v<level><endian>)
//           // Where <text> is some text like "ARMv7 Processor rev 2"
//           // and <level> is a decimal corresponding to the ARM
//           // architecture number. <endian> is either 'l' or 'b'
//           // and corresponds to the endianess, it is ignored here.
//           while (value_len > 0 && my_isspace(value[value_len-1]))
//             value_len--;

//           size_t nn = value_len;
//           while (nn > 0 && value[nn-1] != '(')
//             nn--;
//           if (nn > 0 && value[nn] == 'v') {
//             uintptr_t arch_level = 5;
//             my_read_decimal_ptr(&arch_level, value + nn + 1);
//             sys_info->processor_level = static_cast<uint16_t>(arch_level);
//           }
//         }
// #elif defined(__aarch64__)
//         // The aarch64 architecture does not provide the architecture level
//         // in the Processor field, so we instead check the "CPU architecture"
//         // field.
//         if (!my_strcmp(field, "CPU architecture")) {
//           uintptr_t arch_level = 0;
//           const char* value = reader->GetValue();
//           const char* p = value;
//           p = my_read_decimal_ptr(&arch_level, value);
//           if (p == value)
//             continue;
//           sys_info->processor_level = static_cast<uint16_t>(arch_level);
//         }
// #endif
//         // Rebuild the ELF hwcaps from the 'Features' field.
//         if (!my_strcmp(field, "Features")) {
//           size_t value_len;
//           const char* value = reader->GetValueAndLen(&value_len);

//           // Parse each space-separated tag.
//           while (value_len > 0) {
//             const char* tag = value;
//             size_t tag_len = value_len;
//             const char* p = my_strchr(tag, ' ');
//             if (p) {
//               tag_len = static_cast<size_t>(p - tag);
//               value += tag_len + 1;
//               value_len -= tag_len + 1;
//             } else {
//               tag_len = strlen(tag);
//               value_len = 0;
//             }
//             for (const CpuFeaturesEntry& entry : cpu_features_entries) {
//               if (tag_len == strlen(entry.tag) &&
//                   !memcmp(tag, entry.tag, tag_len)) {
//                 sys_info->cpu.arm_cpu_info.elf_hwcaps |= entry.hwcaps;
//                 break;
//               }
//             }
//           }
//         }
//       }
//       sys_close(fd);
//     }

//     return true;
//   }
