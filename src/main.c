/*
 * main.c - p18clock source file
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

// clang-format off
#include <ledmtx_scrollstr.h>
#include <ledmtx_core.h>
#include <ledmtx_font5x7.h>
#include <ledmtx_stdio.h>
#include <signal.h>
#include <stdio.h>
// clang-format on

#include <lang.h>
#include <lm35.h>
#include <rbuf.h>

#pragma config XINST=OFF

__CONFIG(__CONFIG1L, _USBPLL_CLOCK_SRC_FROM_OSC1_OSC2_1L
                         &_CPUDIV__OSC1_OSC2_SRC___1__96MHZ_PLL_SRC___2__1L
                             &_PLLDIV_DIVIDE_BY_2__8MHZ_INPUT__1L);
__CONFIG(__CONFIG1H, _OSC_HS__USB_HS_1H);
__CONFIG(__CONFIG2L, _BODEN_CONTROLLED_WITH_SBOREN_BIT_2L &_BODENV_2_7V_2L);
__CONFIG(__CONFIG2H, _WDT_DISABLED_CONTROLLED_2H);
__CONFIG(
    __CONFIG3H,
    _CCP2MUX_RC1_3H &_PBADEN_PORTB_4_0__CONFIGURED_AS_DIGITAL_I_O_ON_RESET_3H
        &_LPT1OSC_OFF_3H &_MCLRE_MCLR_ON_RE3_OFF_3H);
__CONFIG(__CONFIG4L, _STVR_ON_4L &_LVP_OFF_4L &_ENHCPU_OFF_4L &_BACKBUG_OFF_4L);

/* IDLOC0:IDLOC1:IDLOC2 -> firmware version */
__CONFIG(__IDLOC0, 0);
__CONFIG(__IDLOC1, 2);
__CONFIG(__IDLOC2, 2);

LEDMTX_BEGIN_MODULES_INIT
LEDMTX_MODULE_INIT(scrollstr)
LEDMTX_END_MODULES_INIT

LEDMTX_FRAMEBUFFER_RES(28)

#define CLASS_INPUT 0x00
#define CLASS_RTC 0x40

#define MESSAGE_CLASS(m) (m & 0xc0)

#define YEARCHG (CLASS_RTC | 0x00)
#define PUSHB1 (CLASS_INPUT | 0x08)
#define PUSHB2 (CLASS_INPUT | 0x0a)
#define PUSHB3 (CLASS_INPUT | 0x0c)
#define PUSHB4 (CLASS_INPUT | 0x0e)

DECLARE_RBUF(_mbuf, 32)

DEF_INTHIGH(_high_int)
DEF_HANDLER(SIG_TMR1, _tmr1_handler)
DEF_HANDLER(SIG_INT0, _int0_handler)
END_DEF

DEF_INTLOW(_low_int)
DEF_HANDLER(SIG_TMR0, _tmr0_handler)
END_DEF

static unsigned char _monthdays[] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};

#define LEAP_YEAR(year)                                                        \
  ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0))

volatile struct {
  unsigned char sec;
  unsigned char min;
  unsigned char hour;
  unsigned char mday;
  unsigned char mon;
  unsigned int year;
} _time = {0, 0, 0, 1, 1, 1970};

static struct {
  unsigned ena : 1;
  unsigned nack : 1;
  unsigned : 6;
  unsigned char hour;
  unsigned char min;
} _alarm = {0, 0, 0, 0};

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
  lfsr		0, __monthdays
  banksel	__time+4
  decf		__time+4, w
  movf		_PLUSW0, w, 0		; _monthdays[_time.mon-1] -> WREG
  banksel	__time+3
  incf		__time+3, f		; _time.mday++
  cpfsgt	__time+3
  bra		@tmr1_ret
  
  ; _time.mday overflow
  movlw		1
  movwf		__time+3		; _time.mday=1
  banksel	__time+4
  incf		__time+4, f		; _time.mon++
  movlw		12
  cpfsgt	__time+4
  bra		@tmr1_ret
  
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
  
@tmr1_ret:
  movff		_PREINC1, _FSR0L	; pop FSR0x
  movff		_PREINC1, _FSR0H
  retfie	1
  __endasm;
}
// clang-format on

#define INT0_UNMASK_TIMEOUT 7

static volatile unsigned char _int0_timeout;

