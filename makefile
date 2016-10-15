all: oss slaverunner
.PHONY: clean

masterObjects = master.o 
slaveObjects = slave.o

oss: $(masterObjects)
	gcc -g -o oss master.o

slaverunner: $(slaveObjects)
	gcc -g -o slaverunner slave.o

master.o: struct.h
	gcc -g -c master.c

slave.o: struct.h
	gcc -g -c slave.c

clean:
	-rm oss slaverunner $(masterObjects) $(slaveObjects)
