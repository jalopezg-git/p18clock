/*
 * main.c - p18clock source file
 *
 * Copyright (C) 2011-2025  Javier Lopez-Gomez
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

// clang-format off
#include <ledmtx_scrollstr.h>

#include <ledmtx_config.h>
#include <ledmtx_core.h>
#include <ledmtx_fontstd.h>
#include <ledmtx_stdio.h>
#if LEDMTX_HAVE_VIEWPORT != 1
#error p18clock requires a libledmtx build that has ENABLE_VIEWPORT=1

#endif

#include <delay.h>
#include <signal.h>
#include <stdio.h>
// clang-format on

#include <lang.h>
#include <lm35.h>
#include <rbuf.h>

#include "ledmtx_modegen_modes.h"

/* CONFIG1L */
#if defined(__SDCC_PIC18F2550)
#pragma config USBDIV = 1 // USB clock source comes directly from the primary
                          // oscillator block with no postscale
#pragma config CPUDIV =                                                        \
    OSC1_PLL2             // [Primary Oscillator Src: /1][96 MHz PLL Src: /2]
#pragma config PLLDIV = 2 // Divide by 2 (8 MHz oscillator input)
#endif                    /* __SDCC_PIC18F2550 */

/* CONFIG1H */
#if defined(__SDCC_PIC18F2550)
#pragma config FOSC = HS // HS oscillator (HS)
#else
#pragma config OSC = HS // HS oscillator (HS)
#endif                  /* __SDCC_PIC18F2550 */

/* CONFIG2L */
#if defined(__SDCC_PIC18F2550)
#pragma config BOR = OFF // Brown-out Reset disabled in hardware and software
#else
#pragma config BOREN = OFF // Brown-out Reset disabled in hardware and software
#endif                     /* __SDCC_PIC18F2550 */
#pragma config BORV = 2    // Setting 1 2.79V

/* CONFIG2H */
#pragma config WDT = OFF // WDT disabled

/* CONFIG3H */
#if defined(__SDCC_PIC18F2550)
#pragma config CCP2MX = ON // CCP2 input/output is multiplexed with RC1
#else
#pragma config CCP2MX = PORTC // CCP2 input/output is multiplexed with RC1
#endif                        /* __SDCC_PIC18F2550 */

#pragma config PBADEN =                                                        \
    OFF // PORTB<4:0> pins are configured as digital I/O on Reset
#pragma config LPT1OSC = OFF // Timer1 configured for higher power operation
#pragma config MCLRE = ON    // MCLR pin enabled; RE3 input pin disabled

/* CONFIG4L */
#pragma config STVREN = ON // Stack full/underflow will cause Reset
#pragma config LVP = OFF   // Single-Supply ICSP disabled
#pragma config XINST =                                                         \
    OFF // Instruction set extension and Indexed Addressing mode disabled
        // (Legacy mode); XINST not supported in SDCC
#pragma config DEBUG = OFF // Background debugger disabled, RB6 and RB7
                           // configured as general purpose I/O

/* The IDLOC0, IDLOC1, and IDLOC2 bytes store the firmware version */
__CONFIG(__IDLOC0, 2);
__CONFIG(__IDLOC1, 0);
__CONFIG(__IDLOC2, 2);

#define UNDEF ((unsigned char)-1)

/// Base delay for re-queueing last keystroke, in Timer3 ticks after prescaler.
/// The effective delay can be adjusted by changing Timer3 prescaler. Default to
/// initial repetition after 1s; all other repetitions happen every 0.125s.
#define INPUT_REP_DELAY_BASE 4096
#define INPUT_REP_PRESCALER_INITIAL 0x3
#define INPUT_REP_PRESCALER 0x1

/// Interval (in seconds) that separates two LM35 samples.
#define TEMPERATURE_INTERVAL 20

// Interval (in minutes) before entering vertical scroll in AUTO mode.
#define AUTO_INTERVAL 5

/* PWM 2.44khz@50 (Fosc=8mhz) */
#define CCP1_PR2 0xcb
#define CCP1_R 0x066
#define CCP1_T2PS 0x01

LEDMTX_BEGIN_MODULES_INIT
LEDMTX_MODULE_INIT(scrollstr)
LEDMTX_END_MODULES_INIT

LEDMTX_DECLARE_FRAMEBUFFER(LEDMTX__DEFAULT_WIDTH, LEDMTX__DEFAULT_HEIGHT)

