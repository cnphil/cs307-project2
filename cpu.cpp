#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <deque>
using namespace std;

typedef pair<int,int> InterruptType;

#define TimerInterrupt(t) make_pair(t,1)
#define DiskInterrupt(t) make_pair(t,2)
#define ProcessCreationInterrupt(t) make_pair(t,3)
#define IDLE "Idle process and some random redundancy J&UHDSUX(AC())"
#define EOP "END OF PROGRAM"
#define SOP "START OF PROGRAM"
#define ERR printf("VirtualError");
int globalQuantum = 200;
int globalContextSwitch = 50;
int globalPages = 100;


typedef string Interrupt;
#define TimerMsg() ("")
#define DiskMsg(processName) (processName)
#define ProcessCreationMsg(processName) (processName)
#define msgParseProcessName(msg) (msg)

class SchedulerBase
{
public:
	virtual bool handleInterrupts(int time, string currentProcess) {ERR}
	virtual void processTermination(int time, string currentProcess) {ERR}
	virtual void pageFault(int time, string faultingProcess, string faultingPage) {ERR}
};

class CPUBase
{
public:
	virtual void notifyContextSwitch(string newProcess, int newProcessStartTime) {ERR}
	virtual void simulate() {ERR}
};

class MemoryBase
{
public:
};

class ProcessInfo
{
	int quantumLeft;
	int nextTimer;
};

class Scheduler : SchedulerBase
{
	map<InterruptType, Interrupt> interrupts;
	deque<string> faultQueue, readyQueue;
	map<string, ProcessInfo> infoTable;
	
	CPUBase *cpu;
	MemoryBase *memory;

public:
	bool handleInterrupts(int time, string currentProcess)
	{
		if(interrupts.size() == 0) return false;
		
		// handle ProcessCreation first
		if(interrupts.find(ProcessCreationInterrupt(time)) != interrupts.end()) {
			// add it to the ready queue
			readyQueue.push_back(msgParseProcessName(interrupts[ProcessCreationInterrupt(time)]));
			
			interrupts.erase(interrupts.find(ProcessCreationInterrupt(time)));
		}
		// handle Disk then
		if(interrupts.find(DiskInterrupt(time)) != interrupts.end()) {
			// add it to the fault queue
			faultQueue.push_back(msgParseProcessName(interrupts[DiskInterrupt(time)]));
			
			interrupts.erase(interrupts.find(DiskInterrupt(time)));
		}
		// handle Timer at last
		if(interrupts.find(TimerInterrupt(time)) != interrupts.end() || currentProcess == IDLE) {
			// timer triggered!
			if(faultQueue.size() + readyQueue.size() == 0) { // nothing is runnable
				if(currentProcess != IDLE) { // we only have one process
					// renew this one's quantum
					infoTable[currentProcess].quantumLeft = globalQuantum;
					infoTable[currentProcess].nextTimer = time + globalQuantum;
					
					interrupts[TimerInterrupt(time + globalQuantum)] = TimerMsg();
				} else {
					// really idle...
					// going to sleep.. Zzzz...
				}
			} else { // something is runnable
				string nextProcess = IDLE;
				if(faultQueue.size() != 0) {
					nextProcess = faultQueue.front();
					faultQueue.pop_front();
				} else {
					nextProcess = readyQueue.front();
					readyQueue.pop_front();
				}
				
				// kick out the current process
				if(currentProcess != IDLE) {
					infoTable[currentProcess].quantumLeft = globalQuantum;
					readyQueue.push_back(currentProcess);
				}
				
				int nextTimer = time + infoTable[nextProcess].quantumLeft;
				if(nextTimer == time) {
					// this is not expected
				} else {
					infoTable[nextProcess].nextTimer = nextTimer;
					interrupts[TimerInterrupt(nextTimer)] = TimerMsg();
				}
				cpu->notifyContextSwitch(nextProcess, nextTimer);
			}
			
			
			if(interrupts.find(TimerInterrupt(time)) != interrupts.end())
				interrupts.erase(interrupts.find(TimerInterrupt(time)));
		}
		
		return true;
	}
	
