all: cpu

clear:
	rm *.o
	rm cpu
cpu:
	g++ cpu.cpp -o cpu
