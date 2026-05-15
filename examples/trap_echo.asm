; name: TRAP echo input
; description: reads one character from the TRAP input buffer and echoes the next character in ASCII.
.orig x3000
        lea r0, prompt
        puts
        getc
        add r0, r0, #0
        brz zero
        add r1, r0, #0
        out
        lea r0, next
        puts
        add r0, r1, #1
        out
        lea r0, newline
	puts
        halt
zero	lea r0, buf_ept
	puts
	lea r0, newline
	puts
	halt
buf_ept .stringz "\ninput is empty"
prompt  .stringz "Type one character: "
next	.stringz "\nthe next character is "
newline	.stringz "\n\n"
.end
