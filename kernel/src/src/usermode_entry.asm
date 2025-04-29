section .text
global jmp_usermode

; Define GDT selectors for user mode (adjust if your GDT differs)
USER_CODE_SELECTOR equ 0x18 | 3 ; Selector 3 (0x18), RPL=3 -> 0x1b (Correct for Limine User Code)
USER_DATA_SELECTOR equ 0x20 | 3 ; Selector 4 (0x20), RPL=3 -> 0x23 (Correct for Limine User Data)

SERIAL_PORT equ 0x3F8

; Helper function to print a single hex digit (from AL)
print_hex_digit:
    cmp al, 9
    jle .digit
    add al, 'A' - 10
    jmp .print
.digit:
    add al, '0'
.print:
    mov dx, SERIAL_PORT
    out dx, al
    ret

; Helper function to print a 64-bit value in RAX in hex
print_hex_64:
    push rcx
    push rdx
    mov rcx, 16 ; 16 hex digits for 64 bits
.loop:
    rol rax, 4 ; Rotate highest nibble into lowest position
    push rax
    and al, 0x0F ; Mask lowest nibble
    call print_hex_digit
    pop rax
    loop .loop
    pop rdx
    pop rcx
    ret

; External C debug function
extern debug_print_iretq_frame

; void jmp_usermode(uint64_t user_rip, uint64_t user_rsp);
; Jumps to user mode using iretq.
; Assumes RDI = user_rip, RSI = user_rsp
jmp_usermode:
    ; SERIAL DEBUG: Print [JMPUSER] as the very first thing
    mov dx, 0x3F8
    mov al, '['
    out dx, al
    mov al, 'J'
    out dx, al
    mov al, 'M'
    out dx, al
    mov al, 'P'
    out dx, al
    mov al, 'U'
    out dx, al
    mov al, 'S'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, ']'
    out dx, al
    mov al, 10 ; '\n'
    out dx, al
    ; Disable interrupts before switching stack/segments
    cli

    ; Set up the stack frame for iretq: SS (64-bit), RSP (64-bit), RFLAGS (64-bit), CS (64-bit), RIP (64-bit)
    mov rax, USER_DATA_SELECTOR
    push rax                ; User SS
    push rsi                ; User RSP
    push 0x202              ; RFLAGS (IF=1, reserved)
    mov rax, USER_CODE_SELECTOR
    push rax                ; User CS
    push rdi                ; User RIP

    ; Zero out general purpose registers that user mode shouldn't inherit
    ; (Except RSP which is set by iretq, and RIP)
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15

    ; --- ADDED DEBUG PRINTS --- 
    mov dx, SERIAL_PORT
    mov al, ' '
    out dx, al ; Spacer
    mov al, 'R'
    out dx, al
    mov al, 'I'
    out dx, al
    mov al, 'P'
    out dx, al
    mov al, ':'
    out dx, al
    mov rax, [rsp] ; Read RIP from stack [rsp+0]
    call print_hex_64
    mov al, 10
    out dx, al

    mov al, ' '
    out dx, al
    mov al, 'C'
    out dx, al
    mov al, 'S'
    out dx, al
    mov al, ':'
    out dx, al
    mov rax, [rsp + 8] ; Read CS from stack [rsp+8]
    call print_hex_64
    mov al, 10
    out dx, al

    mov al, ' '
    out dx, al
    mov al, 'F'
    out dx, al
    mov al, 'L'
    out dx, al
    mov al, 'G'
    out dx, al
    mov al, ':'
    out dx, al
    ; RFLAGS is on the stack at [rsp + 16]
    mov rax, [rsp + 16] ; Read RFLAGS from stack [rsp+16]
    call print_hex_64
    mov al, 10
    out dx, al

    mov al, ' '
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'S'
    out dx, al
    mov al, 'P'
    out dx, al
    mov al, ':'
    out dx, al
    mov rax, [rsp + 24] ; Read RSP from stack [rsp+24]
    call print_hex_64
    mov al, 10
    out dx, al

    mov al, ' '
    out dx, al
    mov al, 'S'
    out dx, al
    mov al, 'S'
    out dx, al
    mov al, ':'
    out dx, al
    mov rax, [rsp + 32] ; Read SS from stack [rsp+32]
    call print_hex_64
    mov al, 10
    out dx, al

    mov al, '['
    out dx, al
    mov al, 'I'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'T'
    out dx, al
    mov al, 'Q'
    out dx, al
    mov al, ']'
    out dx, al
    mov al, 10 ; '\n'
    out dx, al

    ; Perform the jump to user mode
    iretq

    ; This code will be reached when the user program returns (if it does)
    ; We need a proper label here to handle the return
usermode_return:
    ; (No serial output here; returning from usermode is not expected, but handle gracefully)
    mov dx, 0x3F8
    mov al, '['
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'E'
    out dx, al
    mov al, 'T'
    out dx, al
    mov al, 'U'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, 'N'
    out dx, al
    mov al, ']'
    out dx, al
    mov al, 10 ; '\n'
    out dx, al

    ; Call the C handler for usermode return
    extern usermode_return_handler
    mov edi, 0  ; Default return code
    call usermode_return_handler
    
    ; If usermode_return_handler returns (it shouldn't), halt the CPU
    hlt

section .note.GNU-stack noalloc noexec nowrite progbits
