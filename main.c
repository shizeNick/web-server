#include "vars.h"
#include "httpd.h"
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    int s, c; 
    char *port;
    char *error;
    
    if(argc < 2){
        fprintf(stderr, "Usage: %s <listening port>\n", argv[0]);
        return -1;
    }else{
        port = argv[1];
    }

    s = srv_init(atoi(port));
    if(!s){
        error = get_error();
        fprintf(stderr, error, "%s\n", error);
        return -1;
    }

    printf("Listening on %s:%s\n", LISTENADDR, port);
    while(1){
        c = cli_accept(s);
        if(!c){
            error = get_error();
            fprintf(stderr, "%s\n", error);
            continue;
        }
        
        printf("incomming connection\n");
        if (!(fork())){
            cli_conn(s, c);
        }
            /* for the main process: return the new process' id
             * for the new process: return 0
             * */

    }

    return -1;
}
