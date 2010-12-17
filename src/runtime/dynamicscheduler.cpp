/***************************************************************************
 *  Copyright (C) 2008-2009 Massachusetts Institute of Technology          *
 *                                                                         *
 *  This source code is part of the PetaBricks project and currently only  *
 *  available internally within MIT.  This code may not be distributed     *
 *  outside of MIT. At some point in the future we plan to release this    *
 *  code (most likely GPL) to the public.  For more information, contact:  *
 *  Jason Ansel <jansel@csail.mit.edu>                                     *
 *                                                                         *
 *  A full list of authors may be found in the file AUTHORS.               *
 ***************************************************************************/
#include "dynamicscheduler.h"

#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

petabricks::DynamicScheduler& petabricks::DynamicScheduler::cpuScheduler(){
  static DynamicScheduler t;
  if(t._rawThreads.empty()){
    // add the main thread
    t._rawThreads.push_back(pthread_self());
  }
  return t;
}
petabricks::DynamicScheduler& petabricks::DynamicScheduler::lookupScheduler(DynamicTask::TaskType t){
  static DynamicScheduler extraSchedulers[DynamicTask::TYPE_COUNT-1];
  //TODO: once we get more than 2 or 3 task types we should convert this to a table
  switch(t){
    case DynamicTask::TYPE_CPU:    return cpuScheduler();  
    case DynamicTask::TYPE_OPENCL: return extraSchedulers[0];  
    default:
    {
      UNIMPLEMENTED();
      return *(DynamicScheduler*)NULL;
    }
  }
}

extern "C" void *workerStartup(void *arg) {
  try {
    JASSERT(arg!=0);
    petabricks::WorkerThread worker(*(petabricks::DynamicScheduler*)arg);
    worker.mainLoop();
  }catch(petabricks::DynamicScheduler::CleanExitException e){}
  return NULL;
}

void petabricks::DynamicScheduler::startWorkerThreads(int total)
{
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, 0);
  JASSERT(numThreads() <= total)(numThreads())(total);
  while(numThreads() < total){
    _rawThreads.push_back(pthread_t());
    JASSERT(pthread_create(&_rawThreads.back(), &attr, workerStartup, this) == 0);
  }
  pthread_attr_destroy(&attr);
}

void petabricks::DynamicScheduler::abort(){
  static jalib::JMutex lock;
  if(lock.trylock()){
    DynamicTaskPtr t = new AbortTask(numThreads(), false);
    try{
      t->run();
    }catch(...){
      lock.unlock();
      throw;
    }
  }else{
    //another thread beat us to the abort()
    WorkerThread* self = WorkerThread::self();
    JASSERT(self!=NULL);
    //cancel all our pending tasks
    DynamicTask* t;
    while((t=self->popLocal())!=NULL)
      t->cancel();
    //wait until the other thread aborts us
    for(;;) self->popAndRunOneTask(STEAL_ATTEMPTS_MAINLOOP);
  }
  JASSERT(false).Text("unreachable");
}

void petabricks::DynamicScheduler::shutdown(){
  if(numThreads()==1)
    return;
  try {
    DynamicTaskPtr t = new AbortTask(numThreads(), true);
    t->run();
  }catch(AbortException e){}
  pthread_t self = pthread_self();
  bool hadSelf = false;
  while(!_rawThreads.empty()){
    if(!pthread_equal(_rawThreads.front(), self)){
      int rv = pthread_join(_rawThreads.front(), NULL);
      JWARNING(rv==0)(rv).Text("pthread_join failed");
    } else {
      hadSelf = true;
    }
    _rawThreads.pop_front();
  }
  if(hadSelf)
    _rawThreads.push_back(self);
}
  

