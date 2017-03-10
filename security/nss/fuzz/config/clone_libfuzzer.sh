#!/bin/sh

d=$(dirname $0)
$d/git-copy.sh https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer b96a41ac6bbc3824fc7c7977662bebacac8f0983 $d/../libFuzzer

# [https://llvm.org/bugs/show_bug.cgi?id=31318]
# This prevents a known buffer overrun that won't be fixed as the affected code
# will go away in the near future. Until that is we have to patch it as we seem
# to constantly run into it.
cat <<EOF | patch -p0 -d $d/..
diff --git libFuzzer/FuzzerLoop.cpp libFuzzer/FuzzerLoop.cpp
--- libFuzzer/FuzzerLoop.cpp
+++ libFuzzer/FuzzerLoop.cpp
@@ -476,6 +476,9 @@
   uint8_t dummy;
   ExecuteCallback(&dummy, 0);

+  // Number of counters might have changed.
+  PrepareCounters(&MaxCoverage);
+
   for (const auto &U : *InitialCorpus) {
     if (size_t NumFeatures = RunOne(U)) {
       CheckExitOnSrcPosOrItem();
EOF

# Latest Libfuzzer uses __sanitizer_dump_coverage(), a symbol to be introduced
# with LLVM 4.0. To keep our code working with LLVM 3.x to simplify development
# of fuzzers we'll just provide it ourselves.
cat <<EOF | patch -p0 -d $d/..
diff --git libFuzzer/FuzzerTracePC.cpp libFuzzer/FuzzerTracePC.cpp
--- libFuzzer/FuzzerTracePC.cpp
+++ libFuzzer/FuzzerTracePC.cpp
@@ -33,6 +33,12 @@
 ATTRIBUTE_INTERFACE
 uintptr_t __sancov_trace_pc_pcs[fuzzer::TracePC::kNumPCs];

+#if defined(__clang_major__) && (__clang_major__ == 3)
+void __sanitizer_dump_coverage(const uintptr_t *pcs, uintptr_t len) {
+  // SanCov in LLVM 4.x will provide this symbol. Make 3.x work.
+}
+#endif
+
 namespace fuzzer {

 TracePC TPC;
EOF
