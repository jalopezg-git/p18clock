/*
 * lang.h - language-dependent string literals
 *
 * Copyright (C) 2011-2024  Javier Lopez-Gomez
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

/* English (EN) */
#if defined(LANG_EN)
static __code const char *__code _str_month[] = {"Jan", "Feb", "Mar", "Apr",
                                                 "May", "Jun", "Jul", "Aug",
                                                 "Sep", "Oct", "Nov", "Dec"};

static __code const char *__code _str_day[] = {"Sun", "Mon", "Tue", "Wed",
                                               "Thu", "Fri", "Sat"};

static __code const char *__code _str_state[] = {"TIME", "DATE", "TEMPERATURE",
                                                 "ALARM", "AUTO"};

#define STR_FMT_TEMP " %02u'C"
#define STR_FMT_MDAY "day"
#define STR_FMT_MONTH "m "
#define STR_FMT_YEAR "y"
#define STR_FMT_VDD "V "
#define STR_FMT_OFF "of"
#define STR_FMT_YEARCHG "Happy %u!"

/* Spanish (ES) */
#elif defined(LANG_ES)
static __code const char *__code _str_month[] = {"Ene", "Feb", "Mar", "Abr",
                                                 "May", "Jun", "Jul", "Ago",
                                                 "Sep", "Oct", "Nov", "Dic"};

static __code const char *__code _str_day[] = {"Dom", "Lun", "Mar", "Mie",
                                               "Jue", "Vie", "Sab"};

static __code const char *__code _str_state[] = {"HORA", "FECHA", "TEMPERATURA",
                                                 "ALARMA", "AUTO"};

#define STR_FMT_TEMP " %02u'C"
#define STR_FMT_MDAY "dia"
#define STR_FMT_MONTH "m "
#define STR_FMT_YEAR "a"
#define STR_FMT_VDD "V "
#define STR_FMT_OFF "of"
#define STR_FMT_YEARCHG "Feliz %u!"

#else
#error Unsupported language

#endif

#define STR_FMT_INIT "p18clock-%hu.%hu.%hu (libledmtx %s)"
#define STR_FMT_AUTO "%02hu:%02hu\n %s\n%2hu%s\n" STR_FMT_TEMP

#endif // __LANG_H__
