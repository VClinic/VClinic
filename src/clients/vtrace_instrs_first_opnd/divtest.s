.section .data
dividend:
   .quad 8335
divisor:
   .int 25
quotient:
   .int 0
remainder:
   .int 0
output:
   .asciz "<93>The quotient is %d, and the remainder is %d\n<94>"
.section .text
.globl main
main:
   nop
   movl dividend, %eax
   movl dividend+4, %edx
   mulw divisor
   divw divisor      ;//除数

   movl %eax, quotient
   movl %edx, remainder

   movq $output,%rdi
   movl quotient,%esi;//商
   movl remainder,%edx;//余数

   movl $10, %ecx
   mull %ecx
   divl %ecx

   movl $1, %eax
   movl $0, %ebx
   ret