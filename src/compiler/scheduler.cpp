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
#include "scheduler.h"

#include "codegenerator.h"
#include "pbc.h"
#include "transform.h"

#include "common/jasm.h"

#include <cstdio>
#include <algorithm>

static void _setEachTo(petabricks::ChoiceDepGraphNodeRemapping& mapping,
                       const petabricks::ChoiceDepGraphNodeSet& set,
                       petabricks::ChoiceDepGraphNode* node) {
  petabricks::ChoiceDepGraphNodeSet::const_iterator e;
  for(e=set.begin();e!=set.end(); ++e) {
    mapping[*e] = node;
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void petabricks::Schedule::initialize(const ChoiceDepGraphNodeSet& inputs,
                                      const ChoiceDepGraphNodeSet& outputs){
  JASSERT(_schedule.empty());

  SchedulingState state;
  state.generated = inputs;

  for( std::set<ChoiceDepGraphNode*>::const_iterator i=outputs.begin()
    ; i!=outputs.end()
    ; ++i )
  {
    JASSERT((*i)->isOutput());
    depthFirstChoiceDepGraphNode(state, *i);
  }
}


void petabricks::Schedule::depthFirstChoiceDepGraphNode(SchedulingState& state, ChoiceDepGraphNode* n){
  if(state.generated.find(n)!=state.generated.end())
    return;

  JASSERT(state.pending.find(n)==state.pending.end()).Text("dependency cycle");
  state.pending.insert(n);

  for( ScheduleDependencies::const_iterator i=n->directDepends().begin()
      ; i!=n->directDepends().end()
      ; ++i)
  {
    if(i->first != n) {
      depthFirstChoiceDepGraphNode(state, i->first);
    }
  }
//   JTRACE("scheduling")(n->matrix()); 
  _schedule.push_back(ScheduleEntry(n, n->directDependsRemapped()));
  state.generated.insert(n);
}

void petabricks::Schedule::generateCode(Transform& trans, CodeGenerator& o, RuleFlavor flavor){
  JASSERT(_schedule.size()>0);
  for(ScheduleT::iterator i=_schedule.begin(); i!=_schedule.end(); ++i){
    if(i!=_schedule.begin() && flavor!=E_RF_STATIC)
      o.continuationPoint();

    i->node().generateCode(trans, o, flavor, _choiceAssignment);

    if(flavor!=E_RF_STATIC) {
      for(ScheduleDependencies::const_iterator d=i->deps().begin();
          d!=i->deps().end();
          ++d){
        o.write(i->node().nodename()+"->dependsOn("+d->first->nodename()+");");
      }
      o.write(i->node().nodename()+"->enqueue();");
    }
  }
  if(flavor == E_RF_DYNAMIC) {
    o.write("DynamicTaskPtr  _fini = new NullDynamicTask();");
    for(ScheduleT::iterator i=_schedule.begin(); i!=_schedule.end(); ++i){
      if(i->node().isOutput()) {
        o.write("_fini->dependsOn(" + i->node().nodename() + ");");
      }
    }
    for(ScheduleT::iterator i=_schedule.begin(); i!=_schedule.end(); ++i){
      o.write(i->node().nodename()+"=0;");
    }
    o.write("return _fini;");
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



petabricks::StaticScheduler::StaticScheduler(const ChoiceGridMap& cg, Transform& tx){
  for(ChoiceGridMap::const_iterator m=cg.begin(); m!=cg.end(); ++m){
    ChoiceDepGraphNodeList& regions = _matrixToNodes[m->first];
    for(ChoiceGridIndex::const_iterator i=m->second.begin(); i!=m->second.end(); ++i){
      SimpleRegionPtr tmp = new SimpleRegion(i->first);
      regions.push_back(new BasicChoiceDepGraphNode(m->first, tmp, i->second));
      _allNodes.push_back(regions.back());
    }
  }
  _dbgpath = pbcConfig::theObjDir+"/"+tx.name();
}

petabricks::ChoiceDepGraphNodeSet petabricks::StaticScheduler::lookupNode(const MatrixDefPtr& matrix, const SimpleRegionPtr& region){
  ChoiceDepGraphNodeSet rv;
  ChoiceDepGraphNodeList& regions = _matrixToNodes[matrix];
  if(matrix->numDimensions()==0){
    JASSERT(regions.size()==1);
    rv.insert(regions.begin()->asPtr());
  }else{
    for(ChoiceDepGraphNodeList::iterator i=regions.begin(); i!=regions.end(); ++i){
      if(region->toString() == (*i)->region()->toString()){
        rv.insert(i->asPtr());
        break; //optimization
      }
      if((*i)->region()->hasIntersect(region)){
        rv.insert(i->asPtr());
      }
    }
  }
  JASSERT(rv.size()>0)(matrix)(region).Text("failed to find rule for region");
  return rv;
}

void petabricks::StaticScheduler::generateSchedule(){
  std::string dbgpathorig = _dbgpath;
  for(ChoiceDepGraphNodeList::iterator i=_allNodes.begin(); i!=_allNodes.end(); ++i){
    _choices.addConsumer(i->asPtr());
  }
  for(RuleChoiceCollection::iterator choice=_choices.begin(); choice!=_choices.end(); ++choice) {
    _dbgpath = dbgpathorig+".schedule"+jalib::XToString(choice);
    try {
      RuleChoiceAssignment choiceassign = _choices.getAssignment(choice);
      for(ChoiceDepGraphNodeList::iterator i=_allNodes.begin(); i!=_allNodes.end(); ++i){
        (*i)->resetRemapping();
      }

      #ifdef DEBUG
      writeGraph((_dbgpath+".initial.dot").c_str());
      #endif

      for(ChoiceDepGraphNodeList::iterator i=_allNodes.begin(); i!=_allNodes.end(); ++i){
        (*i)->applyChoiceRemapping(choiceassign);
      }

      computeIndirectDependencies();
      
      #ifdef DEBUG
      writeGraph((_dbgpath+".remapped.dot").c_str());
      #endif

      mergeCoscheduledNodes(choiceassign);

      #ifdef DEBUG
      writeGraph((_dbgpath+".merged.dot").c_str());
      #endif

      _schedules.push_back(new Schedule(choiceassign, _inputsRemapped, _outputsRemapped));
      
      #ifdef DEBUG
      _schedules.back()->writeGraph((_dbgpath+".final.dot").c_str());
      #endif
    } catch(CantScheduleException) {
      _choices.markInvalid(choice);
      JTRACE("SCHEDULING CHOICE FAIL");
    }
  }
  _dbgpath = dbgpathorig;
  JASSERT(!_schedules.empty())
    .Text("failed to find a legal schedule for nodes");
}



void petabricks::StaticScheduler::computeIndirectDependencies(){
  // this algorithm can be optimized, but since graphs are small it doesn't matter
  for(int c=1; c>0;){ //keep interating until no changes have been made
    c=0;
    for(ChoiceDepGraphNodeList::iterator i=_allNodes.begin(); i!=_allNodes.end(); ++i)
      c+=(*i)->updateIndirectDepends();
  }
}

void petabricks::StaticScheduler::mergeCoscheduledNodes(const RuleChoiceAssignment& choice){
  ChoiceDepGraphNodeSet done;
  ChoiceDepGraphNodeRemapping mapping;
  ChoiceDepGraphNodeList newMetaNodes;
  for(ChoiceDepGraphNodeList::iterator i=_allNodes.begin(); i!=_allNodes.end(); ++i){
    if(done.find(i->asPtr()) == done.end()){
      //process nodes one connected component at a time
      ChoiceDepGraphNodeSet set=(*i)->getStronglyConnectedComponent();
      done.insert(set.begin(),set.end());
      if(set.size()>1){

        // merge the nodes and update the dependencies 
        ChoiceDepGraphNodePtr meta = new MetaChoiceDepGraphNode(set);
        _setEachTo(mapping, set, meta.asPtr());
        meta->applyRemapping(mapping);
        
        JTRACE("coscheduling nodes")(meta);
       // meta->printNode(std::cerr);
       // meta->printEdges(std::cerr);

        // decide which scheduling policy to use
        const DependencyInformation& selfDep = meta->indirectDepends()[meta.asPtr()];
        if(selfDep.direction.isNone()){
          meta = new MultiOutputChoiceDepGraphNode(set);
        }else{
          meta = new SlicedChoiceDepGraphNode(set);
        }
        _setEachTo(mapping, set, meta.asPtr());
        newMetaNodes.push_back(meta);
      }
    }
  }
  for(ChoiceDepGraphNodeList::iterator i=_allNodes.begin(); i!=_allNodes.end(); ++i){
    (*i)->applyRemapping(mapping);
  }
  for(ChoiceDepGraphNodeList::iterator i=newMetaNodes.begin(); i!=newMetaNodes.end(); ++i){
    (*i)->applyRemapping(mapping);
  }
  for(ChoiceDepGraphNodeList::iterator i=newMetaNodes.begin(); i!=newMetaNodes.end(); ++i){
    if(!(*i)->findValidSchedule(choice)) {
      throw StaticScheduler::CantScheduleException();
    }
  }
  _inputsRemapped = _inputsOriginal;
  _inputsRemapped.applyRemapping(mapping);
  _outputsRemapped = _outputsOriginal;
  _outputsRemapped.applyRemapping(mapping);
  _metaNodes.insert(_metaNodes.end(), newMetaNodes.begin(), newMetaNodes.end());
}

void petabricks::StaticScheduler::generateCode(Transform& trans, CodeGenerator& o, RuleFlavor flavor) {

  if(flavor==E_RF_DYNAMIC) {
    for(ChoiceDepGraphNodeList::iterator i=_allNodes.begin(); i!=_allNodes.end(); ++i){
      o.addMember("DynamicTaskPtr", (*i)->nodename(), "");
    }
    for(ChoiceDepGraphNodeList::iterator i=_metaNodes.begin(); i!=_metaNodes.end(); ++i){
      o.addMember("DynamicTaskPtr", (*i)->nodename(), "");
    }
  }

  SchedulesT::iterator i;
  CodeGenerator& schedOutput = o.forkhelper();
  int n=0;
  o.beginSwitch("selectSchedule()");
  for(i=_schedules.begin(); i!=_schedules.end(); ++i,++n) {
    o.beginCase(n);

    if(flavor==E_RF_STATIC){
      schedOutput.beginFunc("void", "schedule"+jalib::XToString(n)+"seq");
      o.write("schedule"+jalib::XToString(n)+"seq();");
      o.write("return;");
    } else {
      schedOutput.beginFunc("DynamicTaskPtr", "schedule"+jalib::XToString(n)+"workstealing");
      o.write("return schedule"+jalib::XToString(n)+"workstealing();");
    }
    (*i)->generateCode(trans, schedOutput, flavor);
    schedOutput.endFunc();

    o.endCase();
  }
  o.endSwitch();
  o.write("JASSERT(false).Text(\"invalid schedule\");");
  if(flavor==E_RF_DYNAMIC) {
    o.write("return 0;");
  }

    
  if(flavor==E_RF_STATIC){
    std::string prefix = trans.name() + "_" + jalib::XToString(trans.nextTunerId()) + "_";
    CodeGenerator& decTreeOutput = o.forkhelper();
    decTreeOutput.beginFunc("int", "selectSchedule");
    if(_schedules.size()==1) {
      decTreeOutput.write("return 0;");
    }else{
      _choices.generateDecisionTree(prefix, _schedules.size(), decTreeOutput);
    }
    decTreeOutput.endFunc();
  }
}
  


void petabricks::StaticScheduler::renderGraph(const char* filename, const char* type) const{
  FILE* fd = popen(("dot -Grankdir=TD -T"+std::string(type)+" -o "+std::string(filename)).c_str(), "w");
  writeGraph(fd);
  pclose(fd);
}

void petabricks::StaticScheduler::writeGraph(const char* filename) const{
  FILE* fd = fopen(std::string(filename).c_str(), "w");
  writeGraph(fd);
  fclose(fd);
}

void petabricks::StaticScheduler::writeGraph(FILE* fd) const{
  std::string schedulerGraph = toString();
  fwrite(schedulerGraph.c_str(),1,schedulerGraph.length(),fd);
}