#if defined(__P18CLOCK_LARGE_DISPLAY)
#define LEDMTX_VIEWPORT_HEIGHT 8
#define P18CLOCK_FONT_SMALL ledmtx_font5x7
#define P18CLOCK_FONT_LARGE ledmtx_font6x8

#else
#define LEDMTX_VIEWPORT_HEIGHT 7
#define P18CLOCK_FONT_SMALL ledmtx_font5x7
#define P18CLOCK_FONT_LARGE ledmtx_font5x7

#endif /* __P18CLOCK_LARGE_DISPLAY */

#define CLASS_INPUT (0 << 6)
#define CLASS_RTC (1 << 6)

/// A circular buffer is used to communicate ISR code with the main loop.  Data
/// is typically queued from an ISR and dispatched from the main loop. The 2 MSb
/// of a message are reserved for the message class, i.e., a tag that groups
/// messages according to their semantics.
#define MESSAGE_CLASS(m) (m & 0xc0)

/// The year has changed and `_days_per_month[1]` may need updating per
/// definition of leap year
#define YEARCHG (CLASS_RTC | 0x00)

/// The corresponding button has been pushed
#if !defined(__P18CLOCK_REVERSE_PUSHB_ORDER)
#define B_MODE (CLASS_INPUT | 0x08)
#define B_SET (CLASS_INPUT | 0x0a)
#define B_UP (CLASS_INPUT | 0x0c)
#define B_DOWN (CLASS_INPUT | 0x0e)

#else
#define B_MODE (CLASS_INPUT | 0x0e)
#define B_SET (CLASS_INPUT | 0x0c)
#define B_UP (CLASS_INPUT | 0x0a)
#define B_DOWN (CLASS_INPUT | 0x08)

#endif /* __P18CLOCK_REVERSE_PUSHB_ORDER */

DECLARE_RBUF(_mbuf, 32)

// # Summary of PIC peripheral usage:
// - Timer0: used by libledmtx ISR and INT0 interrupt unmasking
// - Timer1: used by RTC; external 32.768 kHz clock
// - Timer2/CCP1: used by CCP1 module in PWM mode (for buzzer)
// - Timer3: used for keystroke repetition
//
// # External interrupt sources:
// - INT0: user input (keystroke)
DEF_INTHIGH(_high_int)
DEF_HANDLER(SIG_TMR1, _tmr1_handler)
DEF_HANDLER(SIG_INT0, _int0_handler)
DEF_HANDLER(SIG_TMR3, _tmr3_handler)
END_DEF

DEF_INTLOW(_low_int)
DEF_HANDLER(SIG_TMR0, _tmr0_handler)
END_DEF

/// Number of days for each month of the year in the Gregorian calendar
static unsigned char _days_per_month[] = {31, 28, 31, 30, 31, 30,
                                          31, 31, 30, 31, 30, 31};

#define LEAP_YEAR(year)                                                        \
  ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0))

#define YEAR_MIN 2000
#define YEAR_MAX 2099

/// Struct that holds the current date and time
volatile struct {
  unsigned char sec;
  unsigned char min;
  unsigned char hour;
  unsigned char mday;
  unsigned char mon;
  unsigned int year;
} _time = {0, 0, 0, 1, 1, YEAR_MIN};

/// Struct that holds information about the alarm
static struct {
  unsigned char hour;
  unsigned char min;
  unsigned ena : 1;
  unsigned nack : 1;
  unsigned : 6;
} _alarm = {0, 0, 0, 0};

/// Last LM35 measurement, in Celsius degrees.
static int _temperature = 0;

