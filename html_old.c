#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "html.h"

#define ALIGN_UP(T, offset, align) (T)(((uintptr_t)offset + align - 1) & ~(align-1))

enum {
	HTML_TOKEN_NONE = 0,
	HTML_TOKEN_SPACE,
	HTML_TOKEN_EQUAL,
	HTML_TOKEN_GT,
	HTML_TOKEN_LT,
	HTML_TOKEN_IDENT,
	HTML_TOKEN_SLASH,
	HTML_TOKEN_QUOTE,
};

static void *ParserStackAlloc(HTML_Parser *parser, int size){

	if(parser->stack + ALIGN_UP(int, size, parser->align) > parser->stackEnd)
		longjmp(parser->env, HTML_ERROR_STACK_MEMORY);

	void *mem = parser->stack;

	parser->stack += ALIGN_UP(int, size, parser->align);

	return mem;
}

static HTML_Tag *AddTag(HTML_Parser *parser, HTML_Tag **list){

	HTML_Tag *next = *list;

	HTML_Tag *curr = ParserStackAlloc(parser, sizeof(HTML_Tag));

	memset(curr, 0, sizeof(HTML_Tag));

	curr->next = next;

	*list = curr;

	return curr;
}

static int PeekNextToken(HTML_Parser *parser){

	char *str = (char *)parser->memory + parser->index;
	char character = *str;

	int ret = HTML_TOKEN_NONE;

	if(character == '/' ) ret = HTML_TOKEN_SLASH;
	else if(character == ' ' ) ret = HTML_TOKEN_SPACE;
	else if(character == '\t') ret = HTML_TOKEN_SPACE;
	else if(character == '\n') ret = HTML_TOKEN_SPACE;
	else if(character == '=' ) ret = HTML_TOKEN_EQUAL;
	else if(character == '<' ) ret = HTML_TOKEN_LT;
	else if(character == '>' ) ret = HTML_TOKEN_GT;
	else if(character == '"' ) ret = HTML_TOKEN_QUOTE;
	else if(character == '\'' ) ret = HTML_TOKEN_QUOTE;

	else if((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z')
		|| character == '-' || character == '_')
			ret = HTML_TOKEN_IDENT;

	return ret;
}

static int GetNextToken(HTML_Parser *parser){

	int ret = PeekNextToken(parser);

	if(++parser->index >= parser->size)
		longjmp(parser->env, HTML_ERROR_INVALID_SYNTAX);

	return ret;
}

static char *CopyKey(HTML_Parser *parser){

	if(!parser->key) return NULL;

	int len = (parser->memory + parser->index) - parser->key;

	if(len <= 0) return NULL;

	char *key = (char *)ParserStackAlloc(parser, len+1);

	memcpy(key, parser->key, len);

	key[len] = 0;

	parser->key = NULL;

	return key;
}

static void ParseTag(HTML_Parser *parser, HTML_Tag *into){


	parser->key = NULL;

	for(;parser->index < parser->size; ++parser->index){

		char *str = (char *)parser->memory + parser->index;

		int token = PeekNextToken(parser);

		if(token == HTML_TOKEN_GT){

			if(!into->key)
				into->key = CopyKey(parser);

			++parser->index;

			return;
		}

		if(token == HTML_TOKEN_IDENT && !parser->key){
		
			parser->key = str;
		}

		if(!into->key && token == HTML_TOKEN_SPACE && parser->key){
			
			into->key = CopyKey(parser);
		}

		if(token == HTML_TOKEN_EQUAL){


			AddTag(parser, &into->attributes)->key = CopyKey(parser);

			do{

				token = GetNextToken(parser);

			} while(token != HTML_TOKEN_QUOTE && token != HTML_TOKEN_GT);
	

			if(token == HTML_TOKEN_GT)
				return;

			str = (char *)parser->memory + parser->index;

			do{

				token = GetNextToken(parser);

			} while(token != HTML_TOKEN_QUOTE && token != HTML_TOKEN_GT);

			if(token == HTML_TOKEN_GT)
				return;

			int len = (parser->index - (str - parser->memory)) - 1;

			char *key = (char *)ParserStackAlloc(parser, len+1);
			
			memcpy(key, str, len);
			
			key[len] = 0;

			into->attributes->string = key;
		}
	}
}

static void ParseBody(HTML_Parser *parser, HTML_Tag *into){

	char *contents = (char *)parser->memory + parser->index;

	while(parser->index < parser->size){

		if(GetNextToken(parser) == HTML_TOKEN_LT){

			--parser->index;

			int len = (parser->memory + parser->index) - contents;

			if(len > 0){

				AddTag(parser, &into->children)->string = (char *)ParserStackAlloc(parser, len+1);

				memcpy(into->children->string, contents, len);

				into->children->string[len] = 0;
			}

			++parser->index;

			int token;

			while((token = GetNextToken(parser)) == HTML_TOKEN_SPACE);

			if(token == HTML_TOKEN_SLASH){
	
				while(GetNextToken(parser) != HTML_TOKEN_GT);

				return;
			}

			--parser->index;

			ParseTag(parser, into);


			if(strcmp(into->key, "input") != 0 && strcmp(into->key, "meta") != 0 && strcmp(into->key, "link") != 0 &&
				strcmp(into->key, "img") != 0 && strcmp(into->key, "br") != 0 && strcmp(into->key, "hr") != 0)

				ParseBody(parser, AddTag(parser, &into->children));

			contents = (char *)parser->memory + parser->index + 1;

			continue;
		}
	}
}


static void Parse(HTML_Parser *parser){

	*parser->top = ParserStackAlloc(parser, sizeof(HTML_Tag));

	memset(*parser->top, 0, sizeof(HTML_Tag));

	ParseBody(parser, *parser->top);

}

static void PaddedPrint(int tabs, char *format, ...){

	va_list args;
	va_start(args, format);

	int k;
	for(k = 0; k < tabs; k++) printf("    ");

	vprintf(format, args);

	va_end(args);
}

void HTML_Dump(HTML_Tag *tag, int tabs){

	if(!tag) return;

	HTML_Dump(tag->next, tabs);

	PaddedPrint(tabs, "%s {\n", tag->key);

	PaddedPrint(tabs+1, "attributes {\n");

	HTML_Dump(tag->attributes, tabs+2);

	PaddedPrint(tabs+1, "}\n");

	PaddedPrint(tabs+1, "contents {\n");

	if(tag->children)
		HTML_Dump(tag->children, tabs+2);

	if(tag->string)
		PaddedPrint(tabs+2, "%s\n", tag->string);

	PaddedPrint(tabs+1, "}\n");

	PaddedPrint(tabs, "}\n");
}

char *HTML_Error(int err){

	switch(err){
		case HTML_ERROR_NON_TERMINATED_STRING:
			return "Non terminated string.\n";
			break;
		case HTML_ERROR_INVALID_SYNTAX:
			return "Invalid syntax.\n";
			break;
		case HTML_ERROR_STACK_MEMORY:
			return "Ran out of stack memory.\n";
			break;
	}

	return "";
}

int HTML_Parse(HTML_Tag **top, char *memory, int size, void *stack, void *stackEnd, int align){

	stack = ALIGN_UP(void *, stack, align);
	stackEnd = ALIGN_UP(void *, stackEnd - align, align);

	HTML_Parser parser = {
		.top = top,
		.curr = NULL,
		.key = NULL,
		.memory = memory,
		.size = size,
		.index = 0,
		.stack = stack,
		.stackEnd = stackEnd,
		.align = align,
	};

	int err = setjmp(parser.env);

	if(err < 0) return err;

	Parse(&parser);

	return parser.stack - stack;
}