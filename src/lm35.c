/*
 * lm35.c - LM35-related routines
 *
 * Copyright (C) 2011-2023  Javier Lopez-Gomez
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

#include <adc.h>
#include <delay.h>
#include <pic18fregs.h>

static __code int _hlvdvdd[] = {217, 223, 236, 244, 260, 279, 289, 312,
                                339, 355, 371, 390, 411, 433, 459, 485};

#define HLVD_DELAY() delay10tcy(0x33)

int lm35_vdd(void) {
  char limit;

  for (limit = 0x00; limit < 0x0f; limit++) {
    HLVDCON = 0x00;
    HLVDCON |= limit;
    HLVDCONbits.VDIRMAG = 1;
    HLVDCONbits.HLVDEN = 1;
    PIR2bits.HLVDIF = 0;

    while (!HLVDCONbits.IRVST)
      Nop();
    HLVD_DELAY(); /* wait interrupt */

    if (!PIR2bits.HLVDIF)
      break;
  }
  HLVDCONbits.HLVDEN = 0;
  return _hlvdvdd[limit];
}

/// The input voltage, as approximated by the `lm34_vdd()` function
int _lm35vdd;
/// An offset that is applied by `lm35_get()`, e.g. to compensate for the
/// specific physical location of the LM35 part
int _lm35off = 0;

void lm35_init(void) {
  ADCON1 = (ADC_CFG_1A | ADC_VCFG_VDD_VSS);
  ADCON0 = (ADC_CHN_0 << 2);
  ADCON2 = (ADC_FRM_RJUST | ADC_ACQT_8 | ADC_FOSC_8);
  ADCON0bits.ADON = 1;

  // FIXME: adc_open() looks broken for p18f2550?
  /* adc_open(ADC_CHN_0, ADC_FOSC_8|ADC_ACQT_8, ADC_CFG_1A,
              ADC_FRM_RJUST|ADC_VCFG_VDD_VSS); */

  _lm35vdd = lm35_vdd();
}

int lm35_get(void) {
  adc_conv();
  while (adc_busy())
    Nop();
  return ((unsigned int)adc_read() * _lm35vdd / 0x3ff) + _lm35off;
}
