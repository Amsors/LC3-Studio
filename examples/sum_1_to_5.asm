; name: sum 1 to 5
; description: loops from 5 down to 1 and leaves 15 in R0.
.orig x3000
        and r0, r0, #0
        and r1, r1, #0
        add r1, r1, #5
loop    add r0, r0, r1
        add r1, r1, #-1
        brp loop
        halt
.end
