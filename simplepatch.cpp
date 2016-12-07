#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32_WCE
#include "simplepatch_wce.cpp"
#endif

typedef unsigned char ubyte;

struct File
{
    FILE *fp;
    File() : fp(NULL)
    {
    }
    File(FILE *fp) : fp(fp)
    {
    }
    ~File()
    {
        if(fp) fclose(fp);
    }
};
struct FreeOnExitScope
{
    void* ptr;
    FreeOnExitScope() : ptr(NULL)
    {
    }
    FreeOnExitScope(void* ptr) : ptr(ptr)
    {
    }
    ~FreeOnExitScope()
    {
        if(ptr) free(ptr);
    }
};

inline size_t __fwrite(ubyte* buffer, size_t size, FILE* fp)
{
    //printf("Writing %d bytes \"%.*s\"\n", size, size, buffer);
    return fwrite(buffer, 1, size, fp);
}

// Returns: false on error, logs error to console
bool FlushFile(ubyte* buffer, size_t bufferSize, FILE* dest, FILE* src)
{
    while(true)
    {
        size_t lastRead = fread(buffer, 1, bufferSize, src);
        if(lastRead == 0)
        {
            return true; // success
        }
        size_t written = __fwrite(buffer, lastRead, dest);
        if(written != lastRead)
        {
            printf("Error: fwrite (size=%u) returned %u (e=%d)\n", lastRead, written, errno);
            return false; // fail
        }
    }
}

// Returns: false on error, logs error to console
bool Copy(ubyte* buffer, size_t bufferSize, FILE* dest, FILE* src, size_t size)
{
    while(size > 0)
    {
        size_t readSize = (size > bufferSize) ? bufferSize : size;
        size_t lastRead = fread(buffer, 1, readSize, src);
        if(lastRead == 0)
        {
            printf("Error: fread (size=%u) failed (e=%d)\n", readSize, errno);
            return false; // fail
        }
        size_t written = __fwrite(buffer, lastRead, dest);
        if(written != lastRead)
        {
            printf("Error: fwrite (size=%u) returned %u (e=%d)\n", lastRead, written, errno);
            return false; // fail
        }
        size -= lastRead;
    }
    return true;
}

struct PatchReader
{
    FILE *fp;
    ubyte* buffer;
    size_t bufferSize;
    size_t offset;
    size_t limit;
    PatchReader(FILE *fp, ubyte* buffer, size_t bufferSize) :
        fp(fp), buffer(buffer), bufferSize(bufferSize), offset(0), limit(0)
    {
    }

    // Assumption: only called if there is no data (offset >= limit)
    // Returns: false on error or end of file
    bool Read()
    {
        offset = 0;
        limit = fread(buffer, 1, bufferSize, fp);
        if(limit == 0)
        {
            if(ferror(fp))
            {
                printf("Error: fread (size=%u) failed (e=%d)\n", bufferSize, errno);
            }
            return false; // fail
        }
        return true; // success
    }

