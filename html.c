#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "html.h"

#define ALIGN_UP(T, offset, align) (T)(((uintptr_t)offset + align - 1) & ~(align-1))

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

static int ExclusiveSkip(HTML_Parser *parser, char *characters){

	for(; parser->on < parser->end && *parser->on; ++parser->on){
		if(strchr(characters, *parser->on))
			return 1;
	}

	return 0;
}

static int Skip(HTML_Parser *parser, char *characters){

	for(; parser->on < parser->end && *parser->on; ++parser->on){
		if(strchr(characters, *parser->on) == NULL)
			return 1;
	}

	return 0;
}

static int CharacterReferences(char *str, int len){

	char *curr = str;
	char *from = str;

	while(from && from < str + len){

		if(*from == '&'){
			
			++from;

			if(*from == '#'){

				++from;

				if(*from == 'x'){
					*(from + 5) = 0;
					*curr = (char)strtol(from+1, NULL, 16);
					from += 6;
				} else {
					*(from + 4) = 0;
					*curr = (char)strtol(from, NULL, 0);
					from += 5;
				}

				++curr;

				continue;
			}
	
			else if(strncmp(from, "gt", 2) == 0)
				*curr = '>';			
			else if(strncmp(from, "lt", 2) == 0)
				*curr = '<';			
			else if(strncmp(from, "quot", 4) == 0)
				*curr = '"';
			else if(strncmp(from, "amp", 3) == 0)
				*curr = '&';
			else if(strncmp(from, "apos", 3) == 0)
				*curr = '\'';

			++curr;

			from = strchr(from, ';') + 1;

			continue;
		}

		*curr = *from;
		++curr;
		++from;
	}

	return curr - str;
}

static char *CopyStr(HTML_Parser *parser, char *start){

	if(!start) return NULL;

	int len = parser->on - start;

	if(len <= 0) return NULL;

	len = CharacterReferences(start, len);

	char *key = (char *)ParserStackAlloc(parser, len+1);

	memcpy(key, start, len);

	key[len] = 0;

	return key;
}

static void ParseTag(HTML_Parser *parser, HTML_Tag *into){

	Skip(parser, " \r\n\t");

	char *key = parser->on;

	Skip(parser, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");
	
	if(key == parser->on)
		return;

	into->key = CopyStr(parser, key);

	while(parser->on < parser->end){

		Skip(parser, " \r\n\t");

		char *key = parser->on;

		Skip(parser, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");

		ExclusiveSkip(parser, " \r\n\t=>\"'");

		if(*parser->on == '>'){
			++parser->on;
			return;
		}

		if(*parser->on == '='){

			AddTag(parser, &into->attributes)->key = CopyStr(parser, key);

			ExclusiveSkip(parser, ">\"'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-");

			if(*parser->on == '>')
				return;

			++parser->on;
			
			char *start = parser->on;

			if(*(parser->on-1) == '"')
				do { ExclusiveSkip(parser, "\""); } while(*(parser->on - 1) == '\\');
			else
				do { ExclusiveSkip(parser, "'");  } while(*(parser->on - 1) == '\\'); // until it's not escaped

			into->attributes->string = CopyStr(parser, start);
		}

		++parser->on;
	}
}

static void ParseBody(HTML_Parser *parser, HTML_Tag *into){

	parser->contents = parser->on;

	while(parser->on < parser->end){

		if(!ExclusiveSkip(parser, "<")) break;

		AddTag(parser, &into->children)->string = (char *)CopyStr(parser, parser->contents);

		++parser->on;

		Skip(parser, " \r\n\t");

		if(*parser->on == '/' && into->key){
	
			char *start = parser->on + 1;

			ExclusiveSkip(parser, ">");

			++parser->on;

			parser->contents = parser->on;

			if(strncmp(start, into->key, parser->on - (start + 1)) == 0)
				return;
			else
				continue;
		}

		ParseTag(parser, AddTag(parser, &into->children));

		if(!into->children->key)
			longjmp(parser->env, HTML_ERROR_INVALID_SYNTAX);

		if(strcmp(into->children->key, "input") != 0 && strcmp(into->children->key, "meta") != 0 && strcmp(into->children->key, "link") != 0 &&
			strcmp(into->children->key, "img") != 0 && strcmp(into->children->key, "br") != 0 && strcmp(into->children->key, "hr") != 0 &&
			strcmp(into->children->key, "wbr") != 0){

			ParseBody(parser, into->children);
		}

		parser->contents = parser->on;
	}

	AddTag(parser, &into->children)->string = (char *)CopyStr(parser, parser->contents);
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
		.on = memory,
		.end = memory+size,
		.key = NULL,
		.contents = NULL,
		.size = size,
		.stack = stack,
		.stackEnd = stackEnd,
		.align = align,
	};

	int err = setjmp(parser.env);

	if(err < 0) return err;

	Parse(&parser);

	return parser.stack - stack;
}