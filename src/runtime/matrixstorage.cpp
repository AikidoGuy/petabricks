/*****************************************************************************
 *  Copyright (C) 2008-2011 Massachusetts Institute of Technology            *
 *                                                                           *
 *  Permission is hereby granted, free of charge, to any person obtaining    *
 *  a copy of this software and associated documentation files (the          *
 *  "Software"), to deal in the Software without restriction, including      *
 *  without limitation the rights to use, copy, modify, merge, publish,      *
 *  distribute, sublicense, and/or sell copies of the Software, and to       *
 *  permit persons to whom the Software is furnished to do so, subject       *
 *  to the following conditions:                                             *
 *                                                                           *
 *  The above copyright notice and this permission notice shall be included  *
 *  in all copies or substantial portions of the Software.                   *
 *                                                                           *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY                *
 *  KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE               *
 *  WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND      *
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE   *
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION   *
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION    *
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE           *
 *                                                                           *
 *  This source code is part of the PetaBricks project:                      *
 *    http://projects.csail.mit.edu/petabricks/                              *
 *                                                                           *
 *****************************************************************************/
#include "matrixstorage.h"

#include "petabricksruntime.h"


#define GPU_TRACE 1

MATRIX_ELEMENT_T petabricks::MatrixStorage::rand(){
  return petabricks::PetabricksRuntime::randDouble(-2147483648, 2147483648);
}

void petabricks::MatrixStorage::randomize(){
  for(size_t i=0;i<_count; ++i){
    _data[i] = MatrixStorage::rand();
  }
}

petabricks::MatrixStorageInfo::MatrixStorageInfo(){reset();}
petabricks::MatrixStorageInfo::~MatrixStorageInfo(){
#ifdef HAVE_OPENCL
  if(_hasGpuMem){
#ifdef GPU_TRACE
    std::cout << this << " : release clmem (deconstructor)" << std::endl;
#endif
    clReleaseMemObject(_clmem);
  }
#endif
}

void petabricks::MatrixStorageInfo::setStorage(const MatrixStoragePtr& s, const ElementT* base){
  if(s){
    _storage=s;
    //std::cout << "base = " << base << " data = " << s->data() << " count = " << s->count() << " data + count = " << s->data()+s->count() << std::endl;
    JASSERT(base >= s->data());
    JASSERT(base < s->data()+s->count());
    _baseOffset=base-s->data();
  }else{
    _storage=NULL;
    _baseOffset=0;
  }
  _hash=HashT();
}

void petabricks::MatrixStorageInfo::setSizeMultipliers(int dim, const IndexT* mult, const IndexT* siz){
  _dimensions=dim;
  for(int d=0; d<_dimensions; ++d)
    _multipliers[d]=mult[d];
  _count = 1;
  for(int d=0; d<_dimensions; ++d) {
    _sizes[d]=siz[d];
    _count *= _sizes[d];
  }
}

void petabricks::MatrixStorageInfo::setMultipliers(const IndexT* mult){
  for(int d=0; d<_dimensions; ++d)
    _multipliers[d]=mult[d];
}

void petabricks::MatrixStorageInfo::setExtraVal(ElementT v)
{
  _extraVal=v;
}

void petabricks::MatrixStorageInfo::computeDataHash() { 
  if(_storage) 
    _hash=_storage->hash();
  else
    _hash=HashT();
}

void petabricks::MatrixStorageInfo::reset(){
  setExtraVal();
  setStorage(0,0);
  _dimensions=-1;
#ifdef HAVE_OPENCL
  _hasGpuMem = false;
  _reading = false;
  _refCount = 0;
  _copyBack = 0;
  _coverage = 0;
#endif
}

void petabricks::MatrixStorageInfo::releaseStorage() { _storage=0; }


bool petabricks::MatrixStorageInfo::isMetadataMatch(const MatrixStorageInfo& that) const{
  if(_dimensions != that._dimensions) return false;
  if(_dimensions<0)                   return false;
  if(_baseOffset != that._baseOffset) return false;
  for(int d=0; d<_dimensions; ++d)
    if(_multipliers[d]!=that._multipliers[d])
      return false;
  for(int d=0; d<_dimensions; ++d)
    if(_sizes[d]!=that._sizes[d])
      return false;
  return true;
}

bool petabricks::MatrixStorageInfo::isDataMatch(const MatrixStorageInfo& that) const{
  if(_extraVal != that._extraVal) return false;
  if(_storage && _hash==HashT()) return false;
  return _hash==that._hash;
}

