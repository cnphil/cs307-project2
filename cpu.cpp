#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <fstream>
#include <deque>
#include <cmath>
using namespace std;

typedef pair<int,int> InterruptType;

#define TimerInterrupt(t) make_pair(t,1)
#define DiskInterrupt(t) make_pair(t,2)
#define ProcessCreationInterrupt(t) make_pair(t,3)
#define IDLE "IDLE_PROCESS"
#define NOBUFFER "NO_BUFFER"
#define EOP "END OF PROGRAM"
#define SOP "START OF PROGRAM"
#define ERR printf("VirtualError\n");
#define quantumLeft first
#define nextTimer second
int globalQuantum = 1000;
int globalContextSwitch = 50;
int globalPages = 75;
int globalSwap = 1000;
int globalCyclesPerSec = 100000;
bool debugEnable = false;


typedef string Interrupt;
#define TimerMsg() ("")
#define DiskMsg(pageName) (pageName)
#define ProcessCreationMsg(processName) (processName)
#define msgParseProcessName(msg) (msg)
#define msgParsePageName(msg) (msg)

class CPUBase;
class MemoryBase;
class MemoryModel;

class SchedulerBase
{
public:
	virtual bool handleInterrupts(int time, string currentProcess) {ERR}
	virtual void processTermination(int time, string currentProcess) {ERR}
	virtual void pageFault(int time, string faultingProcess, string faultingPage) {ERR}
	virtual void diskInterrupt(int time, string pageName) {ERR}
	virtual void initialize(SchedulerBase *s, CPUBase *c, MemoryBase *m) {ERR}
	virtual void creationInterrupt(int time, string processName) {ERR}
	virtual int closestInterruptTime() {ERR}
	
	virtual void debug() {ERR}
	virtual void debugCheck(string page) {ERR}
};

class CPUBase
{
public:
	virtual void notifyContextSwitch(string newProcess, int newProcessStartTime) {ERR}
	virtual void simulate() {ERR}
	virtual void initialize(SchedulerBase *s, CPUBase *c, MemoryBase *m) {ERR}
	virtual void creationInterrupt(int time, string processName) {ERR}
	virtual void pageFaultIncrease(string process) {ERR}
};

class MemoryBase
{
public:
	virtual void initialize(SchedulerBase *s, CPUBase *c, MemoryBase *m, MemoryModel *model) {ERR}
	virtual bool fetch(int time, string processName, string pageName) {ERR}
	virtual void swapPage(int time, string faultingProcess, string faultingPage) {ERR}
	virtual void pageArrival(string page) {ERR}
	
	virtual void debug() {ERR}
};

/*class ProcessInfo
{
public:
	int quantumLeft;
	int nextTimer;
};*/

void perror(string msg)
{
	cout << endl << msg << endl;
	exit(0);
}

typedef pair<int,int> ProcessInfo;

class Scheduler : SchedulerBase
{
	map<InterruptType, Interrupt> interrupts;
	deque<string> faultQueue, readyQueue;
	set<string> blockedQueue;
	map<string, string> blockedPage; // maps a process to a page
	map<string, ProcessInfo> infoTable;
	
	CPUBase *cpu;
	MemoryBase *memory;

public:
	int closestInterruptTime()
	{
		if(interrupts.begin() == interrupts.end()) return -1;
		return (interrupts.begin()->first).first;
	}
	void debugCheck(string page)
	{
		for(set<string>::iterator iter = blockedQueue.begin(); iter != blockedQueue.end(); iter++) {
			if(blockedPage[(*iter)] == page) {
				perror("Gun");
			}
		}
		
	}
	
