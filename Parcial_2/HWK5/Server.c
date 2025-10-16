#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>

static void caesar(char *s, int k, int decrypt){
    if(!s) return;
    k = ((k%26)+26)%26;
    if(decrypt) k = 26-k;
    for(char *p=s; *p; ++p){
        char c=*p;
        if(c>='a'&&c<='z') *p = 'a'+((c-'a'+k)%26);
        else if(c>='A'&&c<='Z') *p = 'A'+((c-'A'+k)%26);
    }
}

int main(int argc, char **argv){
    if(argc<3){ fprintf(stderr,"Usage: %s <port> <shift>\n", argv[0]); return 1; }
    int port = atoi(argv[1]);
    int shift = atoi(argv[2]);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if(s<0){ perror("socket"); return 1; }

    int opt=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);


    if(bind(s,(struct sockaddr*)&addr,sizeof(addr))<0){ perror("bind"); return 1; }
    if(listen(s,8)<0){ perror("listen"); return 1; }
    printf("Server listening on port %d (shift=%d)\n", port, shift);

    for(;;){
        int c = accept(s,NULL,NULL);
        if(c<0){ perror("accept"); continue; }
        char buf[2048]; ssize_t n;
        while((n=recv(c,buf,sizeof(buf)-1,0))>0){
            buf[n]='\0';
            caesar(buf, shift, 0);            // ENCRYPT on server
            send(c, buf, strlen(buf), 0);
        }
        close(c);
    }
}


