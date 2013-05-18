all: cpu

clear:
	rm cpu
cpu:
	g++ cpu.cpp -o testData/cpu
