#!/usr/bin/python

import errno
import getopt
import math
import os
import progress
import re
import select
import signal
import socket
import subprocess
import sys
import time
from xml.dom.minidom import parse
from pprint import pprint
from configtool import getConfigVal, setConfigVal

try:
  import numpy
except:
  sys.stderr.write("failed to import numpy\n")

#return number of cpus online
def cpuCount():
  try:
    return os.sysconf("SC_NPROCESSORS_ONLN")
  except:
    None
  try:
    return int(os.environ["NUMBER_OF_PROCESSORS"])
  except:
    None
  try:
    return int(os.environ["NUM_PROCESSORS"])
  except:
    sys.write("failed to get the number of processors\n")
  return 1 # guess 1

def getmemorysize():
  try:
    return int(re.match("MemTotal: *([0-9]+) *kB", open("/proc/meminfo").read()).group(1))*1024
  except:
    sys.write("failed to get total memory\n"%n)
    return 8 * (1024**3) # guess 8gb

def setmemlimit(n = getmemorysize()):
  try:
    import resource
    resource.setrlimit(resource.RLIMIT_AS, (n,n))
  except:
    sys.write("failed to set memory limit\n"%n)



def parallelRunJobs(jobs):
  class JobInfo:
    def __init__(self, id, fn):
      self.id=id
      self.fn=fn
      self.pid=None
      self.fd=None
      self.msg=""
      self.rv=None
    def __cmp__(this, that):
      return this.id-that.id
    def fileno(self):
      return self.fd.fileno()
    def forkrun(self):
      self.fd, w = socket.socketpair()
      self.pid = os.fork()
      if self.pid == 0:
        #child
        self.fd.close()
        class Redir():
          def __init__(self, fd):
            self.fd=fd
          def write(self, s):
            self.fd.sendall(s)
        sys.stdout = Redir(w)
        try:
          rv = self.fn()
        except Exception, e:
          print "Exception:",e
          rv = False
        print exitval
        if rv:
          sys.exit(0)
        else:
          sys.exit(1)
      else:
        #parent
        w.close()
        self.fd.setblocking(0)
        return self 
    def handleevent(self):
      if self.pid is None:
        return None
      try:
        m=self.fd.recv(1024)
        if m is not None:
          self.msg+=m
        if self.msg.rfind(exitval) >= 0:
          raise Exception("done")
      except:
        pid, self.rv = os.waitpid(self.pid, 0)
        assert self.pid == pid
        self.pid = None
        self.fd.close()
        self.fd = None
    def kill(self):
      if self.pid is not None:
        os.kill(self.pid, signal.SIGKILL)
    def addmsg(self, msg):
      self.msg+=msg
    def getmsg(self):
      return self.msg.replace(exitval,"") \
                     .replace('\n',' ')   \
                     .strip()
    
  startline = progress.currentline()
  NCPU=cpuCount()+2
  exitval="!EXIT!"
  maxprinted=[0]

  jobs_pending = map(lambda id: JobInfo(id, jobs[id]), xrange(len(jobs)))
  jobs_running = []   # JobInfo list
  jobs_done    = []   # JobInfo list

  def mkstatus():
    s="running jobs: "
    failed=len(filter(lambda x: x.rv!=0, jobs_done))
    complete=(len(jobs_done)-failed)
    if complete>0:
      s += "%d complete, "%complete
    if failed>0:
      s += "%d failed, "%failed
    s += "%d running, "%len(jobs_running)
    s += "%d pending"%len(jobs_pending)
    return s
  def updatestatus(fast=False):
    progress.remaining(2*len(jobs_pending)+len(jobs_running))
    if not fast:
      for j in jobs_done[maxprinted[0]:]:
        if j.id==maxprinted[0]:
          print j.getmsg()
          maxprinted[0]+=1
        else:
          break

  progress.push()
  progress.status(mkstatus)
  updatestatus()

  try:
    while len(jobs_pending)>0 or len(jobs_running)>0:
      #spawn new jobs
      while len(jobs_pending)>0 and len(jobs_running)<NCPU:
        jobs_running.append(jobs_pending.pop(0).forkrun())
      updatestatus()
        
      #wait for an event
      rj, wj, xj = select.select(jobs_running, [], jobs_running)

      #handle pending data
      for j in rj:
        j.handleevent()
      for j in wj:
        j.handleevent()
      for j in xj:
        j.handleevent()

      #move completed jobs to jobs_done list
      newdone=filter(lambda x: x.pid is None, jobs_running)
      jobs_running = filter(lambda x: x.pid is not None, jobs_running)
      jobs_done.extend(newdone)
      jobs_done.sort()
      updatestatus(True)
  except KeyboardInterrupt:
    for j in jobs_running:
      j.kill()
      j.addmsg("INTERRUPTED")
    jobs_done.extend(jobs_running)
    jobs_done.sort()
    updatestatus()
    raise
  updatestatus()
  progress.pop()
  return jobs_done

