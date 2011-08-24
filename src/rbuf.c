/*
 * rbuf.c - ring buffer routines
 *
 * Copyright (C) 2011  Javier L. Gomez
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

extern char FSR0L;
extern char FSR0H;
extern char INDF0;
extern char PLUSW0;
extern char POSTDEC0;
extern char POSTINC0;
extern char PLUSW1;
extern char POSTDEC1;
extern char PREINC1;

// clang-format off
char rbuf_put(char c, __data char *buf) __wparam __naked
{
  (void)c;
  (void)buf;
  __asm
  movwf		_POSTDEC1, 0		; push c
  movlw		2
  movff		_PLUSW1, _FSR0L		; FSR0 points to buf
  movlw		3
  movff		_PLUSW1, _FSR0H
  
  movff		_POSTINC0, _POSTDEC1	; full buffer?
  incf		_INDF0, w, 0
  andwf		_PREINC1, w, 0
  movff		_POSTINC0, _POSTDEC1	; push pointer
  xorwf		_POSTINC0, w, 0
  bnz		@putc
  movf		_PREINC1, f, 0		; restore stack
  movf		_PREINC1, f, 0
  retlw		-1
  
@putc:
  movf		_PREINC1, w, 0		; pop pointer
  movff		_PREINC1, _PLUSW0	; pop c and store
  movf		_POSTDEC0, f, 0
  movf		_POSTDEC0, f, 0
  incf		_INDF0, f, 0		; update pointer
  movlw		-1
  movf		_PLUSW0, w, 0
  andwf		_INDF0, f, 0
  retlw		0
  __endasm;
}
// clang-format on

// clang-format off
char rbuf_get(__data char *buf) __naked
{
  (void)buf;
  __asm
  movlw		1
  movff		_PLUSW1, _FSR0L		; FSR0 points to buf
  movlw		2
  movff		_PLUSW1, _FSR0H
  
  movff		_POSTINC0, _POSTDEC1	; push mask
  movff		_POSTINC0, _POSTDEC1
  movf		_INDF0, w, 0
  xorwf		_PREINC1, w, 0		; empty buffer?
  bnz		@getc
  movf		_PREINC1, f, 0		; restore stack
  retlw		-1
  
@getc:
  incf		_INDF0, w, 0		; incf pointer
  andwf		_PREINC1, w, 0
  movwf		_POSTDEC1, 0		; push
  movf		_INDF0, w, 0
  movff		_PREINC1, _POSTINC0	; pop and update
  movf		_PLUSW0, w, 0		; c -> WREG
  return
  __endasm;
}
// clang-format on
