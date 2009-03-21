/***************************************************************************
 *   Copyright (C) 2008 by Jason Ansel                                     *
 *   jansel@csail.mit.edu                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
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
#include "dynamictask.h"
#include "jasm.h"
#include "jtunable.h"

#include <pthread.h>

//#define PBCC_SEQUENTIAL
#define INLINE_NULL_TASKS

#define MIN_NUM_WORKERS  0
#define MAX_NUM_WORKERS  512
#define MIN_INLINE_TASK_SIZE  1
#define MAX_INLINE_TASK_SIZE  65536

JTUNABLE(tunerNumOfWorkers,   8, MIN_NUM_WORKERS, MAX_NUM_WORKERS);
JTUNABLE(tunerInlineTaskSize, 1024, MIN_INLINE_TASK_SIZE, MAX_INLINE_TASK_SIZE);

namespace petabricks {

DynamicScheduler *DynamicTask::scheduler = NULL;
size_t            DynamicTask::firstSize = 0;
size_t            DynamicTask::maxSize   = 0;

DynamicTask::DynamicTask()
{
  // when this task is created, no other thread would touch it
  // so no lock for numOfPredecessor update
  state = S_NEW;
  numOfPredecessor = 0;

#ifndef PBCC_SEQUENTIAL
  // allocate scheduler when the first task is created
  if(scheduler == NULL) {
    scheduler = new DynamicScheduler();
    scheduler->startWorkerThreads(tunerNumOfWorkers);
  }
#endif
}


#ifdef PBCC_SEQUENTIAL
void DynamicTask::enqueue() { run();}
#else
void DynamicTask::enqueue()
{
  int preds;
  {
    JLOCKSCOPE(lock);
    preds=numOfPredecessor;
    if(preds==0)
      state=S_READY;
    else
      state=S_PENDING;
  }
  if(preds==0){
    inlineOrEnqueueTask();
  }
}
#endif // PBCC_SEQUENTIAL


#ifdef PBCC_SEQUENTIAL
void DynamicTask::dependsOn(const DynamicTaskPtr &that){}
#else
void DynamicTask::dependsOn(const DynamicTaskPtr &that)
{
  if(!that) return;
  JASSERT(that!=this).Text("task cant depend on itself");
  JASSERT(state==S_NEW)(state).Text(".dependsOn must be called before enqueue()");
  that->lock.lock();
  if(that->state == S_CONTINUED){
    that->lock.unlock();
    dependsOn(that->continuation);
  }else if(that->state != S_COMPLETE){
    that->dependents.push_back(this);
    { 
      JLOCKSCOPE(lock);
      numOfPredecessor++;
    }
    that->lock.unlock();
  }else{
    that->lock.unlock();
  }
#ifdef VERBOSE
    printf("thread %d: task %p depends on task %p counter: %d\n", pthread_self(), this, that.asPtr(), numOfPredecessor);
#endif  
}
#endif // PBCC_SEQUENTIAL

void petabricks::DynamicTask::decrementPredecessors(){
  bool shouldEnqueue = false;
  {
    JLOCKSCOPE(lock);
    if(--numOfPredecessor==0 && state==S_PENDING){
      state = S_READY;
      shouldEnqueue = true;
    }
  }
  if(shouldEnqueue){
    inlineOrEnqueueTask();
  }
}


void petabricks::DynamicTask::runWrapper(){
  JASSERT(state==S_READY && numOfPredecessor==0)(state)(numOfPredecessor);
  continuation = run();

  std::vector<DynamicTaskPtr> tmp;

  {
    JLOCKSCOPE(lock);
    dependents.swap(tmp);
    if(continuation) state = S_CONTINUED;
    else             state = S_COMPLETE;
  }

  if(continuation){
#ifdef VERBOSE
    JTRACE("task complete, continued")(tmp.size());
#endif
    {
      JLOCKSCOPE(continuation->lock);
      if(continuation->dependents.empty()){
        //swap is faster than insert
        continuation->dependents.swap(tmp);
      }else{
        continuation->dependents.insert(continuation->dependents.end(), tmp.begin(), tmp.end());
      }
    }
    continuation->enqueue();
  }else{
    #ifdef VERBOSE
    if(!isNullTask()) JTRACE("task complete")(tmp.size());
    #endif
    std::vector<DynamicTaskPtr>::iterator it;
    for(it = tmp.begin(); it != tmp.end(); ++it) {
      (*it)->decrementPredecessors();
    }
  }
}


#ifdef PBCC_SEQUENTIAL
void DynamicTask::waitUntilComplete() {}
#else
void DynamicTask::waitUntilComplete()
{
  lock.lock();
  while(state != S_COMPLETE && state!= S_CONTINUED) {
    lock.unlock();
    // get a task for execution
    scheduler->popAndRunOneTask(false);
    lock.lock();
  }
  lock.unlock();
  if(state == S_CONTINUED)
    continuation->waitUntilComplete();
}
#endif // PBCC_SEQUENTIAL


///
/// check if the task should be enqueued of inlined
/// heuristics to check if task should be inlined
/// 1. fixed task size threshold
/// 2. function of input size
/// 3. function of max task size
/// 4. number of items in queue
/// 5. mix of above
bool DynamicTask::inlineTask()
{
  size_t taskSize = size();
  // if too small, inline
  if(isNullTask() || (int)taskSize < tunerInlineTaskSize)
    return true;

  // if large task, do not inline
//   if(maxSize  < taskSize) {
//     maxSize   = taskSize;
//     return false;
//   } 

  // if no tasks in queue, do not inline
  // this is to increase parallelism
//   if(scheduler->empty())
//     return false;

  // if task size is very small relative to max task, inline
//   if(taskSize < (maxSize >> 8))
//     return true;

  return false;
}

void DynamicTask::inlineOrEnqueueTask()
{
#ifdef INLINE_NULL_TASKS
  if(inlineTask()){
//     static __thread int inlineCount=0;
//     if(inlineCount<100){
//      ++inlineCount;
//      runWrapper(); //dont bother enqueuing just run it
//      --inlineCount;
//     }else{
      //push it in local queue
      DynamicScheduler::myThreadLocalQueue().push_back(this);
//     }
  }else
#endif
  {
    scheduler->enqueue(this);
  }
}

}