    // Returns: -1 on error or EOF
    int ReadByte()
    {
        if(limit <= offset)
        {
            if(!Read())
            {
                return -1;
            }
        }
        return buffer[offset++];
    }
    void SkipOptionalNewline()
    {
        if(limit <= offset)
        {
            if(!Read())
            {
                return;
            }
        }
        if(buffer[offset] == '\n')
        {
            offset++;
        }
    }

private:
    static void PrintLengthError(ubyte c)
    {
        if(c == '\r')
        {
            printf("Error: command cannot have a '\\r' at the end of the line\n");
        }
        else
        {
            printf("Error: expected length but got '%c' (code=%d 0x%02x)\n", c, c, c);
        }
    }
public:
    // Returns: false on error, logs errors to console
    bool ReadLength(size_t* outLength)
    {
        size_t length = 0;

        while(true)
        {
            int nextByte = ReadByte();
            if(nextByte < 0)
            {
                if(feof(fp))
                {
                    if(length == 0)
                    {
                        printf("Error: patch file ended while reading a command length\n");
                        return false; // fail
                    }
                    *outLength = length;
                    return true; // success
                }
                // error already printed
                return false; // fail
            }
            if(nextByte < '0')
            {
                if(nextByte == '\n')
                {
                    if(length == 0)
                    {
                        printf("Error: command is missing it's length\n");
                        return false; // fail
                    }
                    *outLength = length;
                    return true; // success
                }
                PrintLengthError((ubyte)nextByte);
                return false; // fail
            }

            if(nextByte <= '9')
            {
                if(length == 0 && nextByte == '0')
                {
                    printf("Error: length cannot start with '0'\n");
                    return false; // fail
                }
                length *= 10;
                length += nextByte - '0';
            }
            else
            {
                PrintLengthError((ubyte)nextByte);
                return false; // fail
            }
        }
    }
    // Returns: false on failure
    bool Inject(FILE* dest, size_t injectLength)
    {
        size_t dataReady = limit - offset;
        if(dataReady)
        {
            size_t dataToWrite = (injectLength <= dataReady) ? injectLength : dataReady;
            {
                size_t written = __fwrite(buffer + offset, dataToWrite, dest);
                if(written != dataToWrite)
                {
                    printf("Error: fwrite (size=%u) returned %u (e=%d)\n", dataToWrite, written, errno);
                    return false; // fail
                }
            }
            if(injectLength <= dataReady)
            {
                offset += injectLength;
                return true;
            }

            offset = 0;
            limit = 0;
            injectLength -= dataToWrite;
        }
        return Copy(buffer, bufferSize, dest, fp, injectLength);
    }
};

char* MakeFilenameWithPostfix(const char* filename, size_t filenameLength, const char* postfix, size_t postfixLength)
{
    char* buffer = (char*)malloc(filenameLength + postfixLength + 1);
    if(!buffer)
    {
        printf("Error: malloc failed (e=%d)\n", errno);
    }
    else
    {
        memcpy(buffer                 , filename, filenameLength);
        memcpy(buffer + filenameLength, postfix, postfixLength);
        buffer[filenameLength+postfixLength] = '\0';
    }
    return buffer;
}

void Usage()
{
    printf("Usage: simplepatch <patch-file> <old-file> [new-file]\n");
}


