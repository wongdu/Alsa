#


#CC =g++
CC =arm-none-linux-gnueabi-g++
CXXFLAGS = -g -Wall -O0 
LIBS =  -L../lib -lasound -lpthread -ldl -lrt


TARGS = testCollect

all: $(TARGS)

OBJS += \
./AlsaCollect.o \
./testCollect.o \
./UtilsPoisx.o 


$(TARGS): $(OBJS)
	@echo 'Building target: $@'
	@rm -f $@
	@$(CC) -o $(TARGS) $^ $(CXXFLAGS) $(LIBS)
	@rm -f $(OBJS)

%.o : %.cpp
	@$(CC) -c $< $(CXXFLAGS) $(LIBS)

.PNONY: all clean

clean:
	-rm -rf $(TARGS) *.o