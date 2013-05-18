#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <map>
#include <fstream>
using namespace std;

typedef pair<int,int> InterruptType;

#define TimerInterrupt(t) make_pair(t,1)
#define DiskInterrupt(t) make_pair(t,2)
#define ProcessCreationInterrupt(t) make_pair(t,3)
#define IDLE "Idle process and some random redundancy J&UHDSUX(AC())"
#define EOP "END OF PROGRAM"
#define SOP "START OF PROGRAM"


class Interrupt
{
	
};

class Scheduler
{
	map<InterruptType, Interrupt> interrupts;
};

class CPU
{
	Scheduler *scheduler;
	Memory *memory;
	map<string, ifstream*> fd; // file descripters
	int time, currentProcessStartTime;
	string currentProcess, nextMem;
	
	void notifyContextSwitch(string newProcess, int newProcessStartTime)
	{		
		if(nextMem != SOP && nextMem != EOP && currentProcess != IDLE) {
			// nextMem not executed, repos the fd back
			ifstream *thisFd = fd[currentProcess];
			thisFd->seekg(-nextMem.length());
		}
		
		currentProcess = newProcess;
		currentProcessStartTime = newProcessStartTime;
		nextMem = SOP;
	}
	
	void simulate()
	{
		time = 0;
		do {
			// start of a cycle
			// handle interrupts
			Scheduler->handleInterrupts(++time);
			
			// check who is running now
			if(currentProcess == IDLE) continue;
			// check if currentProcess is on a context switch
			if(currentProcessStartTime < time) continue;
			
			// new process?
			if(fd.find(currentProcess) == fd.end()) {
				ifstream *newFd = new ifstream;
				newFd->open(currentProcess + ".mem");
				fd[currentProcess] = newFd;
				nextMem = SOP;
			}
			
			// do this cycle
			bool thisCycleDone = true;
			if(currentProcessStartTime == SOP) {
				// do nothing
			} else {
				// do this cycle
			}
			
			
			// handling next mem ref
			ifstream *thisFd = fd[currentProcess];
			if(!((*thisFd) >> nextMem)) {
				// end of program
				nextMem = EOP;
			
			} else {
				// not end of program
				// query memory
				if(Memory->fetch(time, currentProcess, nextMem)) { // nextMem in memory
					// pretending page fetched
					
				} else { //nextMem not in memory
					// notify scheduler
					// disk swapping actually happens here
					Scheduler->pageFault(time + 1, currentProcess, nextMem);
				}
			}
			
			
		} while(1);
	}
};
