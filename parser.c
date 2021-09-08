#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "json.h"
#include "html.h"

enum {
	COLOR_NORMAL = 0,
	COLOR_RED = 31,
	COLOR_GREEN = 32,
	COLOR_YELLOW = 33,
	COLOR_BLUE = 34,
	COLOR_MAGENTA = 35,
	COLOR_CYAN = 36,
};

#define MAX_POSTS 1024
#define ALIGNMENT 8

typedef struct {
	HTML_Tag 	*comment;
	char 		*name;
	char 		*trip;
	char 		*filename;
	int 		image;
	int			imgW;
	int			imgH;
	char 		*now;
} Post4Chan;

static void *stack;
static int stackSize;
static Post4Chan posts[MAX_POSTS];
static int numPosts;

void DumpPosts(Post4Chan *post, JSON_Value *value, int *num){

	if(!value) return;

	// if(value->type != JSON_ARRAY && value->type != JSON_OBJECT){
	if(value->type != JSON_ARRAY){
		
		if(strcmp(value->key, "now") == 0)
			post->now = value->string;
		else if(strcmp(value->key, "tn_h") == 0)
			post->imgH = atoi(value->string);
		else if(strcmp(value->key, "tn_w") == 0)
			post->imgW = atoi(value->string);
		else if(strcmp(value->key, "name") == 0)
			post->name = value->string;
		else if(strcmp(value->key, "trip") == 0)
			post->trip = value->string;
		else if(strcmp(value->key, "com") == 0){

			int ret = HTML_Parse(&post->comment, value->string, strlen(value->string),
				stack, stack + stackSize, ALIGNMENT);
			
			if(ret < 0){
				
				// FUUUUUUUUUUUUUUUUUUUUUUUUCK

			} else {

				post->comment = post->comment->children;

				stack += ret;
				stackSize -= ret;
			}
		}

		DumpPosts(post, value->next, num);

		return;
	}

	DumpPosts(post + 1, value->next, num);

	memset(post, 0, sizeof(Post4Chan));

	DumpPosts(post, value->children, num);

	++(*num);
}

void Dump(JSON_Value *value){

	if(!value) return;

	if(value->type == JSON_ARRAY && value->key && strcmp(value->key, "posts") == 0){
		numPosts = 0;
		memset(posts, 0, sizeof(posts));
		DumpPosts(posts, value->children, &numPosts);
		return;
	}

	Dump(value->next);
	Dump(value->children);
}

void PrintComment(HTML_Tag *tag){

	if(tag->next)
		PrintComment(tag->next);

	if(tag->string){
		printf("%s", tag->string);
		printf("\x1b[%im", COLOR_NORMAL);
	}

	if(tag->key){
		
		if(strcmp(tag->key, "br") == 0){
			printf("\n");
		}
		else if(strcmp(tag->key, "a") == 0){
			printf("\x1b[%im", COLOR_RED);
		}
	}

	if(tag->children)
		PrintComment(tag->children);

}

void PrintPosts(void){

	int k;
	for(k = 0; k < numPosts; k++){

		int m;
		for(m = 0; m < 60; m++)
			printf("\x1b[%im-", COLOR_YELLOW);

		printf("\n");

		Post4Chan *post = &posts[k];
	
		if(post->trip)
			printf("\x1b[%im%s%s\t", COLOR_CYAN, post->name, post->trip);
		else
			printf("\x1b[%im%s\t", COLOR_CYAN, post->name);
		
		if(post->filename)
			printf("\t\x1b[%im%s:%i%i\t", COLOR_MAGENTA, post->filename, post->imgW, post->imgH);

		printf("\x1b[%iG", 38);

		printf("\x1b[%im%s\n", COLOR_GREEN, post->now);

		for(m = 0; m < 60; m++)
			printf("\x1b[%im-", COLOR_YELLOW);

		printf("\x1b[%im", COLOR_NORMAL);
		printf("\n");

		// printf("%s\n",post->comment );

		if(post->comment)
			PrintComment(post->comment);

		printf("\n");
		printf("\n");
		printf("\x1b[%im", COLOR_NORMAL);
	}
}

int main(){

	FILE *fp = fopen("result3.json", "rb");

	fseek(fp, 0, SEEK_END);

	int len = ftell(fp);

	rewind(fp);

	char *buffer = (char *)malloc(len);

	fread(buffer, 1, len, fp);

	fclose(fp);

	JSON_Value *top;

	stackSize = (0x01 << 20) * 32;
	char *memory = malloc(stackSize);

	stack = memory;

	int ret = JSON_Parse(&top, buffer, len, stack, stack + stackSize, ALIGNMENT);

	JSON_Dump(top, 0);

	memset(posts, 0, sizeof(posts));

	// if(ret < 0)
	// 	printf("%s\n", JSON_Error(ret));
	// else {
		stack += ret;
		stackSize -= ret;
		Dump(top);
		PrintPosts();
	// }

	free(buffer);
	free(memory);
}