// clang-format off
SIGHANDLERNAKED(_int0_handler)
{
  __asm
  bcf		_INTCON, 1, 0		; ack interrupt
  btfsc		_INTCON2, 6, 0		; INTEDG0 falling edge?
  bra		@int0_rising
  
  movff		_FSR0H, _POSTDEC1	; push FSR0x
  movff		_FSR0L, _POSTDEC1
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
  movff		_PREINC1, _FSR0L	; pop FSR0x
  movff		_PREINC1, _FSR0H
  
@int0_rising:
  bcf		_INTCON, 4, 0		; mask INT0 interrupt
  btg		_INTCON2, 6, 0		; toggle edge
  
  movlw		INT0_UNMASK_TIMEOUT	; INT0 unmasking is done in _tmr0_handler
  banksel	__int0_timeout
  movwf		__int0_timeout
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
  bra		@int0_unmasked
  
  movff		_BSR, _POSTDEC1		; push BSR
  banksel	__int0_timeout
  dcfsnz	__int0_timeout, f	; _int0_timeout--
  bsf		_INTCON, 4, 0		; _int0_timeout==0? unmask INT0 interrupt
  movff		_PREINC1, _BSR		; pop BSR
  
@int0_unmasked:
  __endasm;
  
  LEDMTX_END_ISR
}
// clang-format on

/* PWM 2.44khz@50 (Fosc=8mhz) */
#define CCP1_PR2 0xcb
#define CCP1_R 0x066
#define CCP1_T2PS 0x01

void uc_init(void) {
  OSCCONbits.IDLEN = 1; /* sleep -> CPU idle */

  TRISA = 0xc1;
  PORTA = 0x00;
  TRISB = 0x0f;
  PORTB = 0x00;
  TRISC = 0xfb;
  PORTC = 0x00;
  INTCON2bits.RBPU = 1; /* disable RBx pullup */

  lm35_init();

  /* TMR1 RTC, see p18f2550 datasheet */
  TMR1H = 0x80;
  TMR1L = 0x00;
  T1CON = 0x0f;
  PIE1bits.TMR1IE = 1;

  PR2 = CCP1_PR2;
  CCPR1L = (CCP1_R >> 2);
  CCP1CON = (CCP1_R & 0x03) << 4;
  T2CON = (CCP1_T2PS & 0x03);

  ledmtx_init(LEDMTX_INIT_CLEAR | LEDMTX_INIT_TMR0, 32, 7, 0xe9, 0xae,
              0x88); /* 32x7@50hz (Fosc=8mhz) */

  INTCON2bits.INTEDG0 = 0; /* falling edge */
  INTCONbits.INT0IE = 1;
}

/* Tomohiko Sakamoto day_of_week routine */
char day_of_week(unsigned char mday, unsigned char mon, unsigned int year) {
  static const char t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  year -= mon < 3;
  return (year + (year / 4) - (year / 100) + (year / 400) + t[mon - 1] + mday) %
         7;
}

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

static unsigned char _state = STATE_TIME;

static char _tmpstr[32];
static struct ledmtx_scrollstr_desc _scroll_desc = {
    1, 1,    ledmtx_scrollstr_step, 32,  0, 0, (__data char *)_tmpstr, 0,
    1, 0x80, ledmtx_scrollstr_stop, 0x00};

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

/* idle_alarm counter overflows in 0.125s */
#define IDLE_ALARM__COUNTER_PRELOAD 0xd4

void idle_alarm(void) {
  static unsigned char counter = IDLE_ALARM__COUNTER_PRELOAD;

  /* trigger alarm? */
  if (_alarm.ena && _alarm.hour == _time.hour && _alarm.min == _time.min) {
    _alarm.ena = 0;
    _alarm.nack = 1;
  }
  if (_alarm.nack && (++counter == 0)) {
    counter = IDLE_ALARM__COUNTER_PRELOAD;
    alarm();
  }
}

#define IDLE_ROUTINE() idle_alarm()
#define IDLE()                                                                 \
  do {                                                                         \
    IDLE_ROUTINE();                                                            \
    Sleep();                                                                   \
  } while (0)

#define SYNC_SCROLL(d)                                                         \
  do {                                                                         \
    ledmtx_scrollstr_start(&d);                                                \
    while (d.str[d.i] != 0 || d.charoff != 0)                                  \
      IDLE();                                                                  \
    ledmtx_scrollstr_reset(&d);                                                \
  } while (0)

#define ALTERNATE(arg1, arg2)                                                  \
  ((TMR1H & 0x40) && (arg != CLASS_INPUT) ? arg1 : arg2)

/* state machine routines */

