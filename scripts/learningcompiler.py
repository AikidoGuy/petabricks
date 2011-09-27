#!/usr/bin/python
import sys
import os
import shutil
import sqlite3
import random
import xml.dom.minidom
from pbutil import compileBenchmark
from sgatuner import autotune
from tunerconfig import config

#--------- Config ------------------
deleteTempDir = False
#--------- Autotuner config --------
config.max_time=1 #Seconds
#-----------------------------------





class HeuristicDB:
  def __init__(self):
    #Open DB    
    self.__db = sqlite3.connect(self.computeDBPath())
    self.__createTables()
    self.__bestNCache= dict()
    
  def __createTable(self, name, params):
    cur = self.__db.cursor()
    query = "CREATE TABLE IF NOT EXISTS '{0}' {1}".format(name, params)
    cur.execute(query)
    cur.close()
    self.__db.commit()
    
  def __createTables(self):
    self.__createTable("HeuristicKind", "('ID' INTEGER PRIMARY KEY AUTOINCREMENT, "
                                        "'name' TEXT UNIQUE)")
    self.__createTable("Heuristic", "('kindID' INTEGER, 'formula' TEXT, "
                                    "'useCount' INTEGER, "
                                    "PRIMARY KEY (kindID, formula), "
                                    "FOREIGN KEY ('kindID') REFERENCES 'HeuristicKind' ('ID')"
                                    "ON DELETE CASCADE ON UPDATE CASCADE)")
    #TODO:self.__createTable("InSet", "('setID' INTEGER, 'heuristicID' INTEGER)"
    
  def computeDBPath(self):
    #TODO: make the path more flexible
    dbPath= os.path.expanduser(config.output_dir+"/knowledge.db")
    return dbPath

  def getHeuristicKindID(self, kindName):
    cur = self.__db.cursor()
    query = "SELECT ID From HeuristicKind WHERE name='{0}'".format(kindName)
    cur.execute(query)
    kindID = cur.fetchone()[0]
    cur.close()
    return kindID
    
  def storeHeuristicKind(self, kindName):
    cur = self.__db.cursor()
    query = "INSERT OR IGNORE INTO HeuristicKind ('name') VALUES ('{0}')".format(kindName)
    cur.execute(query)
    cur.close()
    self.__db.commit()
    return self.getHeuristicKindID(kindName)
    
  def increaseHeuristicUseCount(self, name, formula):
    kindID=self.storeHeuristicKind(name) 
    cur = self.__db.cursor()
    query = "UPDATE Heuristic SET useCount=useCount+1 WHERE kindID=? AND formula=?"
    cur.execute(query, (kindID, formula))
    if cur.rowcount == 0:
      #There was no such heuristic in the DB: let's add it
      query = "INSERT INTO Heuristic (kindID, formula, useCount) VALUES (?, ?, 1)"
      cur.execute(query, (kindID, formula))
    cur.close()
    self.__db.commit()
    
  def addHeuristicSet(self, hSet):
    #TODO: also store it as a set
    for name, formula in hSet.iteritems():
      self.increaseHeuristicUseCount(name, formula)

  def getBestNHeuristics(self, name, N):
    try:
      cached = self.__bestNCache[name]
      return cached
    except:
      #Not in the cache
      #Fall back to accessing the db
      pass
    cur = self.__db.cursor()
    query = "SELECT formula FROM Heuristic JOIN HeuristicKind ON Heuristic.kindID=HeuristicKind.ID WHERE HeuristicKind.name=? ORDER BY Heuristic.useCount DESC LIMIT ?"
    cur.execute(query, (name, N))
    result = [row[0] for row in cur.fetchall()]
    cur.close()
    self.__bestNCache[name]=result
    return result
    
    
    
    
  
class HeuristicSet(dict):
  def toXmlStrings(self):
    return ["""<heuristic name="{0}" formula="{1}" />""".format(name, self[name]) for name in self]
  
  def toXmlFile(self, filename):
    outfile = open(filename, "w")
    for xmlstring in self.toXmlStrings():
      outfile.write(xmlstring)
      outfile.write("\n")
    outfile.close()
  
  def importFromXml(self, xmlDOM):
    for heuristicXML in xmlDOM.getElementsByTagName("heuristic"):
      name=heuristicXML.getAttribute("name")
      formula=heuristicXML.getAttribute("formula")
      self[name] = formula
  
  def complete(self, heuristicNames, db, N):
    """Complete the sets using the given db, so that it contains all the 
heuristics specified in the heuristicNames list.

Every missing heuristic is completed with one randomly taken from the best N 
heuristics in the database  """
    random.seed()
    
    #Find the missing heuristics
    missingHeuristics = list(heuristicNames)
    for name in self:
      try:
        missingHeuristics.remove(name)
      except ValueError:
        #A heuristic could be in the input file, but useless, therefore not in
        #the missingHeuristic list
        pass
      
    #Complete the set
    for heuristic in missingHeuristics:
      bestN=db.getBestNHeuristics(heuristic, N)
      if len(bestN) == 0:
        #No such heuristic in the DB. Do not complete the set
        #This is not a problem. It's probably a new heuristic:
        #just ignore it and it will fall back on the default implemented 
        #into the compiler
        continue
      formula=random.choice(bestN)
      self[heuristic] = formula
  
  
  
    
