#include <stdio.h>
#include <stdarg.h>
#include <png.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define IMAGES_DIR "images/"
#define POST_NO "no"
#define POST_COM "com"
#define POST_IMG_URL "tim"
#define POST_IMG_EXT "ext"
#define POST_NAME "name"
#define REQUIRED_NAME "RECAP"
#define CHAN_URL "i.4cdn.org"
#define GET_PREFIX "/vg/"
#define IMAGE_BUFFER_SIZE 4096

typedef struct Post Post;

struct Post {
	char *com;
	char *img;
	char *ext;
	Post *next;
};

static int LoadJSON(char *data, int *index, Post **currPost){

	char **into = NULL;
	char *number = NULL;

	for(; data[*index]; (*index)++){

		char character = data[*index];
		char *str = &data[*index];

		if(character >= '0' && character <= '9' && !number){
			number = str;
			continue;
		}

		if(number){

			if(character >= '0' && character <= '9')
				continue;

			if(into){
				*into = number;
				into = NULL;
			}

			number = NULL;
		}

		// if(character == '{' || character == '['){
		// 	*index += 1;
		// 	LoadJSON(data, index, currPost);
		// 	continue;
		// }

		// if(character == '}' || character == ']'){
		// 	++(*index);
		// 	return 1;
		// }

		if(character == '"'){

			char *end = str;

			char *begin = str+1;

			do {
				
				str = end+1;

				end = strchr(str, '"');

			} while(end && *(end - 1) == '\\'); // for escaped '"'s \"

			if(!end)
				return -1; // error

			*index += (end - begin) + 1;

			*end = 0;

			if(into){
				*into = begin;
				into = NULL;
				continue;
			}

			if(strcmp(begin, POST_NAME) == 0 && 
			// if(strcmp(begin, POST_NAME) == 0 && strcmp(begin, REQUIRED_NAME) == 0&&
				(*currPost)->com && (*currPost)->img && (*currPost)->ext){

				Post *next = *currPost;

				*currPost = (Post *)malloc(sizeof(Post));

				memset(*currPost, 0, sizeof(Post));

				(*currPost)->next = next;

			} else if(strcmp(begin, POST_COM) == 0){

				into = &(*currPost)->com;

			} else if(strcmp(begin, POST_IMG_URL) == 0){
			
				into = &(*currPost)->img;

			} else if(strcmp(begin, POST_IMG_EXT) == 0){
			
				into = &(*currPost)->ext;
			} 
		}
	}

	return 1;
}

int ConnectAPI(void){

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	struct addrinfo *result;

	int res = getaddrinfo(CHAN_URL, "80", &hints, &result);

	if(res != 0){
		printf("Error: getaddrinfo %s\n", gai_strerror(res));
		return -1;
	}

	int sock = -1;

	struct addrinfo *next = result;

	while(next){

		sock = socket(next->ai_family, next->ai_socktype, next->ai_protocol);

		if(sock < 0){
			next = next->ai_next;
			continue;
		}

		if(connect(sock, next->ai_addr, next->ai_addrlen) != -1)
			break;
		else
			sock = -1;

		next = next->ai_next;
	}

	freeaddrinfo(result);

	if(sock < 0){
		printf("Error: connecting\n");
		return -1;
	}

	return sock;
}

void WriteHeader(int sock, char *format, ...){

	char buffer[128];

	memset(buffer, 0, sizeof(buffer));

	va_list args;
	va_start(args, format);

	vsnprintf(buffer, sizeof(buffer), format, args);

	va_end(args);

	char header[2048];

	memset(header, 0, sizeof(header));

	char *text = "GET %s HTTP/1.1\n"
					"Host: %s\n"
					"User-Agent: Mozilla/5.0\n"
					"Accept: */*\n"
					"Accept-Encoding: gzip, deflate\n"
					"Connection: keep-alive\n"
					"\r\n\r\n\0";

    sprintf(header, text, buffer, CHAN_URL);

	write(sock, header, strlen(header) + 1);
}

void GetImage(char *imageBuffer, Post *post){
	
	int sock = ConnectAPI();

	if(sock <= 0) return;

	post->img[13] = 0;

	WriteHeader(sock, "%s%s%s", GET_PREFIX, post->img, post->ext);

	int len = read(sock, imageBuffer, IMAGE_BUFFER_SIZE);

	char *search = "Content-Length: ";

	char *pos = strstr(imageBuffer, search) + strlen(search);

	int total = strtol(pos, &pos, 0);

	pos = strstr(imageBuffer, "\r\n\r\n") + 4;

	len -= (pos - imageBuffer);

	char path[1024];

	sprintf(path, "%s%s%s", IMAGES_DIR, post->img, post->ext);

	FILE *img = fopen(path, "wb");

	fwrite(pos, 1, len, img);

	total -= len;

	while(len){

		len = read(sock, imageBuffer, IMAGE_BUFFER_SIZE);

		fwrite(imageBuffer, 1, len, img);
	}

	fclose(img);

	close(sock);
}