// clang-format off
SIGHANDLERNAKED(_tmr1_handler)
{
  __asm
  extern _rbuf_put
  bsf		_TMR1H, 7, 0
  bcf		_PIR1, 0, 0		; TMR1IF ack interrupt
  
  banksel	__time+0
  incf		__time+0, f		; _time.sec++
  movlw		59
  cpfsgt	__time+0
  retfie	1
  
  ; _time.sec overflow
  clrf		__time+0		; _time.sec=0
  banksel	__time+1
  incf		__time+1, f		; _time.min++
  ;movlw	59
  cpfsgt	__time+1
  retfie	1
  
  ; _time.min overflow
  clrf		__time+1		; _time.min=0
  banksel	__time+2
  incf		__time+2, f		; _time.hour++
  movlw		23
  cpfsgt	__time+2
  retfie	1
  
  ; _time.hour overflow
  clrf		__time+2		; _time.hour=0
  movff		_FSR0H, _POSTDEC1	; push FSR0x
  movff		_FSR0L, _POSTDEC1
  lfsr		0, __days_per_month
  banksel	__time+4
  decf		__time+4, w
  movf		_PLUSW0, w, 0		; _days_per_month[_time.mon-1] -> WREG
  banksel	__time+3
  incf		__time+3, f		; _time.mday++
  cpfsgt	__time+3
  bra		@tmr1__ret
  
  ; _time.mday overflow
  movlw		1
  movwf		__time+3		; _time.mday=1
  banksel	__time+4
  incf		__time+4, f		; _time.mon++
  movlw		12
  cpfsgt	__time+4
  bra		@tmr1__ret
  
  ; _time.mon overflow
  movlw		1
  movwf		__time+4		; _time.mon=1
  banksel	__time+5
  incf		__time+5, f		; _time.year++
  btfsc		_STATUS, 0, 0		; C
  incf		__time+6, f
  
  movlw		high __mbuf		; push &_mbuf
  movwf		_POSTDEC1, 0
  movlw		low __mbuf
  movwf		_POSTDEC1, 0
  movlw		YEARCHG			; put YEARCHG
  call		_rbuf_put
  movlw		2
  addwf		_FSR1L, f, 0
  
@tmr1__ret:
  movff		_PREINC1, _FSR0L	; pop FSR0x
  movff		_PREINC1, _FSR0H
  retfie	1
  __endasm;
}
// clang-format on

#define INT0_UNMASK_TIMEOUT 7
/// INT0 interrupt is externally triggered on RB0/INT0 pin.  RB0:RB3 is
/// (indirectly) driven by push buttons, and thus requires some debouncing
/// logic. In particular, `_int0_handler` masks INT0 interrupts for some
/// time after which they are enabled again.  This interval is given by
/// `INT0_UNMASK_TIMEOUT`.
static volatile unsigned char _int0_timeout;

/// Queue user input (4 LSb of PORTB) in the `_mbuf` message buffer.
// clang-format off
void I_queue_keystroke(void) __naked
{
  __asm
  movlw		high __mbuf		; push &_mbuf
  movwf		_POSTDEC1, 0
  movlw		low __mbuf
  movwf		_POSTDEC1, 0
  movlw		0x0f			; put (PORTB&0x0f)|CLASS_INPUT
  andwf		_PORTB, w, 0
  iorlw		CLASS_INPUT
  call		_rbuf_put
  movlw		2
  addwf		_FSR1L, f, 0
  return
  __endasm;
}
// clang-format on

// clang-format off
SIGHANDLERNAKED(_int0_handler)
{
  __asm
  bcf		_INTCON, 1, 0		; ack interrupt
  btfsc		_INTCON2, 6, 0
  bra		@int0__rising_edge

  ;; Triggered due to falling edge, i.e. key press
  movff		_FSR0H, _POSTDEC1	; push FSR0x
  movff		_FSR0L, _POSTDEC1
  call		_I_queue_keystroke
  movff		_PREINC1, _FSR0L	; pop FSR0x
  movff		_PREINC1, _FSR0H

  movlw		((0xffff - INPUT_REP_DELAY_BASE) >> 8)
  movwf		_TMR3H, 0
  movlw		((0xffff - INPUT_REP_DELAY_BASE) & 0xff)
  movwf		_TMR3L, 0
  movlw		(0x83 | (INPUT_REP_PRESCALER_INITIAL << 3))
  movwf		_T3CON, 0
@int0__toggle_edge:
  bcf		_INTCON, 4, 0		; mask INT0 interrupt
  btg		_INTCON2, 6, 0		; toggle edge
  movlw		INT0_UNMASK_TIMEOUT	; INT0 unmasking is done in _tmr0_handler
  banksel	__int0_timeout
  movwf		__int0_timeout
  retfie	1

  ;; Triggered due to rising edge, i.e. key release
@int0__rising_edge:
  bcf		_T3CON, 0, 0
  bra		@int0__toggle_edge
  __endasm;
}
// clang-format on