	void debug()
	{
		cout << "!! faultQueue: ";
		for(deque<string>::iterator iter = faultQueue.begin(); iter != faultQueue.end(); iter++) cout << (*iter) << " ";
		cout << endl;
		cout << "!! readyQueue: ";
		for(deque<string>::iterator iter = readyQueue.begin(); iter != readyQueue.end(); iter++) cout << (*iter) << " ";
		cout << endl;
		cout << "!! blockedQueue: ";
		for(set<string>::iterator iter = blockedQueue.begin(); iter != blockedQueue.end(); iter++) cout << (*iter) << "(" << blockedPage[*iter]  << ") ";
		cout << endl;
		
	}
	void initialize(SchedulerBase *s, CPUBase *c, MemoryBase *m)
	{
		cpu = c;
		memory = m;
	}
	
	void creationInterrupt(int time, string processName)
	{
		if(debugEnable)cout << "! Interrupt Creation: " << ProcessCreationMsg(processName) << " @ " << time << endl;
		while(interrupts.find(ProcessCreationInterrupt(time)) != interrupts.end()) time++;
		interrupts[ProcessCreationInterrupt(time)] = ProcessCreationMsg(processName);
	}
	
	void diskInterrupt(int time, string pageName)
	{
		interrupts[DiskInterrupt(time)] = pageName;
	}
	
	bool handleInterrupts(int time, string currentProcess)
	{
		if(debugEnable)cout << "Cycle: " << time << " interrupts " << interrupts.size() << endl;
		
		if(interrupts.size() == 0 && faultQueue.size() + readyQueue.size() + blockedQueue.size() == 0) return false;
		
		// handle ProcessCreation first
		if(interrupts.find(ProcessCreationInterrupt(time)) != interrupts.end()) {
			// add it to the ready queue
			if(debugEnable)cout << "! Creation:"  << interrupts[ProcessCreationInterrupt(time)] << endl;
			readyQueue.push_back(msgParseProcessName(interrupts[ProcessCreationInterrupt(time)]));
			infoTable[(msgParseProcessName(interrupts[ProcessCreationInterrupt(time)]))] = make_pair(globalQuantum, 0);
			
			interrupts.erase(interrupts.find(ProcessCreationInterrupt(time)));
		}
		// handle Disk then
		if(interrupts.find(DiskInterrupt(time)) != interrupts.end()) {
			// add it to the fault queue
			string faultingPage = msgParsePageName(interrupts[DiskInterrupt(time)]);
			
			for(set<string>::iterator iter = blockedQueue.begin(); iter != blockedQueue.end(); )
				if(blockedPage[(*iter)] == faultingPage) {
					string faultingProcess = (*iter);
					faultQueue.push_back(faultingProcess);
					
					blockedPage.erase(blockedPage.find(faultingProcess));
					set<string>::iterator titer = iter;
					++iter;
					if(iter != blockedQueue.end()) {
						string tmp = *iter;
						blockedQueue.erase(titer);
						iter = blockedQueue.find(tmp);
					} else {
						blockedQueue.erase(titer);
						break;
					}
				} else {
					iter++;
				}
			
			memory->pageArrival(faultingPage);
			
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
				
				int estTimer = time + globalContextSwitch + infoTable[nextProcess].quantumLeft;
				if(debugEnable)cout<<"quantumLeft:"<<infoTable[nextProcess].quantumLeft<<endl;
				if(infoTable[nextProcess].quantumLeft == 0) {
					// this is not expected
					perror("quantum");
				} else {
					infoTable[nextProcess].nextTimer = estTimer;
					interrupts[TimerInterrupt(estTimer)] = TimerMsg();
				}
				if(debugEnable)cout<<"nextTimer:"<<estTimer<<endl;
				cpu->notifyContextSwitch(nextProcess, time + globalContextSwitch);
			}
			
			
			if(interrupts.find(TimerInterrupt(time)) != interrupts.end())
				interrupts.erase(interrupts.find(TimerInterrupt(time)));
		}
		
