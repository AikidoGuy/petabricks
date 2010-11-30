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

#include "codegenerator.h"
#include "maximawrapper.h"
#include "transform.h"

#include "common/jargs.h"
#include "common/jfilesystem.h"
#include "common/jtunable.h"

#include <iostream>
#include <fstream>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef HAVE_OPENCL
#include "openclutil.h"
#endif

void callCxxCompiler();
std::string cmdCxxCompiler();

using namespace petabricks;

//do dynamic searches for the correct paths to needed libraries/headers
static bool shouldCompile = true;
static bool shouldLink = true;
static std::string theCommonDir;
static std::string theInput;
static std::string theLibDir;
static std::string theMainName;
static std::string theObjectFile;
static std::string theOutputBin;
static std::string theOutputCode;
static std::string theOutputInfo;
static std::string theRuntimeDir;
static std::string theHardcodedConfig;

//defined in pbparser.ypp
TransformListPtr parsePbFile(const char* filename);

int main( int argc, const char ** argv){

  /*
  #ifdef HAVE_OPENCL
  OpenCLUtil::init();
  OpenCLUtil::printDeviceList();
  OpenCLUtil::deinit();
  exit(-1);
  #endif
  */

  jalib::JArgs args(argc, argv);
  std::vector<std::string> inputs;
  if(args.needHelp())
    std::cerr << "OPTIONS:" << std::endl;

  args.param("input",      inputs).help("input file to compile (*.pbcc)");
  args.param("output",     theOutputBin).help("output binary to be produced");
  args.param("outputcpp",  theOutputCode).help("output *.cpp file to be produced");
  args.param("outputinfo", theOutputInfo).help("output *.info file to be produced");
  args.param("outputobj",  theObjectFile).help("output *.o file to be produced");
  args.param("runtimedir", theRuntimeDir).help("directory where petabricks.h may be found");
  args.param("libdir",     theLibDir).help("directory where libpbruntime.a may be found");
  args.param("compile",    shouldCompile).help("disable the compilation step");
  args.param("link",       shouldLink).help("disable the linking step");
  args.param("main",       theMainName).help("transform name to use as program entry point");
  args.param("hardcode",   theHardcodedConfig).help("a config file containing tunables to set to hardcoded values");

  if(args.param("version").help("print out version number and exit") ){
    std::cerr << PACKAGE " compiler (pbc) v" VERSION " " REVISION_LONG << std::endl;
    return 1;
  }

  args.finishParsing(inputs);

  if(inputs.empty() || args.needHelp()){
    std::cerr << "\n" PACKAGE " compiler (pbc) v" VERSION " " REVISION_SHORT << std::endl;
    std::cerr << "USAGE: " << argv[0] << " [OPTIONS] filename.pbcc" << std::endl;
    std::cerr << "run `" << argv[0] << " --help` for options" << std::endl;
    return 1;
  }

  JASSERT(inputs.size()==1)(inputs.size()).Text("expected exactly one input file");
  theInput = inputs.front();
  if(theRuntimeDir.empty()) theRuntimeDir = jalib::Filesystem::Dirname(jalib::Filesystem::FindHelperUtility("runtime/petabricks.h"));
  if(theLibDir    .empty()) theLibDir     = jalib::Filesystem::Dirname(jalib::Filesystem::FindHelperUtility("libpbruntime.a"));
  if(theOutputBin .empty()) theOutputBin  = jalib::Filesystem::Basename(theInput);
  if(theOutputCode.empty()) theOutputCode = theOutputBin + ".cpp";
  if(theOutputInfo.empty()) theOutputInfo = theOutputBin + ".info";
  if(theObjectFile.empty()) theObjectFile = theOutputBin + ".o";
  
  
  if(!theHardcodedConfig.empty())
    CodeGenerator::theHardcodedTunables() = jalib::JTunableManager::loadRaw(theHardcodedConfig);

  JASSERT(jalib::Filesystem::FileExists(theInput))(theInput)
    .Text("input file does not exist");

  TransformListPtr t = parsePbFile(theInput.c_str());

  for(TransformList::iterator i=t->begin(); i!=t->end(); ++i){
    (*i)->initialize();
    #ifdef DEBUG
    (*i)->print(std::cout);
    #endif
  }

  for(TransformList::iterator i=t->begin(); i!=t->end(); ++i){
    (*i)->compile();
  }

  std::ofstream of(theOutputCode.c_str());

  StreamTreePtr root = new StreamTree("root");
  StreamTreePtr prefix = root->add(new StreamTree("prefix"));

  *prefix << "// Generated by " PACKAGE " compiler (pbc) v" VERSION " " REVISION_LONG "\n";
  *prefix << "// Compile with:\n";
  *prefix << "/*\n " << cmdCxxCompiler() << "\n*/\n";
  *prefix << "#include \"petabricks.h\"\n";
  *prefix << "#ifdef __GNUC__\n";
  *prefix << "#pragma GCC diagnostic ignored \"-Wunused-variable\"\n";
  *prefix << "#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n";
  *prefix << "#pragma GCC diagnostic ignored \"-Wunused-value\"\n";
  *prefix << "#endif\n";
  *prefix << "using namespace petabricks;\n\n";
  *prefix << CodeGenerator::theFilePrefix().str();

  std::ofstream infofile(theOutputInfo.c_str());
  CodeGenerator o(root);
  o.cg().beginGlobal();
#ifdef SINGLE_SEQ_CUTOFF
  o.createTunable(true, "system.cutoff.sequential", "sequentialcutoff", 64);
#endif
  o.cg().endGlobal();
  for(TransformList::iterator i=t->begin(); i!=t->end(); ++i){
    (*i)->generateCode(o);
  }

  //find the main transform if it has not been specified
  if(theMainName==""){
    for(TransformList::const_iterator i=t->begin(); i!=t->end(); ++i){
      if((*i)->isMain()){
        JASSERT(theMainName=="")(theMainName)((*i)->name())
          .Text("Two transforms both have the 'main' keyword");
        theMainName = (*i)->name();
      }
    }
    if(theMainName=="") theMainName = t->back()->name();
  }

  o.comment("A hook called by PetabricksRuntime");
  o.beginFunc("petabricks::PetabricksRuntime::Main*", "petabricksMainTransform");
  o.write("return "+theMainName+"_main::instance();");
  o.endFunc();
  
  o.comment("A hook called by PetabricksRuntime");
  o.beginFunc( "petabricks::PetabricksRuntime::Main*"
             , "petabricksFindTransform"
             , std::vector<std::string>(1, "const std::string& name"));
  for(TransformList::iterator i=t->begin(); i!=t->end(); ++i){
    (*i)->registerMainInterface(o);
  }
  o.write("return NULL;");
  o.endFunc();
  
  o.outputTunables(*prefix);
  root->writeTo(of);
  of.flush();
  of.close();
  o.cg().dumpTo(infofile);
  infofile.flush();
  infofile.close();

#ifdef DEBUG
  MAXIMA.sanityCheck();
#endif

  if(shouldCompile)
    callCxxCompiler();

  JTRACE("done")(theInput)(theOutputInfo)(theOutputCode)(theOutputBin);
  return 0;
}

std::string cmdCxxCompiler(){
  std::ostringstream os;
  os << "echo -n Calling C++ compiler...\\ && \\\n"
     << CXX " " CXXFLAGS " " CXXDEFS " -c -o " << theObjectFile << " " << theOutputCode << " -I\""<<theLibDir<<"\" -I\""<<theRuntimeDir<<"\""
     << " && \\\n echo done";
  if(shouldLink)
    os <<" && echo -n Linking...\\ && \\\n"
       << CXX " " CXXFLAGS " -o " << theOutputBin << " " << theObjectFile << CXXLDFLAGS " -L\""<< theLibDir <<"\" -lpbmain -lpbruntime -lpbcommon " CXXLIBS 
       << " && \\\n echo done";
  return os.str();
}

void callCxxCompiler(){
  std::string cmd = cmdCxxCompiler();
  JTRACE("Running g++")(cmd);
  //std::cout << cmd << std::endl;
  int rv = system(cmd.c_str());
  JASSERT(rv==0)(rv)(cmd).Text("g++ call failed");
}

