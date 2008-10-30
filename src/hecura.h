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

#include "matrix.h"
#include "matrixio.h"
#include "dynamictask.h"
#include "hecuraruntime.h"
#include "jtunable.h"
#include <math.h>

#define SPAWN(args...) \
{ DynamicTaskPtr _task = spawn_ ## args; \
  _task->dependsOn(_before);\
  _after->dependsOn(_task);\
  _task->enqueue();\
}

#define SYNC() \
{ \
  _before = _after; \
  _after = new NullDynamicTask(); \
  _before->enqueue(); \
}



