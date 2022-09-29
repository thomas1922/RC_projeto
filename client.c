#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <arpa/inet.h>



#define BUF_SIZE	1024
#define MAX_SOCKETS 2
int sock;

//multicast mercados
typedef struct{
    int sock[MAX_SOCKETS];
    int len [MAX_SOCKETS];
    struct ip_mreq mreq[MAX_SOCKETS];
    struct sockaddr_in addr[MAX_SOCKETS];
} conexoes;
#define MERCADO_1 "239.0.0.1"
#define MERCADO_2 "239.0.0.2"
conexoes conex;


void erro(char *msg);
void receber_servidor();
void subscrever_multicast(int mercado);
sem_t escrita_acoes;
pthread_t menu_t;
int fd;
int m=-1;

void subscrever_multicast(int mercado){
  printf("%d\n",m);
  if (mercado==0||mercado==1){
    if (m!=-1) m=3;
    if (m==-1) m=mercado+1;
    if (bind(conex.sock[mercado], (struct sockaddr *) &conex.addr[mercado], sizeof(conex.addr[mercado])) < 0) {        
         perror("bind");
	      exit(1);
    }  
    if (mercado==0){
    conex.mreq[0].imr_multiaddr.s_addr = inet_addr(MERCADO_1);         
    conex.mreq[0].imr_interface.s_addr = htonl(INADDR_ANY); 
  }

  if (mercado==1){
    conex.mreq[1].imr_multiaddr.s_addr = inet_addr(MERCADO_2);         
    conex.mreq[1].imr_interface.s_addr = htonl(INADDR_ANY); 
  }

    if (setsockopt(conex.sock[mercado], IPPROTO_IP, IP_ADD_MEMBERSHIP,&conex.mreq[mercado], sizeof(conex.mreq[mercado])) < 0) {
        perror("setsockopt mreq");
	      exit(1);
    }
  }

  else{
    printf("MERCADO NAO EXISTENTE");
  }
}

void handler_control_c(int sig){
  char buffer_end[BUF_SIZE];
  sprintf(buffer_end,"EXIT");
  write(fd,buffer_end,strlen(buffer_end)+1);
  close(fd);
  exit(0);
}

void menu_function(){
  char comando[100];
  char resultado[BUF_SIZE];
  sem_wait(&escrita_acoes);
  while(1){
  printf("\n------------MENU--------------\n");
  printf("OPCOES DISPONIVEIS:\n");
  printf("-> DETALHES\n");
  printf("-> MOSTRAR\n");
  printf("-> SUBSCREVER <numero_mercado>\n");
  printf("-> COMPRAR <numero_mercado> <numero_bolsa> <numero_acoes>\n");
  printf("-> VENDER <numero_mercado> <numero_bolsa> <numero_acoes>\n");
  printf("-> LOGOUT\n");


  fscanf(stdin,"%s",comando);

  //ver detalhes
  if (strcmp(comando,"DETALHES")==0){
    write(fd,comando,strlen(comando)+1);
    //espera pelo resultado server
    while(1){
     int nread=read(fd,resultado,BUF_SIZE-1);
     if (nread > 0){
       //versao nao final
         printf("%s", resultado);
         break;
    }
     bzero(resultado,BUF_SIZE);
   }
  }

  //volta a mostrar as acoes multicast
  if (strcmp(comando,"MOSTRAR")==0){
    if (m!=-1){
      sem_post(&escrita_acoes);
      break;
    }
    
  }
  //utilizador faz logout
  if (strcmp(comando,"LOGOUT")==0){
    char buffer_end[BUF_SIZE];
    sprintf(buffer_end,"EXIT");
    write(fd,buffer_end,strlen(buffer_end)+1);
    close(fd);
    exit(0);
  }


    //subscrever endereco multicast
    if (strcmp(comando,"SUBSCREVER")==0){
      int n_mercado;
      fscanf(stdin,"%d",&n_mercado);

      if (n_mercado!=-1 && n_mercado>=0){
        subscrever_multicast(n_mercado);
      }

      else{
        printf("ERRO FORMATACAO OU VALOR MERCADO\n");
      }
    
    }

    //comprar/vender acoes
    if (strcmp(comando,"COMPRAR")==0||strcmp(comando,"VENDER")==0){
      char envio[BUF_SIZE];
      int n_mercado,n_bolsa,n_acoes;
      int d=fscanf(stdin,"%d %d %d",&n_mercado,&n_bolsa,&n_acoes);
      if (d==3){
          sprintf(envio,"%s %d %d %d",comando,n_mercado,n_bolsa,n_acoes);
          write(fd,envio,strlen(envio)+1);

              //espera pelo resultado server
          while(1){
            int nread=read(fd,resultado,BUF_SIZE-1);
            if (nread > 0){
       //versao nao final
              printf("%s", resultado);
              break;
          }
     bzero(resultado,BUF_SIZE);
   }
      }
      else{
        printf("ERRO FORMATACAO\n");
      }
    }
  
  bzero(comando,100);
  }
}
void * menu_th(){
  menu_function();
  pthread_exit(NULL);
}

