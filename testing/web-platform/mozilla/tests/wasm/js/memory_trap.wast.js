
// memory_trap.wast:1
let $1 = instance("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x8f\x80\x80\x80\x00\x03\x60\x00\x01\x7f\x60\x02\x7f\x7f\x00\x60\x01\x7f\x01\x7f\x03\x85\x80\x80\x80\x00\x04\x00\x01\x02\x02\x05\x83\x80\x80\x80\x00\x01\x00\x01\x07\x9e\x80\x80\x80\x00\x03\x05\x73\x74\x6f\x72\x65\x00\x01\x04\x6c\x6f\x61\x64\x00\x02\x0b\x67\x72\x6f\x77\x5f\x6d\x65\x6d\x6f\x72\x79\x00\x03\x0a\xba\x80\x80\x80\x00\x04\x89\x80\x80\x80\x00\x00\x3f\x00\x41\x80\x80\x04\x6c\x0b\x8c\x80\x80\x80\x00\x00\x10\x00\x20\x00\x6a\x20\x01\x36\x02\x00\x0b\x8a\x80\x80\x80\x00\x00\x10\x00\x20\x00\x6a\x28\x02\x00\x0b\x86\x80\x80\x80\x00\x00\x20\x00\x40\x00\x0b");

// memory_trap.wast:21
assert_return(() => call($1, "store", [-4, 42]));

// memory_trap.wast:22
assert_return(() => call($1, "load", [-4]), 42);

// memory_trap.wast:23
assert_trap(() => call($1, "store", [-3, 13]));

// memory_trap.wast:24
assert_trap(() => call($1, "load", [-3]));

// memory_trap.wast:25
assert_trap(() => call($1, "store", [-2, 13]));

// memory_trap.wast:26
assert_trap(() => call($1, "load", [-2]));

// memory_trap.wast:27
assert_trap(() => call($1, "store", [-1, 13]));

// memory_trap.wast:28
assert_trap(() => call($1, "load", [-1]));

// memory_trap.wast:29
assert_trap(() => call($1, "store", [0, 13]));

// memory_trap.wast:30
assert_trap(() => call($1, "load", [0]));

// memory_trap.wast:31
assert_trap(() => call($1, "store", [-2147483648, 13]));

// memory_trap.wast:32
assert_trap(() => call($1, "load", [-2147483648]));

// memory_trap.wast:33
assert_return(() => call($1, "grow_memory", [65537]), -1);