def chdirToPetabricksRoot():
  isCurDirOk = lambda: os.path.isdir("examples") and os.path.isdir("src")
  if isCurDirOk():
    return
  old=os.getcwd()
  if not isCurDirOk():
    os.chdir(os.pardir)
  if not isCurDirOk():
    os.chdir(os.pardir)
  if not isCurDirOk():
    os.chdir(old)
    raise Exception("This script should be run from petabricks root directory")

def compilePetabricks():
  cmd=["make","-sqC","src","all"]
  if subprocess.call(cmd) != 0: 
    cmd=["make", "-j%d"%cpuCount()]
    p=subprocess.Popen(cmd)
    rv=p.wait()
    if rv!=0:
      raise Exception("pbc compile failed")
    return rv
  return 0
    
benchmarkToBin=lambda name:"./examples/%s"%name
benchmarkToSrc=lambda name:"./examples/%s.pbcc"%name
benchmarkToInfo=lambda name:"./examples/%s.info"%name
benchmarkToCfg=lambda name:"./examples/%s.cfg"%name


def normalizeBenchmarkName(n, search=True):
  n=re.sub("^[./]*examples[/]","",n);
  n=re.sub("[.]pbcc$","",n);
  if os.path.isfile(benchmarkToSrc(n)) or not search:
    return n
  #search for the file
  n+=".pbcc"
  for root, dirs, files in os.walk("./examples"):
    if n in files:
      return normalizeBenchmarkName("%s/%s"%(root,n), False)
  raise Exception("invalid benchmark name: "+n)

def compileBenchmarks(benchmarks):
  NULL=open("/dev/null","w")
  pbc="./src/pbc"
  libdepends=[pbc, "./src/libpbmain.a", "./src/libpbruntime.a", "./src/libpbcommon.a"]
  assert os.path.isfile(pbc)
  benchmarkMaxLen=0

  def compileBenchmark(name):
    print name.ljust(benchmarkMaxLen)
    src=benchmarkToSrc(name)
    bin=benchmarkToBin(name)
    if not os.path.isfile(src):
      print "invalid benchmark"
      return False
    srcModTime=max(os.path.getmtime(src), reduce(max, map(os.path.getmtime, libdepends)))
    if os.path.isfile(bin) and os.path.getmtime(bin) > srcModTime:
      print "compile SKIPPED"
      return True
    else:
      if os.path.isfile(bin):
        os.unlink(bin)
      p = subprocess.Popen([pbc, src], stdout=NULL, stderr=NULL)
      status = p.wait()
      if status == 0:
        print "compile PASSED"
        return True
      else:
        print "compile FAILED (rc=%d)"%status
        return False
  
  newjob = lambda name, fn: lambda: compileBenchmark(name) and fn()
  mergejob = lambda oldfn, fn: lambda: oldfn() and fn()

  jobs=[]
  #benchmarks.sort()
  lastname=""
  #build list of jobs
  for b in benchmarks:
    if type(b) is type(()):
      name, fn = b
    else:
      name, fn = b, lambda: True
    benchmarkMaxLen=max(benchmarkMaxLen, len(name))
    if lastname==name:
      jobs.append(mergejob(jobs.pop(), fn))
    else:
      jobs.append(newjob(name,fn))
      lastname=name

  return parallelRunJobs(jobs)

def loadAndCompileBenchmarks(file, searchterms=[], extrafn=lambda b: True):
  chdirToPetabricksRoot()
  compilePetabricks()
  benchmarks=open(file)
  stripcomment = re.compile("([^#]*)([#].*)?")
  benchmarks=map(lambda x: stripcomment.match(x).group(1).strip(), benchmarks)
  benchmarks=filter(lambda x: len(x)>0, benchmarks)
  ws = re.compile("[ \t]+")
  benchmarks=map(lambda x: ws.split(x), benchmarks)

  if len(searchterms)>0:
    benchmarks=filter(lambda b: any(s in b[0] for s in searchterms), benchmarks)

  return compileBenchmarks(map(lambda x: (x[0], lambda: extrafn(x)), benchmarks))

def killSubprocess(p):
  if p.poll() is None:
    try:
      p.kill() #requires python 2.6
    except:
      os.kill(p.pid, signal.SIGTERM)

def tryAorB(A, B):
  def tryAorBinst(x):
    try:
      return A(x)
    except:
      return B(x)
  return tryAorBinst

#attempt to convert to an int or float
tryIntFloat = tryAorB(int, tryAorB(float, lambda x: x))

class TimingRunTimeout(Exception):
  def __str__(self):
    return repr(self.value)

class TimingRunFailed(Exception):
  def __init__(self, value):
    self.value = value
  def __str__(self):
    return repr(self.value)

#parse timing results with a given time limit
def executeTimingRun(prog, n, args=[], limit=None):
  null=open("/dev/null", "w")
  cmd = [ prog, "--n=%d"%n, "--time" ]
  for x in args:
    cmd.append(x);
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=null)

