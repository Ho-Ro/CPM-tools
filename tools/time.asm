; SPDX-License-Identifier: GPL-3.0-or-later
;
; get CP/M 3 date/time structure and display the time as HH:MM:SS
; using BDOS FUNCTION 105: GET DATE AND TIME
; input:
;	C: 105
;	DE: address od DAT structure
; return:
;	A: seconds (BCD)
;	DAT set
; build:
;	ZSM4 =TIME
;	LINK TIME


BDOS	equ	0005h

CR	equ	0dh
LF	equ	0ah

	aseg

	org	100h
START:
	ld	de, DAT
	ld	c, 105
	call	BDOS

	ld	(SEC), a
	ld	a, (HOUR)
	call	OutHex8
	ld	e, ':'
	ld	c, 2
	call	BDOS

	ld	a, (MIN)
	call	OutHex8
	ld	e, ':'
	ld	c, 2
	call	BDOS

	ld	a, (SEC)
	call	OutHex8

	call	CRLF

	ld	c, 0
	call	BDOS


CRLF:
	ld	e, CR
	ld	c, 2
	call	BDOS
	ld	e, LF
	ld	c, 2
	call	BDOS
	ret


OutHex8:
	push	af
	rra
	rra
	rra
	rra
	call	Conv
	pop	af
Conv:	and	0fh
	add	a, 90h
	daa
	adc	a, 40h
	daa
	; Show the value.
	ld	e, a
	ld	c, 2
	call	BDOS
	ret


; CP/M 3 date/time 4 byte structure DAT
DAT:
DAYS:	dw	0	; days since 1.1.1978
HOUR:	db	0	; hour as BCD
MIN:	db	0	; min as BCD

SEC:	db	0	; sec as BCD, ret val from BDOS fkt 105

