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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <fstream>

#include "jthread.h"
#include "jrefcounted.h"
#include "jfilesystem.h"
#include "matrix.h"
#include "matrixoperations.h"
#include "maximawrapper.h"
#include "transform.h"
#include "codegenerator.h"
#include "jfilesystem.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "cxxconfig.h"
#endif

void callCppCompiler(const std::string& src, const std::string& bin);

using namespace hecura;


const static std::string theMatrixHPath = jalib::Filesystem::FindHelperUtility("matrix.h");
const static std::string theMatrixIOHPath = jalib::Filesystem::FindHelperUtility("matrixio.h");
const static std::string theLibHecuraPath = jalib::Filesystem::FindHelperUtility("libhecura.a");
const static std::string theLibJUtilPath = jalib::Filesystem::FindHelperUtility("libjutil.a");

TransformListPtr parsePbFile(const char* filename);

int main( int argc, const char ** argv){
  if(argc != 2){
    fprintf(stderr, PACKAGE " PetaBricks compiler (pbc) v" VERSION "\n");
    fprintf(stderr, "USAGE: %s filename.pbcc\n", argv[0]);
    return 1;
  }
  std::string input = argv[1];
  std::string outputBin = jalib::Filesystem::Basename(input);
  std::string outputCode = outputBin + ".cpp";

  TransformListPtr t = parsePbFile(input.c_str());

  for(TransformList::iterator i=t->begin(); i!=t->end(); ++i){
      JTRACE("initializing")(input)(outputCode)((*i)->name());
      (*i)->initialize();
      #ifdef DEBUG
      (*i)->print(std::cout);
      #endif
  }

  for(bool isDone=false;!isDone;){
    isDone=true;
    JTRACE("begin learning iteration");
    std::ofstream of(outputCode.c_str());
    CodeGenerator o(of);

    o.comment("Generated by " PACKAGE " PetaBricks compiler (pbc) v" VERSION);
    o.write("#include \""+theMatrixHPath+"\"");
    o.write("#include \""+theMatrixIOHPath+"\"");
    #ifdef SHORT_TYPE_NAMES
    o.write("using namespace hecura;");
    #endif
    o.newline();
  
    for(TransformList::iterator i=t->begin(); i!=t->end(); ++i){
      JTRACE("generating")(input)(outputCode)((*i)->name());
      (*i)->learner().onIterationBegin();
      (*i)->generateCodeSimple(o);
      (*i)->learner().onIterationEnd();
      if((*i)->learner().shouldIterateAgain()){
        (*i)->generateMainCode(o);
        of.flush();
        of.close();
        callCppCompiler(outputCode, outputBin);
        (*i)->tester().setBinary(outputBin);
        (*i)->learner().runTests((*i)->tester());
        isDone=false;
        break;
      }
    }
    if(isDone){
      t->back()->generateMainCode(o);
      of.flush();
      of.close();
      callCppCompiler(outputCode, outputBin);
    }
  }
  JTRACE("done")(input)(outputCode)(outputBin);
  return 0;
}

void callCppCompiler(const std::string& src, const std::string& bin){
  std::string cmd = CXX " " CXXFLAGS " -o " + bin + " " + src + " " + theLibHecuraPath + " " + theLibJUtilPath;
  JTRACE("Running g++")(cmd);
  int rv = system(cmd.c_str());
  JASSERT(rv==0)(rv)(cmd).Text("g++ call failed");
}