static unsigned char *LoadPNG(char *path, int *w, int *h){

    FILE *fp = fopen(path,"rb");

    if(fp == NULL){
        printf("Error loading PNG %s: No such file.\n", path);
        return NULL;
    }

    unsigned char header[8];
    fread(header, 1, 8, fp);
    int ispng = !png_sig_cmp(header, 0, 8);

    if(!ispng){
        fclose(fp);
        printf("Not png %s\n", path);
        return NULL;
    }

    png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    png_infop infoPtr = png_create_info_struct(pngPtr);

    setjmp(png_jmpbuf(pngPtr));

    png_set_sig_bytes(pngPtr, 8);
    png_init_io(pngPtr, fp);
    png_read_info(pngPtr, infoPtr);

    int bitDepth, colorType;
    png_uint_32 twidth, theight;

    png_get_IHDR(pngPtr, infoPtr, &twidth, &theight, &bitDepth, &colorType, NULL, NULL, NULL);

    if(colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(pngPtr);

    if(colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(pngPtr);

    png_get_IHDR(pngPtr, infoPtr, &twidth, &theight, &bitDepth, &colorType, NULL, NULL, NULL);

    if(png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(pngPtr);

    if(bitDepth < 8)
        png_set_packing(pngPtr);

    if(colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY)
        png_set_add_alpha(pngPtr, 255, PNG_FILLER_AFTER);

    png_get_IHDR(pngPtr, infoPtr, &twidth, &theight, &bitDepth, &colorType, NULL, NULL, NULL);

    *w = twidth;
    *h = theight;

    png_read_update_info(pngPtr, infoPtr);

    int rowbytes = png_get_rowbytes(pngPtr, infoPtr);

    png_byte *imageData = (png_byte *)malloc(sizeof(png_byte) * rowbytes * theight);
    png_bytep *rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * theight);

    int i;
    for(i = 0; i < (int)theight; ++i)
        rowPointers[theight - 1 - i] = imageData + i * rowbytes;

    png_read_image(pngPtr, rowPointers);
    png_read_end(pngPtr, NULL);

    png_destroy_read_struct(&pngPtr, &infoPtr, NULL);

    free(rowPointers);

    fclose(fp);

    return (unsigned char *)imageData;
}


static void SavePNG(unsigned char *pixels, int width, int height, const char *path){
    
    FILE *fp = fopen(path, "wb");

    png_structp pngPtr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    png_infop infoPtr = png_create_info_struct (pngPtr);
    
    setjmp(png_jmpbuf(pngPtr));

    png_set_IHDR (pngPtr,
                  infoPtr,
                  width,
                  height,
                  8,
                  PNG_COLOR_TYPE_RGBA,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT);

    png_byte **rowPointers = png_malloc (pngPtr, height * sizeof (png_byte *));

    int x, y;
    for (y = 0; y < height; ++y) {
        
        png_byte *row = png_malloc (pngPtr, sizeof (uint8_t) * width * 4);
        
        rowPointers[y] = row;

        for (x = 0; x < width; ++x) {

			int index = ((y * width) + x) * 4;
        
            *row++ = pixels[index];
            *row++ = pixels[index+1];
            *row++ = pixels[index+2];
            *row++ = pixels[index+3];
        }
    }
    
    png_init_io (pngPtr, fp);
    png_set_rows (pngPtr, infoPtr, rowPointers);
    png_write_png (pngPtr, infoPtr, PNG_TRANSFORM_IDENTITY, NULL);

    for (y = 0; y < height; y++) 
        png_free(pngPtr, rowPointers[y]);
    
    png_free(pngPtr, rowPointers);
    

    fclose(fp);
}

int main(int argc, char **argv){

	if(argc < 4){
		printf("Usage: %s bg.png entry.png input.json\n", argv[0]);
		return 1;
	}


	int baseWidth, baseHeight;
	int entryWidth, entryHeight;
	
	unsigned char *entry = LoadPNG(argv[2], &entryWidth, &entryHeight);
	unsigned char *base = LoadPNG(argv[1], &baseWidth, &baseHeight);

	if(!entry || !base) return 1;

	int entriesPerWidth = baseWidth / (entryWidth * 2);

	int nEntries = 2;

	int k;
	for(k = 0; k < nEntries; k++){

		int x = (entryWidth * 0.5) + ((k % entriesPerWidth) * (entryWidth * 1.5));
		
		int y = 512 + (entryHeight * 0.5) + ((k / entriesPerWidth) * (entryHeight * 1.5));

		int imgx, imgy;

		for(imgx = 0; imgx < entryWidth; imgx++){
			
			for(imgy = 0; imgy < entryHeight; imgy++){

				int index = (((y+imgy) * baseWidth) + (x+imgx)) * 4;

				int eIndex = ((imgy * entryWidth) + imgx) * 4;

				memcpy(&base[index], &entry[eIndex], 4);
			}
		}
	}


	SavePNG(base, baseWidth, baseHeight, "final.png");


	return 0;

	char *infile = argv[2];

	FILE *fp = fopen(infile, "rb");

	fseek(fp, 0, SEEK_END);

	int len = ftell(fp);

	rewind(fp);

	char *buffer = (char *)malloc(len + 1);

	buffer[len] = 0;

	fread(buffer, 1, len, fp);

	Post *post = malloc(sizeof(Post));

	memset(post, 0, sizeof(Post));

	int index = 0;

	LoadJSON(buffer, &index, &post);

	char imageBuffer[IMAGE_BUFFER_SIZE];

	while(post){

		if(strcmp(post->ext, ".png") != 0){
			post = post->next;
			continue;
		}

		// if(post->com)
		// 	printf("%s\n", post->com);
	
		GetImage(imageBuffer, post);

		Post *next = post->next;

		free(post);

		post = next;
	}

	free(buffer);

	fclose(fp);

    return 0;
}