; name: Fibonacci 64-bit hex
; description: reads decimal x from the TRAP input buffer, computes fibonacci(x), and prints 16 hex digits.

; the answer is computed modulo 2^64

; example:
; f(15) = 0x262
; f(45) = 0x43a53f82
; f(99) = 0xde2ab8cecafb7902 (mod 2^64)
;
; input examples:
; "3."  -> f(3)
; "96." -> f(96)
; "682" -> f(682)
; at most three digits are consumed; later characters are left in the input buffer

.orig x3000
read_input
        and r1, r1, #0

        getc
        ld r3, neg_period
        add r3, r0, r3
        brz begin
        ld r3, neg_ascii_zero
        add r6, r0, r3
        add r1, r1, r6

        getc
        ld r3, neg_period
        add r3, r0, r3
        brz begin
        ld r3, neg_ascii_zero
        add r6, r0, r3
        add r3, r1, r1
        add r4, r3, r3
        add r4, r4, r4
        add r1, r4, r3
        add r1, r1, r6

        getc
        ld r3, neg_period
        add r3, r0, r3
        brz begin
        ld r3, neg_ascii_zero
        add r6, r0, r3
        add r3, r1, r1
        add r4, r3, r3
        add r4, r4, r4
        add r1, r4, r3
        add r1, r1, r6

begin
        and r0, r0, #0
        ld r4, word_count
        lea r5, fib_a
clear_loop
        str r0, r5, #0
        add r5, r5, #1
        add r4, r4, #-1
        brp clear_loop

        lea r5, fib_b
        add r0, r0, #1
        str r0, r5, #0

        add r1, r1, #0
        brz output_a
        add r1, r1, #-1
        brz output_b

fib_loop
        lea r3, fib_a
        lea r4, fib_b
        lea r5, fib_c
        and r6, r6, #0
        and r2, r2, #0
        add r2, r2, #15
        add r2, r2, #1

add_loop
        ldr r0, r3, #0
        ldr r7, r4, #0
        add r0, r0, r7
        add r0, r0, r6
        add r7, r0, #-16
        brn no_carry
        str r7, r5, #0
        and r6, r6, #0
        add r6, r6, #1
        brnzp add_next
no_carry
        str r0, r5, #0
        and r6, r6, #0
add_next
        add r3, r3, #1
        add r4, r4, #1
        add r5, r5, #1
        add r2, r2, #-1
        brp add_loop

        lea r3, fib_a
        lea r4, fib_b
        lea r5, fib_c
        and r2, r2, #0
        add r2, r2, #15
        add r2, r2, #1

copy_loop
        ldr r0, r4, #0
        str r0, r3, #0
        ldr r0, r5, #0
        str r0, r4, #0
        add r3, r3, #1
        add r4, r4, #1
        add r5, r5, #1
        add r2, r2, #-1
        brp copy_loop

        add r1, r1, #-1
        brp fib_loop

output_b
        lea r5, fib_b
        brnzp print_result
output_a
        lea r5, fib_a

print_result
        add r5, r5, #15
        and r4, r4, #0
        add r4, r4, #15
        add r4, r4, #1
print_loop
        ldr r3, r5, #0
        jsr print_digit
        add r5, r5, #-1
        add r4, r4, #-1
        brp print_loop
        halt

print_digit
        st r7, digit_ret
        add r0, r3, #-10
        brn decimal_digit
        ld r6, ascii_a_minus_10
        add r0, r3, r6
        out
        ld r7, digit_ret
        ret
decimal_digit
        ld r6, ascii_zero
        add r0, r3, r6
        out
        ld r7, digit_ret
        ret

word_count       .fill #48
neg_ascii_zero   .fill #-48
neg_period       .fill #-46
ascii_zero       .fill x0030
ascii_a_minus_10 .fill x0037
digit_ret        .blkw #1
fib_a            .blkw #16
fib_b            .blkw #16
fib_c            .blkw #16
.end
