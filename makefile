all: slavedriver slaverunner
.PHONY: clean

masterObjects = master.o 
slaveObjects = slave.o

slavedriver: $(masterObjects)
	gcc -g -o slavedriver master.o

slaverunner: $(slaveObjects)
	gcc -g -o slaverunner slave.o

master.o: struct.h
	gcc -g -c master.c

slave.o: struct.h
	gcc -g -c slave.c

clean:
	-rm slavedriver slaverunner $(masterObjects) $(slaveObjects)