		return true;
	}
	
	void processTermination(int time, string currentProcess)
	{
		// revoke currentProcess's timer
		int estTimer = infoTable[currentProcess].nextTimer;
		if(interrupts.find(TimerInterrupt(estTimer)) != interrupts.end()) {
			interrupts.erase(interrupts.find(TimerInterrupt(estTimer)));
		}
		
		// and switch to idle temporarily
		cpu->notifyContextSwitch(IDLE, time + 1);
	}
	
	void pageFault(int time, string faultingProcess, string faultingPage)
	{
		if(debugEnable)cout << "! " << "Fault starts for " << faultingProcess << "\n";
		
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
		
		if(debugEnable)cout << "Runnable passed\n";
		// else
		cpu->pageFaultIncrease(faultingProcess);
		// update quantumLeft
		if(infoTable[faultingProcess].nextTimer == time) {
			// revoke its timer
			if(debugEnable)cout<<"starts1\n";
			interrupts.erase(interrupts.find(TimerInterrupt(time)));
			if(debugEnable)cout<<"interrupt erase done \n";
			// renew its quantum
			infoTable[faultingProcess].quantumLeft = globalQuantum;
		} else {
			// revoke its timer
			if(debugEnable)cout<<"starts2"<<time<<" " << infoTable[faultingProcess].nextTimer<< "\n";
			interrupts.erase(interrupts.find(TimerInterrupt(infoTable[faultingProcess].nextTimer)));
			if(debugEnable)cout<<"interrupt erase done 2\n";
			// renew its quantum
			infoTable[faultingProcess].quantumLeft = infoTable[faultingProcess].nextTimer - time;
		}
		
		if(debugEnable)cout << "! " << faultingProcess << " kicked by page fault " << faultingPage << " at " << time << endl;
		
		memory->swapPage(time, faultingProcess, faultingPage);
		blockedQueue.insert(faultingProcess);
		
		blockedPage[faultingProcess] = faultingPage;
		cpu->notifyContextSwitch(IDLE, time);
		
		if(debugEnable)cout << "! " << "Fault ends\n";
	}
	
};

class CPU : CPUBase
{
	SchedulerBase *scheduler;
	MemoryBase *memory;
	map<string, ifstream*> fd; // file descripters
	map<string, string> reverseBuffer;
	map<string, int> cycleCount, pageFaultCount, terminationTime;
	int time, currentProcessStartTime;
	int idleCycles;
	string currentProcess, nextMem;
	
	bool readBuffer(string processName)
	{
		if(reverseBuffer.find(processName) == reverseBuffer.end() || reverseBuffer[processName] == NOBUFFER)
		{
			ifstream *thisFd = fd[processName];
			return ((*thisFd) >> nextMem);
		}
		nextMem = reverseBuffer[processName];
		reverseBuffer[processName] = NOBUFFER;
		return true;
	}
	
public:
	void cycleCountIncrease(string process)
	{
		++cycleCount[process];
	}
	
	void pageFaultIncrease(string process)
	{
		++pageFaultCount[process];
	}
	
	void initialize(SchedulerBase *s, CPUBase *c, MemoryBase *m)
	{
		scheduler = s;
		memory = m;
	}
	void notifyContextSwitch(string newProcess, int newProcessStartTime)
	{	
		if(debugEnable)cout << "! " << "Context switch: " << currentProcess << " -> " << newProcess << " to be started at " << newProcessStartTime << endl;
			
		if(nextMem != SOP && nextMem != EOP && currentProcess != IDLE) {
			// nextMem not executed, restore nextMem to buffer
			reverseBuffer[currentProcess] = nextMem;
		}
		
		
		currentProcess = newProcess;
		currentProcessStartTime = newProcessStartTime;
		nextMem = SOP;
	}
	