#  if limit is not None:
#    signal.signal(signal.SIGALRM, lambda signum, frame: killSubprocess(p))
#    signal.alarm(limit)

  # Python doesn't check if its system calls return EINTR, which is kind of
  # dumb, so we have to catch this here.
  while True:
    try:
      p.wait()
    except OSError, e:
      if e.errno == errno.EINTR:
        continue
      else:
        raise
    else:
      break

#  if limit is not None:
#    signal.alarm(0)
#    signal.signal(signal.SIGALRM, signal.SIG_DFL)

  if p.returncode == -15:
    raise TimingRunTimeout()
  if p.returncode != 0:
    raise TimingRunFailed(p.returncode)

  rslt = parse(p.stdout)
  rslt = rslt.getElementsByTagName("timing")[0].attributes
  attrs=dict()
  for x in xrange(rslt.length):
    attrs[str(rslt.item(x).name)]=tryIntFloat(rslt.item(x).nodeValue)
  return attrs

def collectTimingSamples(prog, n=100, step=100, maxTime=10.0, x=[], y=[], args=[], scaler=lambda x: x):
  start=time.time()
  left=maxTime
  try:
    while left>0:
      ni = int(math.ceil(scaler(n)))
      y.append(executeTimingRun(prog, ni, args=args, limit=int(left+1))['average'])
      x.append(ni)
      n+=step
      left=start+maxTime-time.time()
  except TimingRunTimeout:
    if len(x)<1:
      raise
  return x,y

def binarySearchInverse(fx, y, thresh=0.001, min=0.0, max=1000000000):
  y0=fx(min)
  yn=fx(max)
  assert y0<=yn
  if y0 > y-thresh:
    return min
  if yn < y+thresh:
    return max
  guess=(min+max)/2.0
  yguess=fx(guess)
  #binary search
  if abs(yguess-y) < thresh:
    return guess
  if yguess>y:
    return binarySearchInverse(fx, y, thresh, min, guess)
  else:
    return binarySearchInverse(fx, y, thresh, guess, max)


#fit y = c1 * x**c2
def expFitRaw(x,y):
  # shift to log scale
  x=map(lambda z: math.log(z,2), x)
  y=map(lambda z: math.log(z,2), y)
  # and polyfit
  c2,c1 = numpy.polyfit(x, y, 1)
  c1=2**c1
  return c1,c2

#fit y = c1 * x**c2
def expFit(x,y):
  c1,c2 = expFitRaw(x,y)
  return lambda x: c1*x**c2,\
         lambda y: 2**(math.log(y/c1, 2)/c2), \
         "%.10f * x^%.4f"%(c1,c2)

#fit y = p[0]*x**n + ... + p[n-2]*x + p[n-1]
#order is picked automatically based on expFit
def polyFit(x,y):
  c1, order = expFitRaw(x,y)
  p = numpy.polyfit(x, y, int(math.ceil(order)))
  fx=lambda x: numpy.polyval(p,x)
  invfx=lambda y: binarySearchInverse(fx, y)
  return fx, invfx, repr(p)

def collectTimingSamples2(prog, maxTime=12.0, args=[]):
  #make initial guess at order
  x,y=collectTimingSamples(prog, 4,   1,   maxTime, args=args, scaler=lambda x: 2**x)
  return x,y

def testEstimation(x, y, fit, prog):
  pf, pinv, pStr = fit(x,y)
  print "  ",pStr
  print "   est 10k",   pf(10000) #, "actual=", executeTimingRun(prog,10000)['average']
  print "   est 1 sec", (pinv(1))
  print "   est 2 sec", (pinv(2))
  print "   est 3 sec", (pinv(3))

def inferGoodInputSizes(prog, desiredTimes, maxTime=5.0):
  x,y=collectTimingSamples2(prog, maxTime)
  efx, efy, estr = expFit(x,y)
  #pfx, pfy, pstr = polyFit(x,y)
  sizes=map(int, map(efy, desiredTimes))
  return sizes


def getMakefileFlag(name):
  r=re.compile("^"+name+"[ ]*[=][ ]*(.*)")
  return r.match(filter(lambda l: r.match(l), open("src/Makefile"))[0]).group(1).strip()

getCXX      = lambda: getMakefileFlag("CXX")
getCXXFLAGS = lambda: getMakefileFlag("CXXFLAGS")

def getTunables(tx, type):
  return filter( lambda t: t.getAttribute("type")==type, tx.getElementsByTagName("tunable") )

getTunablesSequential=lambda tx: getTunables(tx, "system.cutoff.sequential")
getTunablesSplitSize=lambda tx: getTunables(tx, "system.cutoff.splitsize") 

if __name__ == "__main__":
  chdirToPetabricksRoot()
  compilePetabricks()
  compileBenchmarks(map(normalizeBenchmarkName, ["add", "multiply", "transpose"]))
  print "Estimating input sizes"
  inferGoodInputSizes("./examples/simple/add", [0.1,0.5,1.0], 2)

