.section .text

.global _syscall
.global _start

# Program entry point
_start:
    # Simple entry point
    # Zero out frame pointer for unwinding (optional but good practice)
    xor %rbp, %rbp

    # Setup argc/argv (simplified - pass 0, NULL)
    xor %rdi, %rdi # argc = 0
    xor %rsi, %rsi # argv = NULL

    # Call main
    call main

    # main returns result in rax
    # Move return value from main (rax) to first arg for exit (rdi)
    mov %rax, %rdi

    # Call exit
    call exit

    # Should not return from exit, but halt if it does
_start_hlt:
    hlt
    jmp _start_hlt

_syscall:
    # Arguments are expected in rdi, rsi, rdx, r10, r8, r9 for syscalls
    # The syscall number is in rax
    mov %rdi, %rax  # Move syscall number to rax
    mov %rsi, %rdi  # Move arg1 to rdi
    mov %rdx, %rsi  # Move arg2 to rsi
    mov %rcx, %rdx  # Move arg3 to rdx (note: calling convention uses rcx for 4th arg, syscall uses r10)
    mov %r8,  %r10  # Move arg4 to r10 (note: syscall uses r8 for 5th arg)
    mov %r9,  %r8   # Move arg5 to r8 (note: syscall uses r9 for 6th arg)
                    # arg6 (r9) is not used by our current syscalls

    # Execute syscall
    syscall

    # Return value is in rax
    ret