// clang-format off
SIGHANDLERNAKED(_tmr3_handler)
{
  __asm
  bcf		_PIR2, 1, 0		; TMR3IF ack interrupt
  btfsc		_PORTB, 0, 0		; INT0 pin driven high, i.e. key released; stop Timer3
  bra		@tmr3__stop

  movff		_FSR0H, _POSTDEC1	; push FSR0x
  movff		_FSR0L, _POSTDEC1
  call		_I_queue_keystroke
  movff		_PREINC1, _FSR0L	; pop FSR0x
  movff		_PREINC1, _FSR0H

  movlw		((0xffff - INPUT_REP_DELAY_BASE) >> 8)
  movwf		_TMR3H, 0
  movlw		((0xffff - INPUT_REP_DELAY_BASE) & 0xff)
  movwf		_TMR3L, 0
  movlw		(0x83 | (INPUT_REP_PRESCALER << 3))
  movwf		_T3CON, 0
  retfie	1

@tmr3__stop:
  bcf		_T3CON, 0, 0
  retfie	1
  __endasm;
}
// clang-format on

// clang-format off
SIGHANDLERNAKED(_tmr0_handler)
{
  LEDMTX_BEGIN_ISR
  LEDMTX_BEGIN_R0
  ledmtx_scrollstr_interrupt();
  LEDMTX_END_R0
  
  LEDMTX_VERTREFRESH
  
  __asm
  btfsc		_INTCON, 4, 0		; INT0 interrupt masked?
  bra		@int0__unmasked
  
  movff		_BSR, _POSTDEC1		; push BSR
  banksel	__int0_timeout
  dcfsnz	__int0_timeout, f	; _int0_timeout--
  bsf		_INTCON, 4, 0		; _int0_timeout==0? unmask INT0 interrupt
  movff		_PREINC1, _BSR		; pop BSR
@int0__unmasked:
  __endasm;
  
  LEDMTX_END_ISR
}
// clang-format on

/// Carry out initialization tasks, e.g. set the `TRISx` registers and configure
/// peripherals
void uc_init(void) {
  OSCCONbits.IDLEN = 1; // Device enters idle mode on `sleep` instruction

  TRISA = 0xc1;
  PORTA = 0x00;
  TRISB = 0x0f;
  PORTB = 0x00;
  TRISC = 0xfb;
  PORTC = 0x00;
  INTCON2bits.RBPU = 1; // Disable RBx pullup

  lm35_init();

  /* Timer1, see p18f2550 datasheet.  RTC */
  TMR1H = 0x80;
  TMR1L = 0x00;
  T1CON = 0x0f;
  PIE1bits.TMR1IE = 1;
  IPR1bits.TMR1IP = 1;

  /* Timer3 */
  PIE2bits.TMR3IE = 1;
  IPR2bits.TMR3IP = 1;

  PR2 = CCP1_PR2;
  CCPR1L = (CCP1_R >> 2);
  CCP1CON = (CCP1_R & 0x03) << 4;
  T2CON = (CCP1_T2PS & 0x03);

  ledmtx_init(LEDMTX_INIT_CLEAR | LEDMTX_INIT_TMR0, LEDMTX__DEFAULT_WIDTH,
              LEDMTX__DEFAULT_HEIGHT, LEDMTX__DEFAULT_TMR0H,
              LEDMTX__DEFAULT_TMR0L, LEDMTX__DEFAULT_T0CON);
  // Framebuffer is either 32x31 or 40x35; set initial viewport height. Vertical
  // scroll is simulated in p18clock by changing the viewport.
  ledmtx_setviewport(0, 0, LEDMTX__DEFAULT_WIDTH, LEDMTX_VIEWPORT_HEIGHT);

  INTCON2bits.INTEDG0 = 0; // INT0 triggered on falling edge
  INTCONbits.INT0IE = 1;

  _temperature = lm35_get();
}

/// Return the day of the week for the given date, i.e. 0 - Sunday, 1 - Monday,
/// ..., 6 - Saturday. Implements the Tomohiko Sakamoto's day-of-the-week
/// algorithm.
char day_of_week(unsigned char mday, unsigned char mon, unsigned int year) {
  static const char t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  year -= mon < 3;
  return (year + (year / 4) - (year / 100) + (year / 400) + t[mon - 1] + mday) %
         7;
}

