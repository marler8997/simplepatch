#include <windows.h>

static int errno;
void AToW(WCHAR* dest, const char* src)
{
    for(size_t i = 0; ; i++)
    {
        char c = src[i];
        dest[i] = c;
        if(c == '\0')
        {
            return;
        }
    }
}
size_t WToA(char* dest, const WCHAR* src)
{
    for(size_t i = 0; ; i++)
    {
        WCHAR c = src[i];
        dest[i] = (char)c;
        if(c == '\0')
        {
            return i;
        }
    }
}
int remove(const char* filename)
{
    WCHAR *filenameW = (WCHAR*)alloca(2*(strlen(filename)+1));
    AToW(filenameW, filename);
    if(!DeleteFile(filenameW))
    {
        errno = GetLastError();
        return -1;
    }
    return 0;
}
int rename(const char* old, const char* new_)
{
    size_t oldLength = strlen(old);
    WCHAR *oldW = (WCHAR*)alloca(sizeof(WCHAR)*(oldLength + strlen(new_) + 2));
    WCHAR *newW = oldW + (oldLength + 1);
    AToW(oldW, old);
    AToW(newW, new_);
    if(!MoveFile(oldW, newW))
    {
        errno = GetLastError();
        return -1;
    }
    return 0;
}
int main(int argc, const char* argvWide[]);
int wmain(int argc, const WCHAR* argvWide[])
{
    char* buffer;
    {
        size_t argLength = 0;
        for(int i = 1; i < argc; i++)
        {
            argLength += wcslen(argvWide[i]);
        }
        buffer = (char*)malloc(sizeof(char)*argLength);
    }

    char** argvAscii = (char**)malloc(sizeof(char*) * argc);
    if(!argvAscii)
    {
        printf("Error: malloc failed\n");
        return 1;
    }
    argvAscii[0] = "simplepatch";
    size_t offset = 0;
    for(int i = 1; i < argc; i++)
    {
        offset += WToA(&buffer[offset], argvWide[i]);
    }

    int status = main(argc, (const char**)argvAscii);
    free(argvAscii);
    free(buffer);
    return status;
}