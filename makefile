.PHONY all:
all: diskinfo disklist diskget diskput

diskinfo: diskinfo.c
	gcc -Wall diskinfo.c -o diskinfo

disklist: disklist.c
	gcc -Wall disklist.c -o disklist

diskget: diskget.c
	gcc -Wall diskget.c -o diskget

diskput: diskput.c	
	gcc -Wall diskput.c -o diskput

.PHONY clean:
clean:
	-rm -rf *.o *.exe diskinfo disklist diskget diskput