	void simulate()
	{
		time = -1; currentProcessStartTime = -1; currentProcess = IDLE; idleCycles = 0;
		do {
			if(debugEnable)cout << "Start of cycle " << time + 1 << " : " << currentProcess << endl;
			// start of a cycle
			// handle interrupts
			if(!(scheduler->handleInterrupts(++time, currentProcess)) && currentProcess == IDLE) {
				// simulation finished
				break;
			}
			
			// check who is running now
			if(currentProcess == IDLE) {
				// magic jump
				if(scheduler->closestInterruptTime() - 1 > time) {
					idleCycles += scheduler->closestInterruptTime() - time;
					time = scheduler->closestInterruptTime() - 1;
					continue;
				}
				++idleCycles; continue;
			}
			// check if currentProcess is on a context switch
			if(currentProcessStartTime > time) {++idleCycles; continue;}
			
			// new process?
			if(fd.find(currentProcess) == fd.end()) {
				ifstream *newFd = new ifstream;
				string targetFile = currentProcess + ".mem";
				newFd->open(targetFile.c_str());
				if(debugEnable)cout << targetFile << " opened\n";
				fd[currentProcess] = newFd;
				nextMem = SOP;
			}
			
			// do this cycle
			
			if(nextMem == SOP) {
				// do nothing
			} else {
				// do this cycle
				this->cycleCountIncrease(currentProcess);
			}
			
			
			// handling next mem ref
			if(debugEnable)cout << "Going to read\n";
			if(!this->readBuffer(currentProcess)) {
				// end of program
				nextMem = EOP;
				
				// switch to idle
				cout << "!! " << currentProcess << " terminated at " << time  << " with total cycles: " << cycleCount[currentProcess] << " and page faults: " << pageFaultCount[currentProcess] << endl;
				terminationTime[currentProcess] = time;
				scheduler->processTermination(time, currentProcess);
			
			} else {
				if(debugEnable)cout << "Read " << nextMem << endl;
				// not end of program
				// query memory
				bool inMem = memory->fetch(time, currentProcess, nextMem);
				
				if(debugEnable)cout << "Mem fetch " << inMem << " " << currentProcess << " : "<< cycleCount[currentProcess] << " \n";
				if(cycleCount[currentProcess] % 10000 == 0) {
					cout << "!!Mem fetch " << inMem << " " << currentProcess << " : "<< cycleCount[currentProcess] << " \n";
					scheduler->debug();
					// memory->debug();
					//if(currentProcess == "P8" && cycleCount[currentProcess] == 115100) debugEnable = true;
				}
					
					
				if(inMem) { // nextMem in memory
					// pretending page fetched
					
				} else { //nextMem not in memory
					// notify scheduler
					// disk swapping actually happens here
					if(debugEnable)cout << "! " << "Starting to call pageFault at " << time << " for " << currentProcess << " and " << nextMem << endl;
					scheduler->pageFault(time, currentProcess, nextMem);
					if(debugEnable)cout << "! " << "Finished pageFault\n";
				}
			}
			if(debugEnable)cout << "" << "Cycle " << time << " complete\n";
			
		} while(1);
		cout << "!! Simulation finished at cycle " << time << " with total idle time: " << idleCycles << endl;
		cout << "!! To conclude:\n";
		int totalPageFaults = 0;
		for(map<string,int>::iterator iter = terminationTime.begin(); iter != terminationTime.end(); iter++) {
			cout << "!! " << iter->first << " terminated at " << iter->second << ", ran " << cycleCount[iter->first] << " cycles with " << pageFaultCount[iter->first] << " page faults.\n";
			totalPageFaults += pageFaultCount[iter->first]; 
		}
		cout << "!! Total page faults: " << totalPageFaults << endl;
	}
};

// typedef pair<string,int> swappingInfo;

class MemoryModel
{
public:
	virtual bool accessPage(int time, string pageName) {ERR}
	virtual bool insertPage(int availTime, string pageName) {ERR} // always use this after kickOut
	virtual void markBusy(string pageName) {ERR}
	virtual void unmarkBusy(string pageName) {ERR}
	virtual bool kickOut() {ERR}
	virtual bool memoryFull() {ERR}
};