#ifdef HAVE_OPENCL
bool petabricks::MatrixStorageInfo::initGpuMem(cl_context& context) {
  if(!_hasGpuMem) {
    cl_int err;
    //std::cout << this << " : create buffer size = " << bytes() << std::endl;
    _clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, bytes(), NULL, &err);
    //std::cout << "&clmem = " << &_clmem << std::endl;
    //std::cout << "clmem = " << _clmem << std::endl;
    JASSERT(CL_SUCCESS == err).Text("Failed to create input memory object.");
    _hasGpuMem = true;
    return true;
  }
  return false;
}

void petabricks::MatrixStorageInfo::check(cl_command_queue& queue) {
    std::cout << "input: check" << std::endl;
    std::cout << "baseoffset = " << _baseOffset << std::endl;
    print();
    std::cout << "clmem = " << _clmem << std::endl;
    ElementT data[_count];
    clEnqueueReadBuffer(queue, _clmem, CL_TRUE, 0, bytes(), data, 0, NULL, NULL);
    for(int i=0;i<_count;i++)
      std::cout << data[i] << " ";
    std::cout << std::endl << std::endl;
}

void petabricks::MatrixStorageInfo::finishGpuMem(cl_command_queue& queue, int copyBack) {
  /*if(!copyBack) {
    std::cout << "input: finishGpuMem" << std::endl;
    std::cout << "baseoffset = " << _baseOffset << std::endl;
    print();
    std::cout << "clmem = " << _clmem << std::endl;
    ElementT data[_count];
    clEnqueueReadBuffer(queue, _clmem, CL_TRUE, 0, bytes(), data, 0, NULL, NULL);
    for(int i=0;i<_count;i++)
      std::cout << data[i] << " ";
    std::cout << std::endl << std::endl;
  }*/
  if(copyBack > _copyBack) _copyBack = copyBack;
#ifdef GPU_TRACE
  std::cout << this << " : refcount " << _refCount << " : copyback " << _copyBack << " : coverage " << _coverage << "/" << _count << std::endl;
#endif
  if(_refCount == _copyBack) {
#ifdef GPU_TRACE
    std::cout << "read buffer" << std::endl;
#endif
    if(_dimensions == 0 || _storage->count() != _coverage)
      _gpuOutputBuffer = new MatrixStorage(count());
    else
      _gpuOutputBuffer = storage();
    //cl_int err = clSetEventCallBack(event, CL_COMPLETE, NULL, NULL);
    //std::cout << this << " : start read buffer " << _refCount << std::endl;
    clEnqueueReadBuffer(queue, _clmem, CL_FALSE, 0, bytes(), _gpuOutputBuffer->data(), 0, NULL, &event);
    clFlush(queue);
    _reading = true;
  }
}

/*bool petabricks::MatrixStorageInfo::doneReadBuffer() {
  JASSERT(_copyBack).Text("Copying unmodified matrix.");

  if(_refCount == 0) {
    cl_int ret;
    clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &ret, NULL);
#ifdef GPU_TRACE
    std::cout << "status = " << ret << "  queue = " << CL_QUEUED << "  submitted = " << CL_SUBMITTED << "  running = " << CL_RUNNING << "  complete = " << CL_COMPLETE << std::endl;
#endif
    if(ret == CL_COMPLETE) {
      _copyBack = false;
      return true;
    }
  }
  return false;
}*/

bool petabricks::MatrixStorageInfo::doneReadBuffer() {
  if(_reading) {
    JASSERT(_copyBack).Text("Copy something it shouldn't copy.");
    cl_int ret;
    clGetEventInfo(event, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &ret, NULL);
#ifdef GPU_TRACE
    std::cout << "status = " << ret << "  queue = " << CL_QUEUED << "  submitted = " << CL_SUBMITTED << "  running = " << CL_RUNNING << "  complete = " << CL_COMPLETE << std::endl;
#endif
    if(ret == CL_COMPLETE) {
      _reading = false;
      if(_coverage == _count){
        _begins.clear();
        _ends.clear();
      }
      _coverage = 0;
      //TODO: anything else?
      return true;
    }
  }
  return false;
}

void petabricks::MatrixStorageInfo::releaseCLMem() {
  if(_refCount == 0){
#ifdef GPU_TRACE
    std::cout << this << " : release clmem" << std::endl;
#endif
    clReleaseMemObject(_clmem);
    _hasGpuMem = false;
  }
}

void petabricks::MatrixStorageInfo::incCoverage(IndexT* begin, IndexT* end, int size) {
  _refCount++;
  _coverage += size;
  JASSERT(_coverage <= _count).Text("Overwrite output.");
  _begins.push_back(begin);
  _ends.push_back(end);
}
#endif
