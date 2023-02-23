; TEXT
segment	.text
; ALIGN
align	4
; LABEL
$_exemplo:
; ENTER
	push	ebp
	mov	ebp, esp
	sub	esp, 0
; LOCV
	push	dword [ebp+8]
; IMM
	push	dword 6
; GE
	pop	eax
	xor	ecx, ecx
	cmp	[esp], eax
	setge	cl
	mov	[esp], ecx
; POP
	pop	eax
; LEAVE
	leave
; RET
	ret
; POP
	pop	eax
; LEAVE
	leave
; RET
	ret
; GLOBL
global	$_exemplo:function
