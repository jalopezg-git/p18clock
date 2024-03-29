/*
 * lm35.h - header for LM35-related routines
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

#ifndef __LM35_H__
#define __LM35_H__

extern int _lm35vdd;
extern int _lm35off;

/// Leverage the HLVD circuitry (Section 24.0 of the P18F2550 datasheet) to
/// probe for the Vdd voltage. The returned value is a fair approximation of the
/// input voltage in units of 0.01v.
extern int lm35_vdd(void);

/// Perform required initialization for measuring the output voltage of an
/// external LM35 temperature sensor.
extern void lm35_init(void);

/// Read the value of an external LM35 temperature sensor.  The returned value
/// is in degrees Celsius.
extern int lm35_get(void);

#endif // __LM35_H__