// clang-format off
/// Enumeration of the possible states of the FSM below.  Transition between
/// main states is triggered by B_MODE; B_SET transitions between sub-states,
/// if applicable.
///                                             B_MODE
///        ,-----------------------------------------------------------------------------------.
///       v                                                                                    `|
/// +------------+        +------------+        +------------+        +-------------+        +------------+
/// | STATE_TIME | -----> | STATE_DATE | -----> | STATE_TEMP | -----> | STATE_ALARM | -----> | STATE_AUTO |
/// +------------+ B_MODE +------------+ B_MODE +------------+ B_MODE +-------------+ B_MODE +------------+
///    |       ^             |       ^             |      ^              |        ^
///    | B_SET  \            | B_SET  \            | B_SET \             | B_SET  |
///    v         `           v         `           v        `            v         `
/// +----------+  `       +----------+  `       +---------+  `        +----------+  `
/// | _SETHOUR |  |       | _SETMDAY |  |       | _SETVDD |  |        | _SETHOUR |  |
/// +----------+  |       +----------+  |       +---------+  |        +----------+  |
///    | B_SET    |          | B_SET    |          | B_SET   |           | B_SET    |
///    v          |          v          |          v         |           v          |
/// +----------+  ,       +----------+  |       +---------+  ,        +----------+  ,
/// | _SETMIN  |--        | _SETMON  |  |       | _SETOFF |--         | _SETMIN  |--
/// +----------+          +----------+  |       +---------+           +----------+
///                          | B_SET    |
///                          v          |
///                       +----------+  ,
///                       | _SETYEAR |--
///                       +----------+
// clang-format on
#define STATE_TIME 0x00
#define STATE_DATE 0x01
#define STATE_TEMP 0x02
#define STATE_ALARM 0x03
#define STATE_AUTO 0x04
#define STATE_TIME_SETHOUR 0x05
#define STATE_TIME_SETMIN 0x06
#define STATE_DATE_SETMDAY 0x07
#define STATE_DATE_SETMON 0x08
#define STATE_DATE_SETYEAR 0x09
#define STATE_TEMP_SETVDD 0x0a
#define STATE_TEMP_SETOFF 0x0b
#define STATE_ALARM_SETHOUR 0x0c
#define STATE_ALARM_SETMIN 0x0d

/// The current state of the FSM
static unsigned char _state = STATE_TIME_SETHOUR;

/// A single descriptor (and NUL-terminated string) is used for scrolling text.
static char _tmpstr[64];
static struct ledmtx_scrollstr_desc _scroll_desc = {2,
                                                    2,
                                                    ledmtx_scrollstr_step,
                                                    LEDMTX_VIEWPORT_HEIGHT,
                                                    LEDMTX__DEFAULT_WIDTH,
                                                    0,
                                                    0,
                                                    (__data char *)_tmpstr,
                                                    0,
                                                    1,
                                                    0x80,
                                                    ledmtx_scrollstr_stop,
                                                    0x00};

/// Schedule a text for asynchronous scroll (see Timer0 handler), and wait for
/// its finalization.
/// Temporarily use a smaller font s.t. more characters fit the display.
#define SYNC_SCROLL(d)                                                         \
  do {                                                                         \
    ledmtx_setfont(P18CLOCK_FONT_SMALL);                                       \
    ledmtx_scrollstr_start(&d);                                                \
    while (d.str[d.i] != 0 || d.charoff != 0)                                  \
      IDLE();                                                                  \
    ledmtx_scrollstr_reset(&d);                                                \
    ledmtx_setfont(P18CLOCK_FONT_LARGE);                                       \
  } while (0)

#define ENABLE_BUZZER()                                                        \
  do {                                                                         \
    T2CONbits.TMR2ON = 1;                                                      \
    CCP1CON |= 0x0c;                                                           \
  } while (0)

#define DISABLE_BUZZER()                                                       \
  do {                                                                         \
    CCP1CON &= 0xf0;                                                           \
    T2CONbits.TMR2ON = 0;                                                      \
  } while (0)

#define BEEP()                                                                 \
  do {                                                                         \
    ENABLE_BUZZER();                                                           \
    delay10ktcy(1);                                                            \
    DISABLE_BUZZER();                                                          \
  } while (0)

void alarm(void) {
  static unsigned char pstate = 0;

  if ((pstate & 0x04) == 0) {
    if (pstate & 0x01) {
      DISABLE_BUZZER();
    } else {
      ENABLE_BUZZER();
    }
  }
  pstate++;
  pstate &= 0x07;
}

/* `idle_alarm()` counter overflows in ~0.125s */
#define IDLE_ALARM__COUNTER_PRELOAD 0xa9
void idle_alarm(void) {
  static unsigned char counter = IDLE_ALARM__COUNTER_PRELOAD;

  if (_alarm.ena && _alarm.hour == _time.hour && _alarm.min == _time.min) {
    _alarm.ena = 0;
    _alarm.nack = 1;
  }
  if (_alarm.nack && (++counter == 0)) {
    counter = IDLE_ALARM__COUNTER_PRELOAD;
    alarm();
  }
}

