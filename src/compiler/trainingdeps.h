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
#ifndef PETABRICKSTRAININGDEPS_H
#define PETABRICKSTRAININGDEPS_H

#include "common/jassert.h"

#include "rule.h"

#include <map>
#include <sstream>
#include <vector>

namespace petabricks {

class TrainingDeps {
public:

  void addAlgchoice(const std::string& name, int rules){
    _os << "    <algchoice "
        << " name=\""    << name << "\""
        << " rules=\""    << rules << "\""
        << " />\n";
  }

  void addTunable( bool isTunable
                 , const std::string& category
                 , const std::string& name
                 , int initial
                 , int min
                 , int max)
  {
    if(isTunable)
      _os<< "    <tunable";
    else
      _os<< "    <config";

    _os << " name=\""    << name << "\""
        << " type=\""    << category << "\""
        << " initial=\"" << initial << "\""
        << " min=\""     << min << "\""
        << " max=\""     << max << "\""
        << " />\n";
  }

  void beginTransform(const std::string& name,
                      const std::string& instanceName,
                      int templateChoice,
                      bool isVariableAccuracy,
                      double accuracyTarget
                      ){
    _os << "  <transform ";
    _os << " name=\""         << instanceName << "\"";
    _os << " templateName=\"" << name << "\"";
    _os << " isTemplateInstance=\"" << (name==instanceName ? "no" : "yes") << "\"";
    _os << " templateChoice=\"" << jalib::XToString(templateChoice) << "\"";
    _os << " isVariableAccuracy=\"" << isVariableAccuracy << "\"";
    _os << " accuracyTarget=\"" << accuracyTarget << "\"";
    _os << ">\n";
  }

  void beginGlobal(){
    _os << "  <global>\n";
  }
  void endGlobal(){
    _os << "  </global>\n";
  }

  void emitRules(std::string& choicename,
                 const std::vector<RulePtr>& sortedRules);

  void endTransform(const std::string& name, const std::string& /*instanceName*/){
    const std::vector<std::string>& calls = _callgraph[name];
    for(size_t i=0; i<calls.size(); ++i) {
      _os << "    <calls caller=\"" << name << "\" callee=\"" << calls[i]
          << "\" />\n";
    }
    _os << "  </transform>\n";
  }

  static void addCallgraphEdge(const std::string& caller, const std::string& callee){
    _callgraph[caller].push_back(callee);
  }

  void dumpTo(std::ostream& o){
    o << "<traininginfo>\n";
    o << _os.str();
    o << "</traininginfo>\n";
  }

private:
  std::stringstream _os;
  static std::map<std::string, std::vector<std::string> > _callgraph;
};

}

#endif
