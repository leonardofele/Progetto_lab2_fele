#include "xerrori.h"
#define QUI __LINE__, __FILE__
#define HOST "127.0.0.1"
#define PORT 58477


ssize_t readn(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; 
            else
                break; 
        }
        else if (nread == 0)
            break; 
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); 
}

/* Write "n" bytes to a descriptor
   analoga alla funzione python sendall() */
ssize_t writen(int fd, void *ptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
 
    nleft = n;
    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) < 0)
        {
            if (nleft == n)
                return -1; 
            else
                break; 
        }
        else if (nwritten == 0)
            break;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n - nleft); 
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        printf("Uso\n\t%s nome_file\n", argv[0]);
        exit(1);
    }

    int fd_skt = 0;
    struct sockaddr_in serv_addr;
    size_t e;
    int tmp;
    
    FILE *file;
    file = fopen(argv[1],"r");
    if (file == NULL) {
        termina("Errore appertura file");
    }

    char *line;
    size_t len = 0;
    ssize_t read;
    

     
    while ((read = getline(&line, &len, file)) != -1) {
        if ((fd_skt = socket(AF_INET, SOCK_STREAM, 0)) < 0) //qua creo il soket come la chiamata con python
        termina("Errore creazione socket");
        serv_addr.sin_family = AF_INET;
        //gli collego host e pore
        serv_addr.sin_port = htons(PORT); 
        serv_addr.sin_addr.s_addr = inet_addr(HOST);

        if (connect(fd_skt, &serv_addr, sizeof(serv_addr)) < 0) // mi collego al server
            termina ("errore di termizione");
        tmp = htonl(0); // lo trasformo in formato network in 4 byte, per dirgli che è una connessione di tipo A
        e = writen(fd_skt,&tmp,sizeof(tmp)); // lo vado a scrivere con la funzione writeN nel file descriptor dove sono i dati quanto sono i dati
        if(e!=sizeof(int)){ 
            if(close(fd_skt)<0) 
                termina("Errore write");
            termina("Errore write");
        }

        
        tmp = htonl(read-1); // gli mando la lunghezza ma tolgo /n
        e = writen(fd_skt,&tmp,sizeof(tmp));
        if(e!=sizeof(int)) {
            termina("Errore write 1");
        }
        ssize_t sent = writen(fd_skt, line, strlen(line));
        if (sent == -1) {
            perror("Errore nell'invio dei dati");
            close(fd_skt);
            exit(EXIT_FAILURE);
        }
        
      if(close(fd_skt)<0) // chiudo il file descriptor cioè il soket e sono apposto
        xtermina("chiusura soket sbagliata",QUI);
        
    }
    fclose(file);
    return 0;
}
