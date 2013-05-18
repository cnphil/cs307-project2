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
int globalQuantum = 200;
int globalContextSwitch = 50;

typedef string Interrupt;
#define TimerMsg() ("")
#define DiskMsg(processName) (processName)
#define ProcessCreationMsg(processName) (processName)
#define msgParseProcessName(msg) (msg)

class ProcessInfo
{
	int quantumLeft;
	int nextTimer;
};

class Scheduler
{
	map<InterruptType, Interrupt> interrupts;
	deque<string> faultQueue, readyQueue;
	map<string, ProcessInfo> infoTable;
	
	CPU *cpu;
	Memory *memory;
	
	void handleInterrupts(int time, string currentProcess)
	{
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
		time = 0; currentProcessStartTime = 0; currentProcess = IDLE;
		do {
			// start of a cycle
			// handle interrupts
			Scheduler->handleInterrupts(++time, currentProcess);
			
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
