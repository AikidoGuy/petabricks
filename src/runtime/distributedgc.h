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
#ifndef PETABRICKSDISTRIBUTEDGC_H
#define PETABRICKSDISTRIBUTEDGC_H

#include "remoteobject.h"
#include "remotehost.h"

namespace petabricks {


  class DistributedGC : public petabricks::RemoteObject {
    enum NotifyStages {
      FLUSH_MSGS,
      DO_SCAN,
      ABORT_GC,
    };
  public:
    DistributedGC() { JTRACE("construct"); }
    ~DistributedGC() { JTRACE("destroyed"); }


    static RemoteObjectPtr gen();
  
    void onCreated();
    void onNotify(int stage);
    void onRecv(const void* , size_t s);


    bool canDeleteLocal(RemoteObject& obj) const;
    
    void scan(std::vector<EncodedPtr>& response);
    void finishup();

  private:
    int _gen;
    RemoteObjectList _objects;
    RemoteObjectList _objectsMaybeDead;
  };

}

#endif