class FIFOMemory : MemoryModel
{
	deque<string> pages;
	map<string, bool> pageMarked;
	map<string, int> pageAvailTime;
public:
	bool accessPage(int time, string pageName)
	{		
		if(pageAvailTime.find(pageName) == pageAvailTime.end()) {
			return false;
		} else if(pageAvailTime[pageName] > time) {
			return false;
		}
		return true;
	}
	bool memoryFull()
	{
		return (pages.size() == globalPages);
	}
	bool insertPage(int availTime, string pageName)
	{
		pages.push_back(pageName);
		pageAvailTime[pageName] = availTime;
		pageMarked[pageName] = false;
		return true;
	}
	void markBusy(string pageName)
	{
		pageMarked[pageName] = true;
	}
	void unmarkBusy(string pageName)
	{
		pageMarked[pageName] = false;
	}
	bool kickOut()
	{
		if(!this->memoryFull()) return false;
		deque<string>::iterator iter = pages.begin();
		while(pageMarked[(*iter)]) {
			iter++;
			if(iter == pages.end()) {
				// not expected
				perror("kickout");
			}
		}
		string kickout = (*iter);
		pages.erase(iter);
		pageMarked.erase(pageMarked.find(kickout));
		pageAvailTime.erase(pageAvailTime.find(kickout));
		return true;
	}	
};

class Memory : MemoryBase
{
	map<string, int> awaitingTime;
	map<string, string> lastFaultProcess;
	MemoryModel *mmu;
	SchedulerBase *scheduler;
	int busyUntil; // should be -1 at first
	
public:
	void initialize(SchedulerBase *s, CPUBase *c, MemoryBase *m, MemoryModel *model)
	{
		scheduler = s;
		busyUntil = -1;
		mmu = model;
	}
	
	void debug()
	{
		for(map<string,int>::iterator iter = awaitingTime.begin(); iter != awaitingTime.end(); iter++)
			cout << "!! Awaiting page: " << iter->first << " to be available at " << iter->second << endl;
	}
	
	void pageArrival(string page)
	{
		awaitingTime.erase(awaitingTime.find(page));
	}
	bool fetch(int time, string processName, string pageName)
	{
		if(debugEnable)cout << "Going to fetch " << pageName << "\n";
		//this->updateAwaitingPages(time);
		if(debugEnable)cout << "Done updateAwaitingPages\n";
		if(lastFaultProcess.find(pageName) != lastFaultProcess.end()) {
			if(lastFaultProcess[pageName] == processName) {
				mmu->unmarkBusy(pageName);
				lastFaultProcess.erase(lastFaultProcess.find(pageName));
			}
		}
		if(debugEnable)cout << "Done lastFault check\n";
		return mmu->accessPage(time, pageName);
	}
	void swapPage(int time, string faultingProcess, string faultingPage)
	{
		//this->updateAwaitingPages(time);
		lastFaultProcess[faultingPage] = faultingProcess;
		if(awaitingTime.find(faultingPage) != awaitingTime.end()) {
			// already on a transfer
			// nothing to do
		} else {
			int ETA = max(time, busyUntil) + globalSwap;
			awaitingTime[faultingPage] = ETA;
			
			busyUntil = ETA;
			
			mmu->kickOut();
			mmu->insertPage(ETA, faultingPage);
			mmu->markBusy(faultingPage);
			
			scheduler->diskInterrupt(ETA, faultingPage);
		}
	}
};

int main()
{
	CPUBase *myCPU = (CPUBase *)(new CPU);
	MemoryBase *myMemory = (MemoryBase *)(new Memory);
	SchedulerBase *myScheduler = (SchedulerBase *)(new Scheduler);
	MemoryModel *myModel = (MemoryModel *)(new FIFOMemory);
	
	myScheduler->initialize(myScheduler, myCPU, myMemory);
	myCPU->initialize(myScheduler, myCPU, myMemory);
	myMemory->initialize(myScheduler, myCPU, myMemory, myModel);
	
	char buffer[255]; double runTime, burstTime, IOTime;
	while(scanf("%s %lf %lf %lf", buffer, &runTime, &burstTime, &IOTime) != EOF) {
		myScheduler->creationInterrupt(runTime * globalCyclesPerSec, buffer);
	}
	
	myCPU->simulate();
	
}
