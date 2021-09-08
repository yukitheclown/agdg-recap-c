#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

#define BUFFER_SIZE 4096
#define GET_PREFIX "/vg/thread/"

#define CHAN_URL "a.4cdn.org"

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

		if(sock < 0)
			continue;

		if(connect(sock, next->ai_addr, next->ai_addrlen) != -1)
			break;

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

	va_list args;
	va_start(args, format);

	vsnprintf(buffer, sizeof(buffer), format, args);

	va_end(args);

	char header[1024];

	char *text = "GET %s HTTP/1.1\n"
					"Host: %s\n"
					"User-Agent: Mozilla/5.0\n"
					"Accept: */*\n"
					"Connection: keep-alive\n"
					"\r\n\r\n\0";

    sprintf(header, text, buffer, CHAN_URL);

    puts(header);

	write(sock, header, strlen(header) + 1);
}

int main(int argc, char **argv){

	if(argc < 3){

		printf("Usage: %s thread-number output.json\nExample %s 130608694 agdg.json\n"
			"With 130608694 being extracted from\n\"boards.4chan.org/vg/thread/130608694/agdg-amateur-game-developing-general\"\n",
			argv[0], argv[0]);

		return 1;
	}

	char *threadnumber = argv[1];
	char *outfile = argv[2];

	int sock = ConnectAPI();

	if(sock < 0) return 1;

	WriteHeader(sock, "%s%s.json", GET_PREFIX, threadnumber);

	char buffer[BUFFER_SIZE];

	FILE *fp = fopen(outfile, "w");

	memset(buffer, 0, sizeof(buffer));

	int len = read(sock, buffer, BUFFER_SIZE);

	char *pos = strstr(buffer, "\r\n\r\n") + 4;

	int packet = strtol(pos, &pos, 16);

	pos += 2; // skip \r\n

	len -= (pos - buffer);

	printf("%x\n",packet );

	while(!feof(fp)){

		if(packet < len){

			fwrite(pos, 1, packet, fp);

			pos = buffer + packet + 2;

			packet = strtol(pos, &pos, 16);

			pos += 2;

			printf("%x\n", packet);

			if(!packet) break;

			len -= (pos - buffer);

		}

		fwrite(pos, 1, len, fp);

		packet -= len;

		len = read(sock, buffer, BUFFER_SIZE);

		pos = buffer;

	}


    fclose(fp);

    return 0;
}