	void processTermination(int time, string currentProcess)
	{
		// revoke currentProcess's timer
		int nextTimer = infoTable[currentProcess].nextTimer;
		if(interrupts.find(TimerInterrupt(nextTimer)) != interrupts.end()) {
			interrupts.erase(interrupts.find(TimerInterrupt(nextTimer)));
		}
		
		// and switch to idle temporarily
		cpu->notifyContextSwitch(IDLE, time + 1);
	}
	
	void pageFault(int time, string faultingProcess, string faultingPage)
	{
		// IMPORTANT: we tell the memory to run a page replacement algorithm here
		// SO, if the faultingProcess is at the end of its quantum
		// (that is, its nextTimer <= time)
		// we just ignore this page fault, and leave it to the next quantum of this process
		// UNLESS it's the only runnable process
		
		// AT END OF THIS FUNCTION, switch to idle
		
		// check runnable processes
		if((infoTable[faultingProcess].nextTimer == time) && (interrupts.find(ProcessCreationInterrupt(time)) != interrupts.end() ||
			interrupts.find(DiskInterrupt(time)) != interrupts.end() ||
				faultQueue.size() + readyQueue.size() != 0)) {
			// ignore this fault
			return;
		}
		
		// else
		
		// update quantumLeft
		if(infoTable[faultingProcess].nextTimer == time) {
			// revoke its timer
			interrupts.erase(interrupts.find(TimerInterrupt(time)));
			// renew its quantum
			infoTable[faultingProcess].quantumLeft = globalQuantum;
		} else {
			// revoke its timer
			interrupts.erase(interrupts.find(TimerInterrupt(infoTable[faultingProcess].nextTimer)));
			// renew its quantum
			infoTable[faultingProcess].quantumLeft = time - infoTable[faultingProcess].nextTimer;
		}
		
		memory->swapPage(time, faultingProcess, faultingPage);
		cpu->notifyContextSwitch(IDLE, time);
	}
	
};

class CPU : CPUBase
{
	SchedulerBase *scheduler;
	MemoryBase *memory;
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
		time = 0; currentProcessStartTime = 0; currentProcess = IDLE;
		do {
			// start of a cycle
			// handle interrupts
			if(!(scheduler->handleInterrupts(++time, currentProcess)) && currentProcess == IDLE) {
				// simulation finished
				break;
			}
			
			// check who is running now
			if(currentProcess == IDLE) continue;
			// check if currentProcess is on a context switch
			if(currentProcessStartTime > time) continue;
			
			// new process?
			if(fd.find(currentProcess) == fd.end()) {
				ifstream *newFd = new ifstream;
				newFd->open(currentProcess + ".mem");
				fd[currentProcess] = newFd;
				nextMem = SOP;
			}
			
			// do this cycle
			bool thisCycleDone = true;
			if(nextMem == SOP) {
				// do nothing
			} else {
				// do this cycle
			}
			
			
			// handling next mem ref
			ifstream *thisFd = fd[currentProcess];
			if(!((*thisFd) >> nextMem)) {
				// end of program
				nextMem = EOP;
				
				// switch to idle
				Scheduler->processTermination(time, currentProcess);
			
			} else {
				// not end of program
				// query memory
				if(memory->fetch(time, currentProcess, nextMem)) { // nextMem in memory
					// pretending page fetched
					
				} else { //nextMem not in memory
					// notify scheduler
					// disk swapping actually happens here
					scheduler->pageFault(time + 1, currentProcess, nextMem);
				}
			}
			
			
		} while(1);
	}
};

typedef pair<string,int> swappingInfo;

class MemoryModel
{
public:
	virtual bool accessPage(int time, string pageName) {ERR}
	virtual bool insertPage(int time, string pageName) {ERR}
	virtual void markBusy(string pageName) {ERR}
	virtual void unmarkBusy(string pageName) {ERR}
	virtual bool kickOut() {ERR}
	virtual bool memoryFull() {ERR}
};

class FIFOMemory : MemoryModel
{
	deque<string> pages;
	deque<string> markedPages;
public:
	bool accessPage(int time, string pageName)
	{
		return pages.find(pageName) != pages.end();
	}
	void insertPage(int time, string pageName)
	{
		pages
	}
	void markBusy(string pageName)
	{
		
	}
	void unmarkBusy(string pageName)
	{
		
	}
	bool kickOut()
	{
		
	}	
};
class Memory : MemoryBase
{
	map<string, swappingInfo> awaitingPages;
	
};
