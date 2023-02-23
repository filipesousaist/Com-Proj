; TEXT
segment	.text
; ALIGN
align	4
; LABEL
$_factorial:
; ENTER
	push	ebp
	mov	ebp, esp
	sub	esp, 0
; LOCV
	push	dword [ebp+8]
; EXTRN
extern	$_printi
; CALL
	call	$_printi
; TRASH
	add	esp, 4
; RODATA
segment	.rodata
; ALIGN
align	4
; LABEL
$_L1:
; CHAR
	db	0x20
; CHAR
	db	0x00
; TEXT
segment	.text
; ADDR
	push	dword $_L1
; EXTRN
extern	$_prints
; CALL
	call	$_prints
; TRASH
	add	esp, 4
; LOCV
	push	dword [ebp+12]
; EXTRN
extern	$_printi
; CALL
	call	$_printi
; TRASH
	add	esp, 4
; RODATA
segment	.rodata
; ALIGN
align	4
; LABEL
$_L2:
; CHAR
	db	0x0A
; CHAR
	db	0x00
; TEXT
segment	.text
; ADDR
	push	dword $_L2
; EXTRN
extern	$_prints
; CALL
	call	$_prints
; TRASH
	add	esp, 4
; LOCV
	push	dword [ebp+8]
; IMM
	push	dword 1
; EQ
	pop	eax
	xor	ecx, ecx
	cmp	[esp], eax
	sete	cl
	mov	[esp], ecx
; COPY
	push	dword [esp]
; JNZ
	pop	eax
	cmp	eax, byte 0
	jne	near $_L3
; TRASH
	add	esp, 4
; LOCV
	push	dword [ebp+8]
; IMM
	push	dword 0
; EQ
	pop	eax
	xor	ecx, ecx
	cmp	[esp], eax
	sete	cl
	mov	[esp], ecx
; LABEL
$_L3:
; JZ
	pop	eax
	cmp	eax, byte 0
	je	near $_L4
; IMM
	push	dword 1
; POP
	pop	eax
; LEAVE
	leave
; RET
	ret
; JMP
	jmp	dword $_L5
; LABEL
$_L4:
; LOCV
	push	dword [ebp+8]
; LOCV
	push	dword [ebp+12]
; LOCV
	push	dword [ebp+8]
; IMM
	push	dword 1
; SUB
	pop	eax
	sub	dword [esp], eax
; CALL
	call	$_factorial
; TRASH
	add	esp, 8
; PUSH
	push	eax
; MUL
	pop	eax
	imul	dword eax, [esp]
	mov	[esp], eax
; POP
	pop	eax
; LEAVE
	leave
; RET
	ret
; LABEL
$_L5:
; POP
	pop	eax
; LEAVE
	leave
; RET
	ret
; GLOBL
global	$_factorial:function
