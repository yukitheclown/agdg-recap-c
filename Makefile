CC := gcc

RECV 	:= recap-recv
CREATE 	:= recap-create
PARSER 	:= parser

all: $(RECV) $(CREATE) $(PARSER)

$(RECV): recv.c
	$(CC) recv.c -o $@ -Wall -Wextra 

$(PARSER): parser.c json.c html.c
	$(CC) parser.c json.c html.c -o $@ -Wall -Wextra 

$(CREATE): create.c
	$(CC) create.c -o $@ -Wall -Wextra -lpng -lz