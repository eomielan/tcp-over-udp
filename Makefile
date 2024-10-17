COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare 

LINKLIBS = -lpthread

SERVEROBJECTS = obj/receiver.o
CLIENTOBJECTS = obj/sender.o

.PHONY: all clean

all : obj sender receiver

receiver: $(SERVEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

sender: $(CLIENTOBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

clean :
	$(RM) obj/*.o sender receiver

obj/%.o: src/%.c
	$(CC) $(COMPILERFLAGS) -c -o $@ $<
obj:
	mkdir -p obj