class HeuristicManager:
  """Manages sets of heuristics stored in a file with the following format:
<heuristics>
  <set>
    <heuristic name="heuristicName" formula="a+b+c" />
    <heuristic name="heuristicName2" formula="a+b+d" />
  </set>
  <set>
    <heuristic name="heuristicName3" formula="x+y*z" />
    <heuristic name="heuristicName4" formula="a+g+s" />
  </set>
</heuristics>
"""
  def __init__(self, heuristicSetFileName):
    self.__heuristicSets = []
    self.__xml = xml.dom.minidom.parse(heuristicSetFileName)
    
    # Extract information
    for hSet in self.__xml.getElementsByTagName("set"):
      self.__heuristicSets.append(self.__parseHeuristicSet(hSet))
    
    
  def __parseHeuristicSet(self, hSetXML):
    """Parses a xml heuristic set returning it as a list of pairs name-formula"""
    hSet = HeuristicSet()
    hSet.importFromXml(hSetXML)
    return hSet
     
  def heuristicSet(self, i):
    """Get the i-th heuristic set"""
    return self.__heuristicSets[i]
  
  def allHeuristicSets(self):
    return self.__heuristicSets
    

  
  
    



class LearningCompiler:
  def __init__(self, pbcExe, heuristicSetFileName, minTrialNumber=0):
    self.__heuristicManager = HeuristicManager(heuristicSetFileName)
    self.__minTrialNumber = 0
    self.__pbcExe = pbcExe    
    
  def compileLearningHeuristics(self, benchmark):
    #Define file names
    path, basenameExt = os.path.split(benchmark)
    basename, ext = os.path.splitext(basenameExt);
    basesubdir=os.path.join(path,basename+".tmp")
    
    #Init variables
    candidates=[]
    db = HeuristicDB()
    
    #Compile with current best heuristics
    outDir = os.path.join(basesubdir, "0")
    if not os.path.isdir(outDir):
      #Create the output directory
      os.makedirs(outDir)
    binary= os.path.join(outDir, basename)  
    compileBenchmark(self.__pbcExe, benchmark, binary=binary)  
    autotune(binary, candidates)
    
    #Get the full set of heuristics used
    infoFile=binary+".info"
    currentBestHSet = HeuristicSet()
    currentBestHSet.importFromXml(xml.dom.minidom.parse(infoFile))
    neededHeuristics = currentBestHSet.keys()
    
    #Get hSets
    allHSets = self.__heuristicManager.allHeuristicSets()
    numSets = max(len(allHSets), self.__minTrialNumber)
    
    
    count=1
    for hSet in allHSets:
      hSet.complete(neededHeuristics, db, numSets)
      
      #Define more file names
      outDir = os.path.join(basesubdir, str(count))
      if not os.path.isdir(outDir):
        #Create the output directory
        os.makedirs(outDir)
      binary= os.path.join(outDir, basename)  
      
      heuristicsFile= os.path.join(outDir, "heuristics.txt")
      hSet.toXmlFile(heuristicsFile)
      
      status = compileBenchmark(self.__pbcExe, benchmark, binary=binary, heuristics=heuristicsFile)
      if status != 0:
        print "Compile FAILED!"
        quit()
        
      #Autotune
      autotune(binary, candidates)
      
      count = count + 1
      
    #Get the number of dimensions available for all candidates
    dimensions = len(candidates[0].metrics[0])
    for candidate in candidates:
      dimensions = min(len(candidate.metrics[0]), dimensions)
      
    #Select best candidate (only looking at the biggest shared dimensions)
    scores = []
    for candidate in candidates:
      timingResultDB = candidate.metrics[0] #Get the 'timings' metric
      #The score for each candidate is the average timing on the highest shared 
      #dimension
      results = timingResultDB[2**(dimensions-1)]
      scores.append(float(results.mean()))
      
    minScore = scores[0]
    p=0
    bestIndex=0
    for s in scores:
      if s<minScore:
        minScore=s
        bestIndex=p
      p=p+1
    
    print "The best candidate is: {0}".format(bestIndex)
  
    #Move every file to the right place
    bestSubDir=os.path.join(basesubdir, str(bestIndex))
    #  compiled program:
    bestBin=os.path.join(bestSubDir, basename)
    finalBin=os.path.join(path, basename)
    shutil.move(bestBin, finalBin)
    #  .cfg file
    bestCfg=os.path.join(bestSubDir, basename+".cfg")
    finalCfg=os.path.join(path, basename+".cfg")
    shutil.move(bestCfg, finalCfg)
    #  .info file
    bestInfo=os.path.join(bestSubDir, basename+".info")
    finalInfo=os.path.join(path, basename+".info")
    shutil.move(bestInfo, finalInfo)
    #  .obj directory
    bestObjDir=os.path.join(bestSubDir, basename+".obj")
    destObjDir=os.path.join(path, basename+".obj")
    if os.path.isdir(destObjDir):
      shutil.rmtree(destObjDir)
    shutil.move(bestObjDir, path)
    #  input heuristic file
    if bestIndex != 0: #Program 0 is run with only the best heuristics in the DB
      bestHeurFile=os.path.join(bestSubDir, "heuristics.txt")
      finalHeurFile=os.path.join(path, basename+".heur")
      shutil.move(bestHeurFile, finalHeurFile)
    
    #Delete all the rest
    if deleteTempDir:
      shutil.rmtree(basesubdir)
    
    #Take the data about the used heuristics and store it into the db
    infoxml = xml.dom.minidom.parse(finalInfo)
    hSet = HeuristicSet()
    hSet.importFromXml(infoxml)
    db.addHeuristicSet(hSet)
    
    
    
    
    

#TEST
if __name__ == "__main__":
  basedir="/home/mikyt/programmi/petabricks/"
  pbc=basedir+"src/pbc"
  l=LearningCompiler(pbc, sys.argv[1])
  l.compileLearningHeuristics(sys.argv[2])
