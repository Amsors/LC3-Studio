; name: TRAP echo input
; description: Reads one character from the TRAP input buffer and echoes it.
.orig x3000
        lea r0, prompt
        puts
        getc
        out
        lea r0, newline
        puts
        halt
prompt  .stringz "Type one character: "
newline .stringz "\n"
.end
