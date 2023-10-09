/*
 * rbuf.h - ring buffer routines
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

#ifndef __RBUF_H__
#define __RBUF_H__

/// Declare and initialize a circular buffer.  `size` must be a power of two.
/// The first three octets describe, respectively, the mask applied to `head`
/// and `tail`, and the head (write) and tail (read) indices Note that if `size`
/// is a power of two, `head & mask` is equivalent to `head % size` (the same
/// applies for tail).
#define DECLARE_RBUF(buf, size) char buf[size + 3] = {size - 1, 0x00, 0x00};

/// Insert the value `c` in the circular buffer pointed to by `buf`.
/// Returns 0 on success, or -1 if the circular buffer is full.
extern char rbuf_put(char c, __data char *buf) __wparam __naked;

/// Pop and return an element from the circular buffer pointed to by `buf`.
/// Returns -1 if the buffer is empty.
extern char rbuf_get(__data char *buf) __naked;

#endif // __RBUF_H__
