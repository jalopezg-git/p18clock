/*
 * lang.h - language dependent definitions
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

#ifndef __LANG_H__
#define __LANG_H__

#if defined(LANG_EN)
static __code const char *__code _month[] = {"Jan", "Feb", "Mar", "Apr",
                                             "May", "Jun", "Jul", "Aug",
                                             "Sep", "Oct", "Nov", "Dec"};

static __code const char *__code _day[] = {"Sun", "Mon", "Tue", "Wed",
                                           "Thu", "Fri", "Sat"};

static __code const char *__code _mainstate[] = {"TIME", "DATE", "TEMPERATURE",
                                                 "ALARM", "AUTO"};

#define AUTOF "  AUTO<%s>"
#define INITF "p18clock firmware/%hu.%hu.%hu"
#define TEMPF " %02u C"
#define MDAYF "day"
#define MONF "m "
#define YEARF "y"
#define VDDF "V "
#define OFFF "of"

#elif defined(LANG_ES)
static __code const char *__code _month[] = {"Ene", "Feb", "Mar", "Abr",
                                             "May", "Jun", "Jul", "Ago",
                                             "Sep", "Oct", "Nov", "Dic"};

static __code const char *__code _day[] = {"Dom", "Lun", "Mar", "Mie",
                                           "Jue", "Vie", "Sab"};

static __code const char *__code _mainstate[] = {"HORA", "FECHA", "TEMPERATURA",
                                                 "ALARMA", "AUTO"};

#define AUTOF "  AUTO<%s>"
#define INITF "p18clock firmware/%hu.%hu.%hu"
#define TEMPF " %02u C"
#define MDAYF "dia"
#define MONF "m "
#define YEARF "a"
#define VDDF "V "
#define OFFF "of"

#else
#error Unsupported language

#endif

#endif // __LANG_H__
