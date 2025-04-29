[bits 64]

global syscall_asm_entry
extern syscall

; Define GDT selectors for user mode (adjust if your GDT differs)
USER_CODE_SELECTOR equ 0x18 | 3 ; Selector 3 (0x18), RPL=3 -> 0x1b
USER_DATA_SELECTOR equ 0x20 | 3 ; Selector 4 (0x20), RPL=3 -> 0x23

; This function is called by the SYSCALL instruction
; Parameters are passed in registers according to the x86_64 ABI:
; - RAX: syscall number
; - RDI: arg1
; - RSI: arg2
; - RDX: arg3
; - R10: arg4 (rcx is overwritten by syscall instruction)
; - R8:  arg5
; - R9:  arg6 (not used in our implementation)

section .bss
align 16
kernel_stack_bottom:
    resb 8192 ; 8KB kernel stack for syscalls
kernel_stack_top:
user_rsp_storage: resq 1 ; Storage for user RSP
user_rip_storage: resq 1 ; Storage for user RIP (from RCX)
user_rflags_storage: resq 1 ; Storage for user RFLAGS (from R11)

section .text

syscall_asm_entry:
    ; Swap GS base to kernel space if using FS/GS base switching (optional, depends on setup)
    ; swapgs ; Uncomment if needed

    ; Switch to kernel stack
    mov [user_rsp_storage], rsp ; Save user RSP
    mov rsp, kernel_stack_top   ; Load kernel RSP

    ; Save registers needed by C ABI and syscall
    ; Callee-saved: rbx, rbp, r12-r15
    ; Syscall clobbers: rcx, r11
    ; Need to save user rip (rcx) and rflags (r11) for iretq
    mov [user_rip_storage], rcx     ; Save user RIP
    mov [user_rflags_storage], r11 ; Save user RFLAGS

    ; Save callee-saved GPRs
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Pass arguments to C handler (syscall):
    ; Syscall num (rax) -> rdi (1st C arg)
    ; Syscall arg 1 (rdi) -> rsi (2nd C arg)
    ; Syscall arg 2 (rsi) -> rdx (3rd C arg)
    ; Syscall arg 3 (rdx) -> rcx (4th C arg)
    ; Syscall arg 4 (r10) -> r8  (5th C arg)
    ; Syscall arg 5 (r8)  -> r9  (6th C arg)

    ; Correct order to avoid clobbering:
    mov r9, r8      ; C arg5 <- syscall arg5 (R8)
    mov r8, r10     ; C arg4 <- syscall arg4 (R10)
    mov rcx, rdx    ; C arg3 <- syscall arg3 (RDX)
    mov rdx, rsi    ; C arg2 <- syscall arg2 (RSI)
    mov rsi, rdi    ; C arg1 <- syscall arg1 (RDI)
    mov rdi, rax    ; C num  <- syscall num (RAX)

    ; Align the stack to 16 bytes before calling C function
    ; Check current RSP alignment. RSP & 0xF should be 0 for alignment.
    ; Current RSP = kernel_stack_top - 6*8 (callee-saved) = kernel_stack_top - 48
    ; If kernel_stack_top is 16-byte aligned, RSP is currently aligned.
    ; No explicit alignment needed if kernel_stack_top is correctly aligned.

    ; Call the C handler
    call syscall

    ; RAX contains the return value from syscall() C function

    ; Restore callee-saved GPRs
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Kernel stack pointer is now back to kernel_stack_top.
    ; Construct the iretq frame:
    ; iretq expects: [RIP] [CS] [RFLAGS] [RSP] [SS]
    push qword USER_DATA_SELECTOR ; User SS
    push qword [user_rsp_storage] ; User RSP (restored)
    push qword [user_rflags_storage] ; User RFLAGS
    push qword USER_CODE_SELECTOR ; User CS
    push qword [user_rip_storage] ; User RIP

    ; DO NOT restore user RSP here, iretq does it.
    ; DO NOT swapgs here if used, iretq handles it implicitly based on target CS descriptor type.

    ; Return to userspace using iretq
    ; RAX contains the return value from syscall() C function
    iretq