#define ACK_ALARM()                                                            \
  do {                                                                         \
    DISABLE_BUZZER();                                                          \
    _alarm.nack = 0;                                                           \
  } while (0)

/// The default 'idle' routine, that checks also if the alarm should be
/// triggered
#define IDLE_ROUTINE() idle_alarm()
#define IDLE()                                                                 \
  do {                                                                         \
    IDLE_ROUTINE();                                                            \
    Sleep();                                                                   \
  } while (0)

#define ALTERNATE(arg1, arg2)                                                  \
  ((TMR1H & 0x40) && (arg != CLASS_INPUT) ? arg1 : arg2)

/// `S_time()` helper to draw alarm-enabled indicator near the right edge if
/// needed.
#if defined(__P18CLOCK_LARGE_DISPLAY)
void __S_time_draw_alarm_indicator(char arg, __data char *input) /* __wparam */
{
  static unsigned char alarm_was_enabled = UNDEF;
  if (input != NULL && *input == B_MODE) {
    alarm_was_enabled = UNDEF; // reset before transition to another main state
    return;
  }
  if (_alarm.ena != alarm_was_enabled) {
    ledmtx_putchar(LEDMTX_PUTCHAR_CPY, /*mask=*/0xfc, /*x=*/34, /*y=*/0,
                   _alarm.ena ? 0x17 : 0x20);
    alarm_was_enabled = _alarm.ena;
  }
}
#endif

/// ==== Functions that implement behavior for each FSM state ====
/// The `input` parameter carries user input that may trigger a state change.
/// These functions are also called periodically with input == NULL; in this
/// case, only the display should be refreshed.

