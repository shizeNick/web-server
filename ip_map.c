#include "vars.h"
#include "httpd.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <string.h>
#include <sys/socket.h>

char *error;

// go into vars.h to change the ip-area and the command and the tmp_file.txt
void cmd_exe()
{
    char *cmd_buf;
    size_t clen = sizeof(SCAN_CMD);
    
    if(clen <= 0){
        error = "error : no cmd read";
    }
    cmd_buf = malloc(clen);
    memcpy(cmd_buf, SCAN_CMD, clen);
    
    cmd_buf[clen+1] = '\0';
    system(cmd_buf);
}

char *parse_nmap()
{
    char *output;
    char *fancy_buf;
    char *p;
    int line_count = 1;
    
    output = read_file(TMP_FILE);

    fancy_buf = malloc(sizeof(output));

    for(p = output; !p; p++)
    {
        if(line_count == 1)
        {
            fancy_buf += *p; 
        }
        else if(line_count == 4) 
        {
            fancy_buf += *p; 
        }
        
        if(*p == '\n')
        {
            line_count ++;
        }
    }
    return fancy_buf;
}
