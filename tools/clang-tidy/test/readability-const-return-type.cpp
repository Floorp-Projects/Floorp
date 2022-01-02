const int p1() {
// CHECK-MESSAGES: [[@LINE-1]]:1: warning: return type 'const int' is 'const'-qualified at the top level, which may reduce code readability without improving const correctness
// CHECK-FIXES: int p1() {
  return 1;
}