/* __wparam uncompatible with function pointers */
void S_time(char arg, __data char *input) /* __wparam */
{
#if defined(__P18CLOCK_LARGE_DISPLAY)
  __S_time_draw_alarm_indicator(arg, input);
#endif
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE("%02hu %02hu", "%02hu:%02hu"), _time.hour, _time.min);
  } else if (*input == B_MODE) {
    _state = STATE_DATE;

    sprintf(_tmpstr, "  %s", _str_state[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == B_SET) {
    _state = STATE_TIME_SETHOUR;
  }
}

void S_date(char arg, __data char *input) /* __wparam */
{
  (void)arg;
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    switch (_time.sec & 0x03) {
    case 0:
      printf(" %s ", _str_day[day_of_week(_time.mday, _time.mon, _time.year)]);
      break;

    case 1:
      printf("  %02hu ", _time.mday);
      break;

    case 2:
      printf(" %s ", _str_month[_time.mon - 1]);
      break;

    case 3:
      printf(" %04u", _time.year);
      break;
    }
  } else if (*input == B_MODE) {
    _state = STATE_TEMP;

    sprintf(_tmpstr, "  %s", _str_state[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == B_SET) {
    _state = STATE_DATE_SETMON;
  }
}

void S_temp(char arg, __data char *input) /* __wparam */
{
  (void)arg;
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(STR_FMT_TEMP, _temperature);
  } else if (*input == B_MODE) {
    _state = STATE_ALARM;

    sprintf(_tmpstr, "  %s", _str_state[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == B_SET) {
    _state = STATE_TEMP_SETVDD;
  }
}

void S_alarm(char arg, __data char *input) /* __wparam */
{
  (void)arg;
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    if (_alarm.ena) {
      printf("%02hu:%02hu", _alarm.hour, _alarm.min);
    } else {
      printf("--:--");
    }
  } else if (*input == B_MODE) {
    _state = STATE_AUTO;

    sprintf(_tmpstr, "  %s", _str_state[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == B_SET) {
    _state = STATE_ALARM_SETHOUR;
  }
}

void S_auto(char arg, __data char *input) /* __wparam */
{
  static unsigned char vscroll_sched_at_min = UNDEF;
  static unsigned char step = 0U;

#if defined(__P18CLOCK_LARGE_DISPLAY)
  __S_time_draw_alarm_indicator(arg, input);
#endif
  // Any user input other than B_MODE may be used to manually change viewport.
  if (input == (__data char *)NULL || *input != B_MODE) {
    if (vscroll_sched_at_min == UNDEF)
      vscroll_sched_at_min = _time.min;
    if (step == 0U && (!input && _time.min != vscroll_sched_at_min)) {
      S_time(arg, input);
      return;
    }

    LEDMTX_HOME();
    printf(STR_FMT_AUTO, _time.hour, _time.min,
           _str_day[day_of_week(_time.mday, _time.mon, _time.year)], _time.mday,
           _str_month[_time.mon - 1], _temperature);
    // Simulate vertical scroll by changing display viewport.
    const unsigned char viewport_y = (step & 0x1f);
#if defined(__P18CLOCK_LARGE_DISPLAY)
    if (viewport_y <= 27)
#else
    if (viewport_y <= 24)
#endif /* __P18CLOCK_LARGE_DISPLAY */
      ledmtx_setviewport(0, viewport_y, LEDMTX__DEFAULT_WIDTH,
                         LEDMTX_VIEWPORT_HEIGHT);

    step = (step + 1) & 0x3f;
    if (step == 0U) {
      ledmtx_setviewport(0, 0, LEDMTX__DEFAULT_WIDTH, LEDMTX_VIEWPORT_HEIGHT);
      vscroll_sched_at_min = (_time.min + AUTO_INTERVAL) % 60;
    }
  } else {
    ledmtx_setviewport(0, 0, LEDMTX__DEFAULT_WIDTH, LEDMTX_VIEWPORT_HEIGHT);
    vscroll_sched_at_min = UNDEF;

    _state = STATE_TIME;
    sprintf(_tmpstr, "  %s", _str_state[_state]);
    SYNC_SCROLL(_scroll_desc);
  }
}

void S_time_sethour(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE("  :", "%02hu:"), _time.hour);
    printf("%02hu", _time.min);
  } else if (*input == B_SET) {
    _state = STATE_TIME_SETMIN;

    LEDMTX_HOME();
    printf("%02hu:", _time.hour);
  } else if (*input == B_UP) {
    _time.hour = (_time.hour == 23) ? 0 : (_time.hour + 1);
  } else if (*input == B_DOWN) {
    _time.hour = (_time.hour == 0) ? 23 : (_time.hour - 1);
  }
}

void S_time_setmin(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_GOTO((ledmtx_font_sz_w + 1) * 2, 0);
    printf(ALTERNATE(":  ", ":%02hu"), _time.min);
  } else if (*input == B_SET) {
    _state = STATE_TIME;

    _time.sec = 0;
  } else if (*input == B_UP) {
    _time.min = (_time.min == 59) ? 0 : (_time.min + 1);
  } else if (*input == B_DOWN) {
    _time.min = (_time.min == 0) ? 59 : (_time.min - 1);
  }
}

void S_date_setmday(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(STR_FMT_MDAY "  ", STR_FMT_MDAY "%02hu"), _time.mday);
  } else if (*input == B_SET) {
    _state = STATE_DATE;
  } else if (*input == B_UP) {
    _time.mday =
        (_time.mday == _days_per_month[_time.mon - 1]) ? 1 : (_time.mday + 1);
  } else if (*input == B_DOWN) {
    _time.mday =
        (_time.mday == 1) ? _days_per_month[_time.mon - 1] : (_time.mday - 1);
  }
}

void S_date_setmon(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(STR_FMT_MONTH "   ", STR_FMT_MONTH "%s"),
           _str_month[_time.mon - 1]);
  } else if (*input == B_SET) {
    _state = STATE_DATE_SETYEAR;
  } else if (*input == B_UP) {
    _time.mon = (_time.mon == 12) ? 1 : (_time.mon + 1);
  } else if (*input == B_DOWN) {
    _time.mon = (_time.mon == 1) ? 12 : (_time.mon - 1);
  }
}

void S_date_setyear(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(STR_FMT_YEAR "    ", STR_FMT_YEAR "%04u"), _time.year);
  } else if (*input == B_SET) {
    _state = STATE_DATE_SETMDAY;

    _days_per_month[1] = (LEAP_YEAR(_time.year) ? 29 : 28);
    if (_time.mday > _days_per_month[_time.mon - 1])
      _time.mday = _days_per_month[_time.mon - 1];
  } else if (*input == B_UP) {
    _time.year = (_time.year == YEAR_MAX) ? YEAR_MIN : (_time.year + 1);
  } else if (*input == B_DOWN) {
    _time.year = (_time.year == YEAR_MIN) ? YEAR_MAX : (_time.year - 1);
  }
}

