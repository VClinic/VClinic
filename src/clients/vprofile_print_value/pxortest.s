.section .data
dividend:
   .quad 8335
divisor:
   .int 25
quotient:
   .int 0
remainder:
   .int 0
num:
   .quad 0x200000000
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
   ;// INS src, des
   movd %eax, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0
   paddb %xmm0, %xmm0


   add $2, %eax
   paddb %xmm0, %xmm0
   add $3, %eax
   paddb %xmm0, %xmm0
   add $4, %eax
   paddb %xmm0, %xmm0
   add $5, %eax
   paddb %xmm0, %xmm0
   add $6, %eax
   paddb %xmm0, %xmm0
   add $7, %eax
   paddb %xmm0, %xmm0
   add $8, %eax
   paddb %xmm0, %xmm0
   add $9, %eax
   paddb %xmm0, %xmm0
   add $10, %eax
   paddb %xmm0, %xmm0
   divl %ecx
   ret
