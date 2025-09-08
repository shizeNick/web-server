/*httpd.c*/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define LISTENADDR "127.0.0.1"

/* struct's */
struct sHttpRequest{
    char method[8];
    char url[128];
    int content_length;
};

typedef struct sHttpRequest httpreq;

/* global */
char *error;

/* returns 0 on error, or it returns a socket fd */
int srv_init(int portno){
    int s;
    struct sockaddr_in srv;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0){
        error = "socket() error";
        return 0;
    }
   
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = inet_addr(LISTENADDR);
    srv.sin_port = htons(portno);
    
    if(bind(s, (struct sockaddr *)&srv, sizeof(srv))){
        close(s);
        error = "bind() error";
        return 0; 
    }

    if (listen(s,5)){
        close(s);
        error = "listen() error";
        return 0;
    }
    
    return s;
}

/* return 0 on error, or returns the client socketfd */
int cli_accept(int s){
    int c;
    socklen_t addrlen;
    struct sockaddr_in cli;
    
    addrlen = 0;
    memset(&cli, 0, sizeof(cli));
    c = accept(s, (struct sockaddr*)&cli, &addrlen);
    if(c < 0){
        error = "accept() error";
        return 0;
    }
    return c;
}

char** split_string(char *str, char *delimiter)
{
    char *p = str;
    int header_count = 0;
    char *next_delimiter;
    
    // count the headers
    while((next_delimiter = strstr(p, delimiter)) != NULL){
        header_count ++;
        p = next_delimiter + strlen(delimiter);
    }
    // if something is there yet, count it 
    if (*p != '\0') {
        header_count++;
    }
    
    // memory allocation for the found headers
    char **header = malloc(sizeof(char*) * (header_count + 1));
    if(header == NULL){
        return NULL;
    }

    p = str;
    int i = 0;

    while((next_delimiter = strstr(p, delimiter)) != NULL){

        size_t len = next_delimiter - p;

        header[i] = malloc(len +1); // + 1 for null-terminator
        if(header[i] == NULL){
            error = "error split_string() (headers)";
            return NULL;
        }

        strncpy(header[i], p, len);
        header[i][len] = '\0'; // add Null-Terminator 
        
        // jump to the next header
        p = next_delimiter + strlen(delimiter);
        i++;
    } 
    if (*p != '\0') {
       
        size_t len = next_delimiter - p;
        header[i] = malloc(len +1);
        if(header[i] == NULL){
            error = "error split_string() (headers)";
            return NULL;
        }
        strncpy(header[i], p, len);
        header[i][len] = '\0';
        i++;
    }

    // mark the end of the array
    header[i] = NULL;

    return header;
}

// this function looks in in Header-Array for a specific Header name
/* return 0 on error, or returns the Header value */
char* get_header_value(char** headers, const char* header_name) {
    for (int i = 0; headers[i] != NULL; i++) {
        // Vergleich den Anfang des Strings mit dem Header-Namen
        if (strncmp(headers[i], header_name, strlen(header_name)) == 0) {
            // Finde den Wert nach dem Doppelpunkt und dem Leerzeichen
            char* value = headers[i] + strlen(header_name) + 1;
            // Entferne führende Leerzeichen
            while (*value == ' ') {
                value++;
            }
            return value;
        }
    }
    return 0; // Header wurde nicht gefunden
}
/* returns 0 on error or returns httpreq structure */
httpreq *parse_http(char *str){
    
    char *p, *c;
    httpreq *request;
    char **h  = split_string(str, "\r\n");

    request = malloc(sizeof(httpreq));
    // METHOD-Parser
    for(p = str; *p && *p != ' '; p++);
    if(*p == ' ')
       *p = 0;  
    else{
        error = "parse_http() NOSPACE error";
        free(request);
        return 0;
    }
    strncpy(request->method, str, 7);
    
    // URL-Parser
    for(str=++p; *p && *p != ' '; p++);
    if(*p == ' ')
       *p = 0;  
    else{
        error = "parse_http() NO2ndSPACE error";
        free(request);
        return 0;
    }
    strncpy(request->url, str, 127);
  
    // subdivide the req in his headers
    char **header = split_string(str, "\r\n");
    // search the Content-Length Header
    char *content_length_str = get_header_value(header, "Content-Length");

    if (content_length_str != NULL) {
        request->content_length = atoi(content_length_str);
    } else {
        request->content_length = 0;
    }


    return request;
}

/* return 0 on error or return the data */
char *cli_read(int c)
{
    static char buf[512];
    memset(buf, 0, 512);
    if (read(c, buf, 511) < 0){
        error = "read() error";
        return 0;
    }else{
        return buf;
    }
}

/*
        "HTTP/1.0 %d OK\n"
        "Server: httpd.c\n"
        "Cache-Control: no-store, no-cache, max-age=0, private\n"
        "Content-Type: text/html\n"
        "Expires: -1\n"
        "X-Frame-Language: en\n"
        "Content-Length: 12\n"
*/

void http_response(int c, char *content_type, char *data)
{
    char buf[512];
    int n;
    
    n = strlen(data);
    memset(buf, 0, 512);
    snprintf(buf, 511, 
        "Content-Type: %s\n"
        "Content-Length: %d\n"
        "\n%s\n",
        content_type, n, data);
    n = strlen(buf);
    write(c, buf, n);

    return;
}

void http_header(int c, int status)
{
    char buf[512];
    int n;

    memset(buf, 0, 512);
    snprintf(buf, 511, 
        "HTTP/1.0 %d OK\n"
        "Server: httpd.c\n"
        "Cache-Control: no-store, no-cache, max-age=0, private\n"
        "Content-Language: en\n"
        "Expires: -1\n"
        "X-Frame-Language: SAMEORIGIN\n", 
        status);

    n = strlen(buf);
    write(c, buf, n);

    return;
}

void cli_conn(int s, int c)
{
    httpreq *req;
    char cli_data[512];
    char *p; // cli_read req buf
    char *res; // server http-header buf
    

    p = cli_read(c);
    if(!p){
        fprintf(stderr, "%s\n", error);
        close(c);
        return;
    }
    req = parse_http(p);
    if(!req){
        fprintf(stderr, "%s\n", error);
        close(c);
        return;
    }
    //printf("'%s'\n'%s'\n", req->method, req->url);
    
    if(!strcmp(req->method, "GET") && !strcmp(req->url, "/app/webpage")){
        res = "<html><h1>hello over there</h1></html>";
        http_header(c, 200); // 200 = succes request
        http_response(c, "text/html",res);
    }
    else if (!strcmp(req->method, "POST") && !strcmp(req->url, "/app/webpage")) {
        http_header(c, 200); // 200 = succes request
        http_response(c, "text/html",res);
    }
    else{
        res = "File not found";
        http_header(c, 404); // 404 = file not found
        http_response(c, "text/plain",res);
    }

    free(req);
    close(c);

    return;
}

int main(int argc, char *argv[])
{
    int s, c; 
    char *port;
    
    if(argc < 2){
        fprintf(stderr, "Usage: %s <listening port>\n", argv[0]);
        return -1;
    }else{
        port = argv[1];
    }

    s = srv_init(atoi(port));
    if(!s){
        fprintf(stderr, error, "%s\n", error);
        return -1;
    }
    printf("Listening on %s:%s\n", LISTENADDR, port);
    while(1){
        c = cli_accept(s);
        if(!c){
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