int main(int argc, const char* argv[])
{
    if(argc <= 1)
    {
        Usage();
        return 0;
    }

    if(argc > 4)
    {
        Usage();
        printf("\nError: too many command line arguments\n");
        return 1;
    }

    if(argc <=2)
    {
        Usage();
        printf("\nError: not enough command line arguments\n");
        return 1;
    }

    const char* patchFilename = argv[1];
    const char* oldFilename = argv[2];
    bool usingTempFile;
    const char* newFilename;
    FreeOnExitScope newFilenameBuffer;
    if(argc >= 4)
    {
        usingTempFile = false;
        newFilename = argv[3];
    }
    else
    {
        usingTempFile = true;
        newFilename = MakeFilenameWithPostfix(oldFilename, strlen(oldFilename), ".patched", sizeof(".patched")-1);
        newFilenameBuffer.ptr = (void*)newFilename;
    }

    {
        File patchFile(fopen(patchFilename, "rb"));
        if(patchFile.fp == NULL)
        {
            printf("Error: failed to open patch file \"%s\" (e=%d)\n", patchFilename, errno);
            return 1;
        }

        File oldFile(fopen(oldFilename, "rb"));
        if(oldFile.fp == NULL)
        {
            printf("Error: failed to open file \"%s\" (e=%d)\n", oldFilename, errno);
            return 1;
        }

        // TODO: check what happens if the new file is the same as the old file
        File newFile;
        if(newFilename)
        {
            newFile.fp = fopen(newFilename, "wb");
            if(newFile.fp == NULL)
            {
                printf("Error: failed to create new file \"%s\" (e=%d)\n", newFilename, errno);
                return 1;
            }
        }
        else
        {
            printf("no new file not implemented\n");
            return 1;
        }


        ubyte patchReadBuffer[4096];
        ubyte oldFileReadBuffer[4096];
        PatchReader patchReader(patchFile.fp, patchReadBuffer, sizeof(patchReadBuffer));
        size_t commandCount = 0;

        while(true)
        {
            commandCount++;
            int command = patchReader.ReadByte();
            if(command < 0)
            {
                if(ferror(patchReader.fp))
                {
                    return 1; // fail
                }
                break;
            }
            switch(command)
            {
            case 'c': // copy
                {
                    size_t length;
                    if(!patchReader.ReadLength(&length))
                    {
                        printf("Error: bad patch file, input ended early\n");
                        return 1;
                    }
                    if(!Copy(oldFileReadBuffer, sizeof(oldFileReadBuffer), newFile.fp, oldFile.fp, length))
                    {
                        printf("Error: copy %u failed\n", length);
                        return 1;
                    }
                }
                break;
            case 's': // skip
                {
                    size_t length;
                    if(!patchReader.ReadLength(&length))
                    {
                        printf("Error: bad patch file, input ended early\n");
                        return 1;
                    }
                    if(-1 == fseek(oldFile.fp, length, SEEK_CUR))
                    {
                        printf("Error: fseek (offset=%u) failed (e=%d)\n", length, errno);
                        return 1;
                    }
                }
                break;
            case 'i': // inject
                {
                    size_t length;
                    if(!patchReader.ReadLength(&length))
                    {
                        printf("Error: bad patch file, input ended early\n");
                        return 1;
                    }
                    if(!patchReader.Inject(newFile.fp, length))
                    {
                        printf("Error: inject %u failed\n", length);
                        return 1;
                    }
                    patchReader.SkipOptionalNewline();
                }
                break;
            case 'f': // flush
                {
                    int newline = patchReader.ReadByte();
                    if(newline < 0)
                    {
                        if(ferror(patchReader.fp))
                        {
                            return 1; // fail
                        }
                    }
                    else
                    {
                        if(newline != '\n')
                        {
                            printf("Error: the flush 'f' command should be followed by a newline '\n' 0x0a, but got '%c' 0x%02x\n",
                                newline, newline);
                            return 1; // fail
                        }
                    }

                    if(!FlushFile(oldFileReadBuffer, sizeof(oldFileReadBuffer), newFile.fp, oldFile.fp))
                    {
                        printf("Error: flush failed\n");
                        return 1;
                    }
                }
                break;
            default:
                printf("Error: invalid command %d (0x%02x) ('%c') (command number %u)\n", command, command, command, commandCount);
                return 1;
            }
        }
    }

    if(usingTempFile)
    {
        FreeOnExitScope tempFilenameBuffer(MakeFilenameWithPostfix(oldFilename, strlen(oldFilename), ".backup", sizeof(".backup")-1));
        if(tempFilenameBuffer.ptr == NULL)
        {
            return 1; // fail
        }
        const char* backupFilename = (char*)tempFilenameBuffer.ptr;

        if(0 == remove(backupFilename))
        {
            printf("Deleted backup file '%s'\n", backupFilename);
        }

        printf("Backing up original file '%s' to '%s'...\n", oldFilename, backupFilename);
        if(-1 == rename(oldFilename, backupFilename))
        {
            printf("Error: failed to backup original file '%s' to '%s' (e=%d)\n", oldFilename, backupFilename, errno);
            return 1; // fail
        }
        printf("Moving temporary file '%s' to '%s'...\n", newFilename, oldFilename);
        if(-1 == rename(newFilename, oldFilename))
        {
            printf("Error: failed to move temporary file '%s' to '%s' (e=%d)\n", newFilename, oldFilename, errno);
            return 1; // fail
        }

        if(0 == remove(backupFilename))
        {
            printf("Deleted backup file '%s'\n", backupFilename);
        }
    }

    return 0;
}