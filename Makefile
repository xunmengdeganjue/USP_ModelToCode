EXE = modelToCode

OBJS = modelToCode.o

LIBS =
CFLAGS = -g -Wall -D_GNU_SOURCE

#
## Implicit rule will make the .c into a .o
## Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
## See Section 10.2 of Gnu Make manual
# 
$(EXE):$(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -Wl,-rpath $(LIBS)

clean:
	rm -rf $(OBJS) $(EXE)
