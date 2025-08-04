import sys
import os
import getopt
import copy
import random
import shlex, subprocess
import time
import fcntl
import fnmatch
import ntpath
import shutil
import csv
import threading
from fcntl import flock, LOCK_EX, LOCK_SH, LOCK_UN, LOCK_NB


#--------------------------------------
# run command, get memory usage
#--------------------------------------
class MemMonitor(threading.Thread):
    def __init__(self,peak, pid):
        threading.Thread.__init__(self)
        self.peak = 0L
        self.pid = pid
    def run(self):
       while True:
           cmd = ['ps', '-p', str(self.pid), '-o' , 'rss=']
           process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False)
           (sout, serr) = process.communicate()
           if process.returncode != 0:
               break
           if self.peak < long(sout): self.peak = long(sout)
           time.sleep(.001)

def MemRun(command, input, output=True):
    t1 = time.time()
    process = subprocess.Popen(shlex.split(command),bufsize=-1,shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    # launch a side thread to monitor the peak memory
    thread1 = MemMonitor(0, process.pid)
    thread1.start()

    (sout, serr) = process.communicate(input)
    t2 = time.time()

    thread1.join()
    if output:
        sys.stdout.write(sout)
        sys.stderr.write(serr)
    print(process.returncode)
    assert(process.returncode == 0)
    t = t2 - t1
    return  thread1.peak, t


ipSz =  os.fstat(sys.stdin.fileno()).st_size
sin = None
if ipSz > 0 :
    sin = sys.stdin.read()

cmd = ''
prestr = sys.argv[1]
argIter = iter(sys.argv[2:])
for arg in argIter:
    cmd = cmd + ' ' + arg

orgRSS, t = MemRun(cmd,sin,True)

f = open(prestr+'peak_memory.txt','a')
fcntl.flock(f.fileno(), LOCK_EX)
f.write(cmd + ', ' + str(orgRSS) + ", KB" + ', Time ,' + str(t) + ', s\n')
fcntl.flock(f.fileno(), LOCK_UN)
f.close()
