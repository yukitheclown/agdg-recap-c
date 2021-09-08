#ifndef HTML_DEF
#define HTML_DEF

#include <setjmp.h>

#define HTML_ERROR_NON_TERMINATED_STRING 	-1
#define HTML_ERROR_INVALID_SYNTAX 			-2
#define HTML_ERROR_STACK_MEMORY 			-3
#define HTML_ERROR_UNEXPECTED_END 			-4
#define HTML_ERROR_NO_TAGS 					-5

typedef struct HTML_Tag HTML_Tag;

struct HTML_Tag {
	char 				*key;
	char 				*string;
	HTML_Tag 			*children;
	HTML_Tag 			*attributes;
	HTML_Tag 			*next;
};

typedef struct {

	HTML_Tag 		**top;

	char 			*end;
	int 			size;

	char 			*on;

	jmp_buf 		env;

	char 			*key;
	char 			*contents;

	void 			*stack;
	void 			*stackEnd;

	int 			align;

} HTML_Parser;


void 	HTML_Dump(HTML_Tag *tag, int tabs);
int 	HTML_Parse(HTML_Tag **top, char *memory, int size, void *stack, void *stackEnd, int align);
char 	*HTML_Error(int err);

#endif