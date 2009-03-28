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
#ifndef PETABRICKSDYNAMICTASK_H
#define PETABRICKSDYNAMICTASK_H

#include "jrefcounted.h"
#include "jmutex.h"
//#include "dynamicscheduler.h"

#include <vector>
#include <algorithm>

namespace petabricks {

// forward declarsion for DynamicTaskScheduler pointer
class DynamicScheduler;

// forward declarsion for DynamicTaskPtr
class DynamicTask;
typedef jalib::JRef<DynamicTask> DynamicTaskPtr;


class DynamicTask : public jalib::JRefCounted {
public:

  enum TaskState {
    S_NEW,       //after creation
    S_PENDING,   //after enqueue()
    S_READY,     //after all dependencies met
    S_COMPLETE,  //after run()==NULL
    S_CONTINUED  //after run()!=NULL
  };

  ///
  /// indicate if the task is executed or not
  TaskState state;

  bool isContinuation;
  ///
  /// Perform this task, return a continuation task that must be completed
  /// before this task is marked done, otherwise return NULL
  virtual DynamicTaskPtr run() = 0;

  /// constructor
  DynamicTask(bool isCont = false);

  ///
  /// Mark that this may not start before that
  void dependsOn(const DynamicTaskPtr& that);

  ///
  /// Enqueue this task to be run (once all dependencies are met)
  /// this method indicates that all dependencies have been registered
  void enqueue();

  ///
  /// Size in bytes the area of this task
  virtual size_t size() const  { return 0; }

  ///
  /// Block until this task has completed
  void waitUntilComplete();

  ///
  /// Wrapper around run that changes state and handles dependencies
  void runWrapper();

  ///
  /// either enqueue or inline the task
  void inlineOrEnqueueTask();

  ///
  /// Scheduler for scheduling the dynamic tasks
  static DynamicScheduler *scheduler;

 protected:

  ///
  /// a maxSize to remember the largest size
  static size_t maxSize;

  ///
  /// a maxSize to remember the first task size
  static size_t firstSize;


  virtual bool isNullTask() const { return false; }

  ///
  /// mark that a task that we dependOn has completed
  void decrementPredecessors();
 protected:
  ///
  /// a list of tasks that depends on me
  std::vector<DynamicTaskPtr> dependents;

  ///
  /// a mutex lock for manipulating task status and dependents list
  jalib::JCondMutex  lock;

  ///
  /// a counter of how many tasks I depends on
  long numOfPredecessor;

  ///
  /// Pointer to the continuation task
  DynamicTaskPtr continuation;

  char __post_padding[64];
};

class NullDynamicTask : public DynamicTask {
public:
  DynamicTaskPtr run(){ return 0; }
  bool isNullTask() const { return true; }
  char __post_padding[64];
};


template< typename T>
class MethodCallTask : public DynamicTask {
public:
  typedef DynamicTaskPtr (T::*MethodPtr)();
  MethodCallTask(const jalib::JRef<T>& obj, MethodPtr method)
    : _obj(obj), _method(method)
  {}
  DynamicTaskPtr run(){
    return ((*_obj).*(_method))();
  }
private:
  jalib::JRef<T> _obj;
  MethodPtr _method;
  char __post_padding[64];
};


}

#endif
