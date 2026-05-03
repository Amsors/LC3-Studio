; name: GUI demo output
; description: Prints a short message and gives a useful breakpoint at x3003.
.orig x3000
        and r0, r0, #0
        add r0, r0, #5
        add r1, r0, #3
        lea r0, msg
        puts
        halt
msg     .stringz "LC-3 GUI OK\n"
.end