void S_temp_setvdd(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(STR_FMT_VDD "   ", STR_FMT_VDD "%03u"), _lm35vdd);
  } else if (*input == B_SET) {
    _state = STATE_TEMP_SETOFF;
  } else if (*input == B_UP) {
    _lm35vdd = (_lm35vdd > 500) ? 270 : (_lm35vdd + 10);
  } else if (*input == B_DOWN) {
    _lm35vdd = (_lm35vdd < 270) ? 500 : (_lm35vdd - 10);
  }
}

void S_temp_setoff(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(STR_FMT_OFF "   ", STR_FMT_OFF "%02d"), _lm35off);
  } else if (*input == B_SET) {
    _state = STATE_TEMP;
  } else if (*input == B_UP) {
    _lm35off = (_lm35off == 10) ? -10 : (_lm35off + 1);
  } else if (*input == B_DOWN) {
    _lm35off = (_lm35off == -10) ? 10 : (_lm35off - 1);
  }
}

void S_alarm_sethour(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE("  :", "%02hu:"), _alarm.hour);
  } else if (*input == B_MODE) {
    _state = STATE_ALARM;

    _alarm.ena = 0;
  } else if (*input == B_SET) {
    _state = STATE_ALARM_SETMIN;

    LEDMTX_HOME();
    printf("%02hu:", _alarm.hour);
  } else if (*input == B_UP) {
    _alarm.hour = (_alarm.hour == 23) ? 0 : (_alarm.hour + 1);
  } else if (*input == B_DOWN) {
    _alarm.hour = (_alarm.hour == 0) ? 23 : (_alarm.hour - 1);
  }
}

void S_alarm_setmin(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_GOTO((ledmtx_font_sz_w + 1) * 2, 0);
    printf(ALTERNATE(":  ", ":%02hu"), _alarm.min);
  } else if (*input == B_MODE) {
    _state = STATE_ALARM;

    _alarm.ena = 0;
  } else if (*input == B_SET) {
    _state = STATE_ALARM;

    _alarm.ena = 1;
  } else if (*input == B_UP) {
    _alarm.min = (_alarm.min == 59) ? 0 : (_alarm.min + 1);
  } else if (*input == B_DOWN) {
    _alarm.min = (_alarm.min == 0) ? 59 : (_alarm.min - 1);
  }
}

typedef void (*state_func_t)(char, __data char *) /* __wparam */;

static state_func_t __code _state_fn[] = {
    S_time,          S_date,         S_temp,        S_alarm,
    S_auto,          S_time_sethour, S_time_setmin, S_date_setmday,
    S_date_setmon,   S_date_setyear, S_temp_setvdd, S_temp_setoff,
    S_alarm_sethour, S_alarm_setmin};

void main(void) {
  static signed char c;
  static unsigned char rcounter = 0xff;
  static unsigned char temp_measurement_sched_sec = UNDEF;

  /* initialisation */
  uc_init();
  ledmtx_setfont(P18CLOCK_FONT_LARGE);
  stdout = STREAM_USER;
  BEEP();

  sprintf(_tmpstr, STR_FMT_INIT, *((__code unsigned char *)__IDLOC0),
          *((__code unsigned char *)__IDLOC1),
          *((__code unsigned char *)__IDLOC2), LEDMTX_GITBRANCH);
  SYNC_SCROLL(_scroll_desc);

  temp_measurement_sched_sec = (_time.sec + TEMPERATURE_INTERVAL) % 60;
  /* main loop */
  while (1) {
    if (_time.sec == temp_measurement_sched_sec) {
      _temperature = lm35_get();
      temp_measurement_sched_sec = (_time.sec + TEMPERATURE_INTERVAL) % 60;
    }

    if (++rcounter == 0) {
      rcounter = 0x51; // Should overflow in roughly 0.25s
      _state_fn[_state](MESSAGE_CLASS(c), NULL);
    }
    c = rbuf_get(_mbuf);
    if (c != -1) {
      switch (c) {
      case YEARCHG:
        _days_per_month[1] = (LEAP_YEAR(_time.year) ? 29 : 28);
#ifdef STR_FMT_YEARCHG
        sprintf(_tmpstr, STR_FMT_YEARCHG, _time.year);
        SYNC_SCROLL(_scroll_desc);
#endif
        break;

      case B_MODE:
      case B_SET:
      case B_UP:
      case B_DOWN:
        if (_alarm.nack) {
          ACK_ALARM();
        } else {
          _state_fn[_state](MESSAGE_CLASS(c), &c);
          rcounter = 0xff; // Forces refresh on next iteration
        }
        break;
      }
    }

    IDLE();
  }
}