/* __wparam uncompatible with function pointers */
void S_time(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE("%02hu %02hu", "%02hu:%02hu"), _time.hour, _time.min);
  } else if (*input == PUSHB1) {
    _state = STATE_DATE;

    sprintf(_tmpstr, "  %s", _mainstate[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == PUSHB2) {
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
      printf(" %s ", _day[day_of_week(_time.mday, _time.mon, _time.year)]);
      break;

    case 1:
      printf("  %02hu ", _time.mday);
      break;

    case 2:
      printf(" %s ", _month[_time.mon - 1]);
      break;

    case 3:
      printf(" %04u", _time.year);
      break;
    }
  } else if (*input == PUSHB1) {
    _state = STATE_TEMP;

    sprintf(_tmpstr, "  %s", _mainstate[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == PUSHB2) {
    _state = STATE_DATE_SETMON;
  }
}

void S_temp(char arg, __data char *input) /* __wparam */
{
  (void)arg;
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(TEMPF, lm35_get());
  } else if (*input == PUSHB1) {
    _state = STATE_ALARM;

    sprintf(_tmpstr, "  %s", _mainstate[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == PUSHB2) {
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
  } else if (*input == PUSHB1) {
    _state = STATE_AUTO;

    sprintf(_tmpstr, "  %s", _mainstate[_state]);
    SYNC_SCROLL(_scroll_desc);
  } else if (*input == PUSHB2) {
    _state = STATE_ALARM_SETHOUR;
  }
}

void S_auto(char arg, __data char *input) /* __wparam */
{
  static unsigned char pstate = 0;

  if (input == (__data char *)NULL) {
    if ((_time.sec & 0x0f) == 0) {
      pstate = (pstate + 1) & 0x03;

      sprintf(_tmpstr, AUTOF, _mainstate[pstate]);
      SYNC_SCROLL(_scroll_desc);
    }

    switch (pstate) {
    case 0:
      S_time(arg, input);
      break;

    case 1:
      S_date(arg, input);
      break;

    case 2:
      S_temp(arg, input);
      break;

    case 3:
      S_alarm(arg, input);
      break;
    }
  } else if (*input == PUSHB1) {
    _state = STATE_TIME;

    sprintf(_tmpstr, "  %s", _mainstate[_state]);
    SYNC_SCROLL(_scroll_desc);
  }
}

void S_time_sethour(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE("  :", "%02hu:"), _time.hour);
  } else if (*input == PUSHB2) {
    _state = STATE_TIME_SETMIN;

    LEDMTX_HOME();
    printf("%02hu:", _time.hour);
  } else if (*input == PUSHB3) {
    _time.hour = (_time.hour == 23) ? 0 : (_time.hour + 1);
  } else if (*input == PUSHB4) {
    _time.hour = (_time.hour == 0) ? 23 : (_time.hour - 1);
  }
}

void S_time_setmin(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_GOTO(12, 0);
    printf(ALTERNATE(":  ", ":%02hu"), _time.min);
  } else if (*input == PUSHB2) {
    _state = STATE_TIME;

    _time.sec = 0;
  } else if (*input == PUSHB3) {
    _time.min = (_time.min == 59) ? 0 : (_time.min + 1);
  } else if (*input == PUSHB4) {
    _time.min = (_time.min == 0) ? 59 : (_time.min - 1);
  }
}

void S_date_setmday(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(MDAYF "  ", MDAYF "%02hu"), _time.mday);
  } else if (*input == PUSHB2) {
    _state = STATE_DATE;
  } else if (*input == PUSHB3) {
    _time.mday =
        (_time.mday == _monthdays[_time.mon - 1]) ? 1 : (_time.mday + 1);
  } else if (*input == PUSHB4) {
    _time.mday =
        (_time.mday == 1) ? _monthdays[_time.mon - 1] : (_time.mday - 1);
  }
}

void S_date_setmon(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(MONF "   ", MONF "%s"), _month[_time.mon - 1]);
  } else if (*input == PUSHB2) {
    _state = STATE_DATE_SETYEAR;
  } else if (*input == PUSHB3) {
    _time.mon = (_time.mon == 12) ? 1 : (_time.mon + 1);
  } else if (*input == PUSHB4) {
    _time.mon = (_time.mon == 1) ? 12 : (_time.mon - 1);
  }
}

void S_date_setyear(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(YEARF "    ", YEARF "%04u"), _time.year);
  } else if (*input == PUSHB2) {
    _state = STATE_DATE_SETMDAY;

    _monthdays[1] = (LEAP_YEAR(_time.year) ? 29 : 28);
    if (_time.mday > _monthdays[_time.mon - 1])
      _time.mday = _monthdays[_time.mon - 1];
  } else if (*input == PUSHB3) {
    _time.year = (_time.year == 2038) ? 1970 : (_time.year + 1);
  } else if (*input == PUSHB4) {
    _time.year = (_time.year == 1970) ? 2038 : (_time.year - 1);
  }
}

