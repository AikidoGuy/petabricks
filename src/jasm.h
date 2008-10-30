/***************************************************************************
 *   Copyright (C) 2008 by Jason Ansel                                     *
 *   jansel@csail.mit.edu                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef JALIBJASM_H
#define JALIBJASM_H

#include <stdint.h>

namespace jalib {

/**
 * Thread safe add, returns old value
 */
template<long v> long atomicAdd(volatile long *p)
{
  long r;
  asm volatile ("lock; xadd %0, %1" : "=r"(r), "=m"(*p) : "0"(v), "m"(*p) : "memory");
  return r+v;
}

/**
 * Break into debugger
 */
inline void Breakpoint(){
  asm volatile ( "int3" );
}


/** 
 * Returns the number of clock cycles that have passed since the machine
 * booted up.
 */
inline uint64_t ClockCyclesSinceBoot()
{
  uint32_t hi, lo;
  asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t ) lo) | (((uint64_t) hi) << 32);
}


}

#endif 
