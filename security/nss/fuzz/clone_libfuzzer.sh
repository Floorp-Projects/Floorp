#!/bin/sh

d=$(dirname $0)
$d/git-copy.sh https://chromium.googlesource.com/chromium/llvm-project/llvm/lib/Fuzzer 1b543d6e5073b56be214394890c9193979a3d7e1 $d/libFuzzer

cat <<EOF | patch -p0 -d $d
diff --git libFuzzer/FuzzerMutate.cpp libFuzzer/FuzzerMutate.cpp
--- libFuzzer/FuzzerMutate.cpp
+++ libFuzzer/FuzzerMutate.cpp
@@ -53,10 +53,9 @@
     DefaultMutators.push_back(
         {&MutationDispatcher::Mutate_AddWordFromTORC, "CMP"});

+  Mutators = DefaultMutators;
   if (EF->LLVMFuzzerCustomMutator)
     Mutators.push_back({&MutationDispatcher::Mutate_Custom, "Custom"});
-  else
-    Mutators = DefaultMutators;

   if (EF->LLVMFuzzerCustomCrossOver)
     Mutators.push_back(
EOF
