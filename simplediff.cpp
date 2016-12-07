#include <stdio.h>
#include <string.h>
#include <errno.h>

struct File
{
	FILE *fp;
	File(FILE *fp) : fp(fp)
	{
	}
	~File()
	{
		if(fp) fclose(fp);
	}
};

//void processLine(

void usage()
{
	printf("Usage: compilepatch <file>\n");
}


#define LITERAL_LENGTH(lit) sizeof(lit)-1

bool isspace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}



int main(int argc, const char* argv[])
{
	char buffer[4096];

	if(argc <= 1)
	{
		usage();
		return 0;
	}

	if(argc > 2)
	{
		printf("Error: too many command line arguments\n");
		return 1;
	}

	const char* filename = argv[1];
	File file(fopen(filename, "rb"));
	if(file.fp == NULL)
	{
		printf("Error: failed to open file \"%s\" (e=%d)\n", filename, errno);
		return 1;
	}


	size_t offset = 0;
	size_t leftToInject = 0;
	size_t lineNumber = 1;
	while(true)
	{
		size_t lastRead = fread(buffer + offset, 1, sizeof(buffer)-offset, file.fp);
		if(lastRead == 0)
		{
			if(offset > 0 || leftToInject > 0)
			{
				printf("Error: file ended too early\n");
				return 1;
			}
			break;
		}

		if(leftToInject > 0)
		{
			printf("not implemented\n");
			return 1;
		}
		else if(lastRead > 5) // Minimum command is 5 characters
		{
			size_t consumed = 0;
			if(memcmp(buffer, "copy ", 5) == 0)
			{
				printf("copy not implemented\n");
				return 1;
			}
			else if(memcmp(buffer, "skip ", 5) == 0)
			{
				printf("copy not implemented\n");
				return 1;
			}
			else if(memcmp(buffer, "inject ", 5) == 0)
			{
				printf("copy not implemented\n");
				return 1;
			}
			else if(memcmp(buffer, "flush", 5) == 0)
			{
				if(buffer[6] != '\n')
				{
					printf("%s(%u): invalid command, expected newline after 'flush'\n",
						filename, lineNumber);
				}
				
				printf("flush not implemented\n");
				return 1;
			}
			else
			{
				int endOfCommand;
				for(endOfCommand = 5; endOfCommand < lastRead; endOfCommand++)
				{
					if(isspace(buffer[endOfCommand])) {
						break;
					}
				}
				printf("%s(%u): unknown command '%.*s'\n", filename, lineNumber,
					endOfCommand, buffer);
				return 1;
			}
		}
	}

	return 0;
}