
#include <stdio.h>
#include <stdarg.h>

static FILE * f_log;
static int log_file_opened = 0;

void Debug_End(void)
{
    fclose(f_log);
    log_file_opened = 0;
}

void Debug_Init(void)
{
    f_log = fopen("log.txt","w");
    if(f_log)
        log_file_opened = 1;
    atexit(Debug_End);
}

void Debug_Log(const char * msg, ...)
{
    if(log_file_opened)
    {
        va_list args;
        va_start(args,msg);
        vfprintf(f_log, msg, args);
        va_end(args);
        fputc('\n',f_log);
        fflush(f_log);
    }
}
