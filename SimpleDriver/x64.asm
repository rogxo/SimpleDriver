
extern MouHid_JmpRax : dq
extern MouHid_AddRsp28hRet : dq
extern MouHid_CopAddress : dq


extern KbdHid_JmpRax : dq
extern KbdHid_AddRsp28hRet : dq

extern MouseClassServiceCallback : dq
extern KeyboardClassServiceCallback : dq


.code

AsmTrickMouseCall PROC
	push rsi
	push rdi
	push r12
	push r13
	push r14
	push [MouHid_AddRsp28hRet]

	mov rax, [MouseClassServiceCallback]
	jmp [MouHid_JmpRax]
AsmTrickMouseCall ENDP

AsmCopMouseCall PROC
	sub		rsp, 1000h
	mov		rdi, rsp					; DriverExtension

	lea		rax, LABEL_CALLRET
	push	rax
;-----------------------------
	push    rbp
	push    rbx
	push    rsi
	push    rdi
	push    r12
	push    r13
	push    r14
	push    r15
	mov     rbp, rsp
	sub     rsp, 58h
;-----------------------------
	mov		rax, [MouseClassServiceCallback]
	mov		[rdi+0E8h], rax				;
	mov     [rdi+0E0h], rcx				; DeviceObject

	mov		rbx, rdi					; Save rdi
	lea     rdi, [rdi+160h]				; InputDataStart
	mov     rsi, rdx					; pInputData
	mov     ecx, 18h
	rep movs byte ptr [rdi], byte ptr [rsi]		; rep movsb
	mov		rdi, rbx					; Restore rdi

	mov     [rbp-014h], r9				; &Consumed

	mov		rax, cr8
	mov		byte ptr [rbp+50h], al		; bypass IoAllocateErrorLogEntry
	mov		rax, 0
	mov     [rbp+58h], rax				; bypass jump up

	jmp		[MouHid_CopAddress]
	;-----------------------------
	;add     rsp, 58h
	;pop     r15
	;pop     r14
	;pop     r13
	;pop     r12
	;pop     rdi
	;pop     rsi
	;pop     rbx
	;pop     rbp
	;ret
LABEL_CALLRET:
	add		rsp, 1000h
	ret
AsmCopMouseCall ENDP

AsmTrickKeyboardCall PROC
	push rsi
	push rdi
	push r12
	push r13
	push r14
	push [KbdHid_AddRsp28hRet]

	mov rax, [KeyboardClassServiceCallback]
	jmp [KbdHid_JmpRax]
AsmTrickKeyboardCall ENDP

end