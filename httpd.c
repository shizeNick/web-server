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
    char *body_start;
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

char* read_file(char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        error = "error in read_file()";
        return 0;
    }

    // calculate file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // allocate storage for content
    char *file_content = malloc(file_size + 1);
    if (file_content == NULL) {
        error = "error in read_file() (malloc failed)";
        fclose(fp); // Denke daran, die Datei zu schließen
        return NULL;
    }
    
    fread(file_content, 1, file_size, fp);
    fclose(fp);

    file_content[file_size] = '\0';

    return file_content;
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
       
        size_t len = strlen(p);
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

void handle_post_data(int content_length, char *body){
   
    char **split_pairs;
    split_pairs = split_string(body, "&");

    char* pair;
    char* key; 
    char* value;

    for(int i = 0; split_pairs[i]; i++){
        
        pair = split_pairs[i];
        key = pair;
        value = strstr(pair, "=");

        if (value != NULL) {
            *value = '\0'; // Split key (this is important for declare the end of string for key on '=')
            value++;      // move pointer to the beginning
        }

        // Jetzt hast du den Key und den Value in zwei getrennten Strings!
        printf("Key: %s, Value: %s\n", key, value);
    } 
    if (split_pairs) {
        for (int i = 0; split_pairs[i]; i++) {
            free(split_pairs[i]);
        }
        free(split_pairs);
    }
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
httpreq *parse_http(char *str)
{
    char *p, *body_start;
   
    httpreq *request = malloc(sizeof(httpreq));
    if(request == NULL){
        return NULL;
    }

    char *cut = strstr(str, "\r\n\r\n");
    if(cut == NULL) {
        free(request);
        return 0;
    }
    
    // copy header
    size_t header_len = cut - str;
    char *header_copy = malloc(header_len + 1);
    strncpy(header_copy, str, header_len);
    header_copy[header_len] = '\0';
    
    char *original_header = header_copy;

    // body start pointer
    request->body_start = cut + strlen("\r\n\r\n");

    // subdivide the req in his headers
    char **header = split_string(header_copy, "\r\n");

    // METHOD-Parser
    for(p = header_copy; *p && *p != ' '; p++);
    if(*p == ' ')
       *p = 0;  
    else{
        error = "parse_http() NOSPACE error";
        free(request);
        free(original_header);
        return 0;
    }
    strncpy(request->method, header_copy, sizeof(request->method)-1);
    request->method[sizeof(request->method)-1] = '\0';
    // URL-Parser
    for(header_copy=++p; *p && *p != ' '; p++);
    if(*p == ' ')
       *p = 0;  
    else{
        error = "parse_http() NO2ndSPACE error";
        free(request);
        free(original_header);
        return 0;
    }
    strncpy(request->url, header_copy, sizeof(request->url)-1);
    request->url[sizeof(request->url)-1] = '\0';
    
    // parse content-length
    // search the Content-Length Header
    char *content_length_str = get_header_value(header, "Content-Length");

    if (content_length_str != NULL) {
        request->content_length = atoi(content_length_str);
    } else {
        request->content_length = 0;
    }

    if (header) {
        for (int i = 0; header[i] != NULL; i++) {
            free(header[i]);
        }
        free(header);
    }
    free(original_header);

    return request;
    
}

/* return 0 on error or return the data */
char *cli_read(int c)
{
    char *total_buf = NULL;
    size_t total_len = 0;
    size_t read_len = 0;
    size_t allocated = 0;
    char temp_buf[1024];
    httpreq *req;
    
    while(1){
        read_len = read(c, temp_buf, sizeof(temp_buf));
        if(read_len <= 0){
            break;
        }
        if(read_len+total_len > allocated){
            allocated += 2048;
            total_buf = realloc(total_buf, allocated);
        }if(total_buf == NULL){
            return NULL;
        }

        memcpy(total_buf + total_len, temp_buf, read_len);
        total_len += read_len;
        
   // Check for end of headers
        if (strstr(total_buf, "\r\n\r\n") != NULL) {
            break; // Headers fully read
        }
    }

    // Now, handle the body based on Content-Length
    httpreq *req_temp = parse_http(total_buf);
    if (req_temp == NULL) {
        free(total_buf);
        return NULL;
    }
    
    size_t header_len = strstr(total_buf, "\r\n\r\n") - total_buf + strlen("\r\n\r\n");
    size_t body_already_read = total_len - header_len;
    size_t body_to_read = req_temp->content_length - body_already_read;

    // Read the rest of the body, if any
    while (body_to_read > 0) {
        ssize_t read_len = read(c, temp_buf, (body_to_read < sizeof(temp_buf)) ? body_to_read : sizeof(temp_buf));
        if (read_len <= 0) {
            // Handle error or unexpected disconnect
            free(total_buf);
            free(req_temp);
            error = "read() error while reading body";
            return NULL;
        }

        // Check if buffer needs to be reallocated for the rest of the body
        if (total_len + read_len + 1 > allocated) {
            allocated += 2048;
            total_buf = realloc(total_buf, allocated);
            if (total_buf == NULL) {
                free(req_temp);
                error = "realloc failed for body";
                return NULL;
            }
        }
        memcpy(total_buf + total_len, temp_buf, read_len);
        total_len += read_len;
        body_to_read -= read_len;
    }
    
    total_buf[total_len] = '\0'; // Final null-termination
    free(req_temp); // Only needed for content_length, now it's done
    return total_buf;
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
    char header_buf[1024]; 
    size_t content_length = (data) ? strlen(data) : 0;

    snprintf(header_buf, sizeof(header_buf),
             "HTTP/1.0 200 OK\r\n"
             "Server: httpd.c\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "\r\n", // Wichtig: Die Leerzeile trennt Header und Body
             content_type, content_length);

    // send header
    write(c, header_buf, strlen(header_buf));

    // send body
    if (data && content_length > 0) {
        write(c, data, content_length);
    }
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
        fprintf(stderr, "error read client: %s\n", error);
        close(c);
        return;
    }
    req = parse_http(p);
    if(!req){
        fprintf(stderr, "error parsing request: %s\n", error);
        close(c);
        return;
    }

    if(!strcmp(req->method, "GET") && !strcmp(req->url, "/app/login")){
        // open .html
        char *content = read_file("login.html");

        http_header(c, 200); // 200 = succes request
        http_response(c, "text/html", content);
    }else if (!strcmp(req->method, "GET") && !strcmp(req->url, "/style/style.css")) {
        // open .css
        char *content = read_file("style/style.css");

        http_header(c, 200); // 200 = succes request
        http_response(c, "text/css", content);
    }
    else if (!strcmp(req->method, "POST") && !strcmp(req->url, "/app/login")) {
       
        http_header(c, 200); // 200 = succes request
        
        // open .html
        char *content = read_file("login.html");

        // handle body data
        handle_post_data(req->content_length, req->body_start);
        
        //response updated site
        http_response(c, "text/html", content);
       
        free(content);
    }
    else{
        char *res = "<html><h1>File not found</h1></html>";
        http_header(c, 404); // 404 = file not found
        http_response(c, "text/html",res);
    }

    free(req);
    free(p);
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