void S_temp_setvdd(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(VDDF "   ", VDDF "%03u"), _lm35vdd);
  } else if (*input == PUSHB2) {
    _state = STATE_TEMP_SETOFF;
  } else if (*input == PUSHB3) {
    _lm35vdd = (_lm35vdd > 500) ? 270 : (_lm35vdd + 10);
  } else if (*input == PUSHB4) {
    _lm35vdd = (_lm35vdd < 270) ? 500 : (_lm35vdd - 10);
  }
}

void S_temp_setoff(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE(OFFF "   ", OFFF "%02d"), _lm35off);
  } else if (*input == PUSHB2) {
    _state = STATE_TEMP;
  } else if (*input == PUSHB3) {
    _lm35off = (_lm35off == 10) ? -10 : (_lm35off + 1);
  } else if (*input == PUSHB4) {
    _lm35off = (_lm35off == -10) ? 10 : (_lm35off - 1);
  }
}

void S_alarm_sethour(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_HOME();
    printf(ALTERNATE("  :", "%02hu:"), _alarm.hour);
  } else if (*input == PUSHB1) {
    _state = STATE_ALARM;

    _alarm.ena = 0;
  } else if (*input == PUSHB2) {
    _state = STATE_ALARM_SETMIN;

    LEDMTX_HOME();
    printf("%02hu:", _alarm.hour);
  } else if (*input == PUSHB3) {
    _alarm.hour = (_alarm.hour == 23) ? 0 : (_alarm.hour + 1);
  } else if (*input == PUSHB4) {
    _alarm.hour = (_alarm.hour == 0) ? 23 : (_alarm.hour - 1);
  }
}

void S_alarm_setmin(char arg, __data char *input) /* __wparam */
{
  if (input == (__data char *)NULL) {
    LEDMTX_GOTO(12, 0);
    printf(ALTERNATE(":  ", ":%02hu"), _alarm.min);
  } else if (*input == PUSHB1) {
    _state = STATE_ALARM;

    _alarm.ena = 0;
  } else if (*input == PUSHB2) {
    _state = STATE_ALARM;

    _alarm.ena = 1;
  } else if (*input == PUSHB3) {
    _alarm.min = (_alarm.min == 59) ? 0 : (_alarm.min + 1);
  } else if (*input == PUSHB4) {
    _alarm.min = (_alarm.min == 0) ? 59 : (_alarm.min - 1);
  }
}

typedef void (*state_func_t)(char, __data char *) /* __wparam */;

static state_func_t __code _state_fn[] = {
    S_time,          S_date,         S_temp,        S_alarm,
    S_auto,          S_time_sethour, S_time_setmin, S_date_setmday,
    S_date_setmon,   S_date_setyear, S_temp_setvdd, S_temp_setoff,
    S_alarm_sethour, S_alarm_setmin};

#define ACK_ALARM()                                                            \
  do {                                                                         \
    DISABLE_BUZZER();                                                          \
    _alarm.nack = 0;                                                           \
  } while (0)

/* rcounter overflow in 0.5s */
#define RCOUNTER_PRELOAD 0x51

void main(void) {
  static char c;
  static unsigned char rcounter = 0xff;

  /* initialisation */
  uc_init();
  ledmtx_setfont(ledmtx_font5x7);
  stdout = STREAM_USER;

  sprintf(_tmpstr, INITF, *((__code unsigned char *)__IDLOC0),
          *((__code unsigned char *)__IDLOC1),
          *((__code unsigned char *)__IDLOC2));
  SYNC_SCROLL(_scroll_desc);

  /* main loop */
  while (1) {
    if (++rcounter == 0) {
      rcounter = RCOUNTER_PRELOAD;
      _state_fn[_state](MESSAGE_CLASS(c), NULL); /* NULL -> refresh display */
    }

    c = rbuf_get(_mbuf);
    if (c != -1) {
      switch (c) {
      case YEARCHG:
        _monthdays[1] = (LEAP_YEAR(_time.year) ? 29 : 28);
        break;

      case PUSHB1:
      case PUSHB2:
      case PUSHB3:
      case PUSHB4:
        if (_alarm.nack) {
          ACK_ALARM();
        } else {
          _state_fn[_state](MESSAGE_CLASS(c), &c); /* input to state machine */
          rcounter = 0xff;                         /* force refresh */
        }
        break;
      }
    }

    IDLE();
  }
}
