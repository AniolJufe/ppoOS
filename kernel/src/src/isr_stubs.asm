; isr_stubs.asm
; Assembly interrupt service routine stubs

section .text
global idt_load
global isr13, isr14 ; Declare the ISRs we are defining
extern isr_handler    ; External C handler function

; Macro to define ISR stubs that push an error code (if provided by CPU)
; %1: ISR number
; %2: Stub label name
%macro ISR_ERRCODE 1
  global isr%1
isr%1:
  ; Error code is already pushed by the CPU
  push %1       ; Push the interrupt number
  jmp isr_common_stub
%endmacro

; Macro to define ISR stubs that do NOT push an error code
; %1: ISR number
; %2: Stub label name
%macro ISR_NOERRCODE 1
  global isr%1
isr%1:
  push 0        ; Push a dummy error code (0)
  push %1       ; Push the interrupt number
  jmp isr_common_stub
%endmacro

; Define specific ISRs
ISR_ERRCODE 13 ; #GP General Protection Fault (Error code pushed by CPU)
ISR_ERRCODE 14 ; #PF Page Fault (Error code pushed by CPU)

; Common stub for all ISRs
isr_common_stub:
  ; Save general purpose registers (order matches struct registers in idt.h)
  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rdi
  push rsi
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  ; Call the C handler (address of struct registers is in RSP)
  mov rdi, rsp     ; Pass pointer to the saved registers struct as the first argument
  call isr_handler ; Call the C function

  ; Restore general purpose registers
  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop rsi
  pop rdi
  pop rbp
  pop rdx
  pop rcx
  pop rbx
  pop rax

  ; Clean up the pushed interrupt number and error code
  add rsp, 16

  ; Return from interrupt
  iretq

; Function to load the IDT pointer
idt_load:
  lidt [rdi] ; Load IDT register with pointer passed in RDI
  ret