void menu(int sig){
  pthread_create(&menu_t,NULL,menu_th,NULL);
}


int main(int argc, char *argv[]) {
  signal(SIGINT,handler_control_c);
  signal(SIGTSTP,menu);

  char endServer[100];
  struct sockaddr_in addr;
  struct hostent *hostPtr;
  
  if (argc != 3) {
    printf("cliente <host><port>\n");
    exit(-1);
  }
  
  sem_init(&escrita_acoes,0,1);

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Não consegui obter endereço");

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short) atoi(argv[2]));

  if ((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
	  erro("socket");
  if (connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
	  erro("Connect");
    int contador=0;
    char buffer_l[BUF_SIZE];
    while(1){
     int nread=read(fd,buffer_l,BUF_SIZE-1);
     if (nread > 0){
        if (strcmp(buffer_l,"EXIT")==0){
          handler_control_c(SIGINT);
        }
        else{
          printf("%s", buffer_l);
          if (contador==0 || contador==1){
            char message[BUF_SIZE];
            char buffer[BUF_SIZE];
            fgets(message, BUF_SIZE-1, stdin);
            sprintf(message, "%s",message);
            write(fd, message, strlen(message)+1);
          }
        }
        contador++;
        if (contador==4) break;
     }
    }
   conex.sock[0] = socket(AF_INET, SOCK_DGRAM, 0);
   if (conex.sock[0] < 0) {
     perror("socket");
     exit(1);
   }

   conex.sock[1] = socket(AF_INET, SOCK_DGRAM, 0);
   if (conex.sock[1] < 0) {
     perror("socket");
     exit(1);
   }

   bzero((char *)&conex.addr[0], sizeof(conex.addr[0]));
   conex.addr[0].sin_family = AF_INET;
   conex.addr[0].sin_addr.s_addr = htonl(INADDR_ANY);
   conex.addr[0].sin_port = htons((short) atoi(argv[2]));
   conex.len[0] = sizeof(conex.addr[0]);

   bzero((char *)&conex.addr[1], sizeof(conex.addr[1]));
   conex.addr[1].sin_family = AF_INET;
   conex.addr[1].sin_addr.s_addr = htonl(INADDR_ANY);
   conex.addr[1].sin_port = htons((short) atoi(argv[2]));
   conex.len[1] = sizeof(conex.addr[1]);

   conex.addr[0].sin_addr.s_addr = inet_addr(MERCADO_1);
   conex.addr[1].sin_addr.s_addr=inet_addr(MERCADO_2);

  menu_function();

  printf("\n---------------------------------------\nCASO QUEIRA ACEDER AO MENU BASTA CLICAR EM CONTROL Z\n---------------------------------------\n");
  int cnt;
  char buffer[BUF_SIZE];
  while (1) {
    printf("%d\n",m);
    if (m==3|| m==1){
      cnt = recvfrom(conex.sock[0], buffer, sizeof(buffer), 0, (struct sockaddr *) &conex.addr[0], &conex.len[0]);
	    if (cnt < 0) {
	      perror("recvfrom");
	      exit(1);
	    } 
      else if (cnt == 0) {
          break;
      }

      if (strcmp(buffer,"EXIT")==0) break;
      sem_wait(&escrita_acoes);
	    printf("->%s\n",buffer);
      bzero(buffer,BUF_SIZE);
    }
    if (m==3||m==2){
      cnt = recvfrom(conex.sock[1], buffer, sizeof(buffer), 0, (struct sockaddr *) &conex.addr[1], &conex.len[1]);
	    if (cnt < 0) {
	      perror("recvfrom");
	      exit(1);
	    } 
      else if (cnt == 0) {
          break;
        }
      
      if (strcmp(buffer,"EXIT")==0) break;
      if (m==2) sem_wait(&escrita_acoes);
      printf("---%s\n",buffer);
      }
      sem_post(&escrita_acoes);
    }

  handler_control_c(SIGINT);

  return 0;
}

void erro(char *msg) {
  printf("Erro: %s\n", msg);
	exit(-1);
}