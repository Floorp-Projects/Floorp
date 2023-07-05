#[cfg(target_os = "android")]
compile_error!("please file an issue if you care about this target");

std::arch::global_asm! {
    ".text",
    ".global crash_context_getcontext",
    ".hidden crash_context_getcontext",
    ".align 4",
    ".type crash_context_getcontext, @function",
"crash_context_getcontext:",
    "movl 4(%esp), %eax",   // eax = uc

    // Save register values
    "movl %ecx, 0x3c(%eax)",
    "movl %edx, 0x38(%eax)",
    "movl %ebx, 0x34(%eax)",
    "movl %edi, 0x24(%eax)",
    "movl %esi, 0x28(%eax)",
    "movl %ebp, 0x2c(%eax)",

    "movl (%esp), %edx",   /* return address */
    "lea  4(%esp), %ecx",  /* exclude return address from stack */
    "mov  %edx, 0x4c(%eax)",
    "mov  %ecx, 0x30(%eax)",

    "xorl %ecx, %ecx",
    "movw %fs, %cx",
    "mov  %ecx, 0x18(%eax)",

    "movl $0, 0x40(%eax)",

    // Save floating point state to fpregstate, then update
    // the fpregs pointer to point to it
    "leal 0xec(%eax), %ecx",
    "fnstenv (%ecx)",
    "fldenv  (%ecx)",
    "mov %ecx, 0x60(%eax)",

    // Save signal mask: sigprocmask(SIGBLOCK, NULL, &uc->uc_sigmask)
    "leal 0x6c(%eax), %edx",
    "xorl %ecx, %ecx",
    "push %edx",   /* &uc->uc_sigmask */
    "push %ecx",   /* NULL */
    "push %ecx",   /* SIGBLOCK == 0 on i386 */
    "call sigprocmask@PLT",
    "addl $12, %esp",

    "movl $0, %eax",
    "ret",

    ".size crash_context_getcontext, . - crash_context_getcontext",
    options(att_syntax)
}
