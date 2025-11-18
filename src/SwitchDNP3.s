	; r0: pointer to data
	; r1: size of data
	
	jlt r1 10 err		; DNP3 link layer header is 10 bytes
	
	ldx16 r2 r0 0		; load DNP3 start bytes
	jne r2 0x6405 err	; error start bytes != 0x0564
	
	ldx8 r2 r0  2		; load DNP3 len byte
	jgt r2 r1 err		; error len > size of data should be well less (doesn't inc CRCs etc)
	
	ldx16 r2 r0 6		; load DNP3 src addr
	ldx16 r3 r0 4		; load DNP3 dst addr
	
	mov r0 0			; success return 0
	exit
err:
    mov r0 -1			; error return -1
    exit
