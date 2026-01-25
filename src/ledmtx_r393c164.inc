  list
; ledmtx_r393c164.inc - libledmtx r393c164 driver configuration
  nolist

LEDMTX_R393C164_IOPORT	equ	PORTA
LEDMTX_R393C164_RCLK	equ	RA1
LEDMTX_R393C164_RRST	equ	RA2
LEDMTX_R393C164_RENA	equ	RA3
LEDMTX_R393C164_CCLK	equ	RA5
LEDMTX_R393C164_CDAT	equ	RA4

; Uncomment to enable experimental software-based PWM for brightness adjustment
; (requires an additional timer module; defaults to Timer3).
#define LEDMTX_R393C164_E_SOFTPWM 1

#ifdef LEDMTX_R393C164_E_SOFTPWM
  global ___ledmtx_r393c164_E_softpwm_tmr3
.udata_ledmtx_driver	udata
___ledmtx_r393c164_E_softpwm_tmr3	res	2

LEDMTX_R393C164_E_SOFTPWM_DUTY_BEGIN	macro
  movff	___ledmtx_r393c164_E_softpwm_tmr3+1, TMR3H
  movff	___ledmtx_r393c164_E_softpwm_tmr3+0, TMR3L
  bsf		T3CON, TMR3ON, A
  endm

LEDMTX_R393C164_E_SOFTPWM_DUTY_END	macro
  bcf		PIR2, TMR3IF, A
  bcf		LEDMTX_R393C164_IOPORT, LEDMTX_R393C164_RENA, A
  bcf		T3CON, TMR3ON, A
  retfie
  endm
#endif
