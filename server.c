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
#include <sys/uio.h>
#include <arpa/inet.h>
#include <semaphore.h>




#define BUF_SIZE	500
#define MAX_SOCKETS 2
#define MERCADO_1 "239.0.0.1"
#define MERCADO_2 "239.0.0.2"

int sock;
int SERVER_PORT;
int SERVER_CONFIG;
int fd;

//estrutura para tratar do multicast
typedef struct{
    int len [MAX_SOCKETS];
    struct sockaddr_in addr[MAX_SOCKETS];
} conexoes;

//estruturas admin, acao, mercado,cliente
typedef struct{
    char nome_admin[10];
    char password_admin[10];
}admin;

typedef struct{
    char nome_acao[30];
    float preco_vendedor;
    int acoes_venda;
    float preco_comprador;
    int acoes_compra; 
}acao;

typedef struct{
    char nome_mercado[20];
    acao acoes[3];
}mercado;

typedef struct{
  int fd;
  char name[20];
  char password[20];
  float saldo;
  bool mercado_subscrito[2];
  bool ocupado;
  int num_acoes[6];
}cliente;


mercado mercados[2];
cliente clients[10];
admin admin1;
pthread_t client_thread[10];

int REFRESH_TIME=5;

void * gerador_acoes();
void * server_udp();
void print_acoes_mercado(mercado mercado_aux);
void random_acao(acao * acao_r);
void leitura_ficheiro(char * file_name);
int verificar_login(char * username, char * password,int client);
void add_user(char * username,char*password,bool mercado1,bool mercado2,float saldo);
void del (char * username);
void list();
void refresh(int seg);
void termina_server();
void * thread_client(void * int_client);


pthread_t thread_gerador_acoes;
pthread_t thread_server_udp;
pid_t parent_process;
sem_t acesso_mercados;
conexoes conex;
void termina(int sig){
    char envio[10];
    strcpy(envio,"EXIT");
    int cnt;
    cnt=sendto(sock,envio,sizeof(envio),0,(struct sockaddr *) &conex.addr[0], conex.len[0]);
    if (cnt < 0) {
 	perror("sendto");
	exit(1);}
    
    cnt = sendto(sock, envio, sizeof(envio) ,0,(struct sockaddr *) &conex.addr[1], conex.len[1]);
    if (cnt < 0) {
 	perror("sendto");
	exit(1);}


    close(fd);
    exit(0);
}
int main(int argc, char * argv[]) {
    if (argc!=4){
        perror("INVALID FORMAT <PORTO_BOLSA> <PORTO_CONFIG> <CONFIG_FILE>\n");
        exit(0);
    }
    
    SERVER_PORT=atoi(argv[1]);
    SERVER_CONFIG=atoi(argv[2]);

    //gera semente para o rand
    srand(time(NULL));  

    int client;
    struct sockaddr_in addr, client_addr;
    int client_addr_size;

    //conexao TCP
    bzero((void *) &addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("na funcao socket");
    if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
        perror("na funcao bind");
    if( listen(fd, 5) < 0)
        perror("na funcao listen");
  client_addr_size = sizeof(client_addr);

  sem_init(&acesso_mercados,0,1);

  
  //nenhum cliente esta conectado
  for (int i=0;i<10;i++){
      clients[i].fd=-1;
      for (int j=0;j<6;j++){
          clients[i].num_acoes[j]=0;
      }
  }

  //le ficheiro config
  leitura_ficheiro(argv[3]);

  //testes mercados subscritos
  clients[0].mercado_subscrito[0]=true;
  clients[0].mercado_subscrito[1]=false;

  parent_process=getpid();

   //conxoes multicast para 2 mercados
   sock = socket(AF_INET, SOCK_DGRAM, 0);
   if (sock < 0) {
     perror("socket");
     exit(1);
   }

   bzero((char *)&conex.addr[0], sizeof(conex.addr[0]));
   conex.addr[0].sin_family = AF_INET;
   conex.addr[0].sin_addr.s_addr = htonl(INADDR_ANY);
   conex.addr[0].sin_port = htons(SERVER_PORT);
   conex.len[0] = sizeof(conex.addr[0]);

   bzero((char *)&conex.addr[1], sizeof(conex.addr[1]));
   conex.addr[1].sin_family = AF_INET;
   conex.addr[1].sin_addr.s_addr = htonl(INADDR_ANY);
   conex.addr[1].sin_port = htons(SERVER_PORT);
   conex.len[1] = sizeof(conex.addr[1]);

   conex.addr[0].sin_addr.s_addr = inet_addr(MERCADO_1);
   conex.addr[1].sin_addr.s_addr=inet_addr(MERCADO_2);

   signal(SIGINT,termina);
    //thread para gerar acoes de 2 mercados com um determinado REFRESH_TIME
    pthread_create(&thread_gerador_acoes,NULL,&gerador_acoes,NULL);

    //thread para aceder via UDP à consola de administracao
    pthread_create(&thread_server_udp,NULL,&server_udp,NULL);
    
    printf("SERVIDOR INICIALIZADO\n");
  
    //espera que clientes se conectem
    while(1){
        char envio[1000];
        char username[20];
        char password[20];
        //novo cliente
        client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);

        int estado_login=-3;

        sprintf(envio,"INTRODUZA O USERNAME\n");
        write(client,envio,strlen(envio)+1);
        if (read(client,username,20)>0);
        sprintf(envio,"INTRODUZA A PASSWORD\n");
        write(client,envio,strlen(envio)+1);
        if (read(client,password,20)>0);
        
        //return -1 caso o login seja invalido, retorna o numero do user caso seja valido o login
        estado_login=verificar_login(username,password,client);
        
        printf("ESTADO_LOGIN %d\n",estado_login);
        
        if (estado_login==-1){
            sprintf(envio,"LOGIN INVALIDO\n");
            write(client,envio,strlen(envio)+1);
            sleep(1);
            sprintf(envio,"EXIT");
            write(client,envio,strlen(envio)+1);
            close(client);
        }

        else {
            printf("NOVO CLIENTE LOGADO:%d\n",estado_login);
            sprintf(envio,"LOGIN VALIDO\n");
            write(client,envio,strlen(envio)+1);
            bzero(envio,1000);
            pthread_t client_thread;
            pthread_create(&client_thread,NULL,&thread_client,(void*)&estado_login);   
        }   
    }
    close(fd);
    return 0;
}

void envia_detalhes_cliente(int num_client){
    char * message=malloc(sizeof(char)*BUF_SIZE);
    sem_wait(&acesso_mercados);
    strcpy(message,"MERCADOS SUBSCRITOS:\n");
    for (int i=0;i<2;i++){
        if (clients[num_client].mercado_subscrito[i]==true){
            strcat(message,strcat(mercados[i].nome_mercado,"\n"));
        }
    }
        char buffer_aux[50];
        sprintf(buffer_aux,"SALDO UTILIZADOR:%f\n",clients[num_client].saldo);
        strcat(message,buffer_aux);
        write(clients[num_client].fd,message,strlen(message)+1);
    bzero(message,BUF_SIZE-20);
    bzero (buffer_aux,50);
    sem_post(&acesso_mercados);
}

bool comprar_vender(int num_client,char opt,int n_mercado,int n_bolsa,int n_acoes){
    //cliente não tem acesso à bolsa
    acao acao_em_causa;
    if (clients[num_client].mercado_subscrito[n_mercado]==false) return false;

    acao_em_causa=mercados[n_mercado].acoes[n_bolsa];

    if (opt=='C'){
        if (acao_em_causa.acoes_venda>=n_acoes){
            if (clients[num_client].saldo-acao_em_causa.preco_comprador*n_acoes>=0){
                if (n_mercado==0){
                    clients[num_client].num_acoes[n_bolsa]+=n_acoes;
                }

                if (n_mercado==1){
                    clients[num_client].num_acoes[n_bolsa+3]+=n_acoes;
                }

                clients[num_client].saldo=clients[num_client].saldo-acao_em_causa.preco_comprador*n_acoes;
            }
            else return false;
        }

        else return false;
    }

    if (opt=='V'){
        if (acao_em_causa.acoes_venda>=n_acoes){
            if (n_mercado==0){
                if (clients[num_client].num_acoes[n_bolsa]>=n_acoes) clients[num_client].num_acoes[n_bolsa]-=n_acoes;
                else return false;
            }

            if (n_mercado==1){
                if (clients[num_client].num_acoes[n_bolsa+3]>=n_acoes) clients[num_client].num_acoes[n_bolsa+3]-=n_acoes;
                else return false;
            }

            clients[num_client].saldo+=n_acoes*acao_em_causa.preco_vendedor;
        }

        else return false;
    }
}


void * thread_client(void * int_client){
    int num_client=*(int*) int_client;

    int nread;
    char buffer[BUF_SIZE];
    char envio[255];

    envia_detalhes_cliente(num_client);

    //lê da ligacao client
    while(1){
     int nread=read(clients[num_client].fd,buffer,BUF_SIZE-1);
     if (nread > 0){
        printf("%d:%s\n",num_client,buffer);
        if (strcmp(buffer,"EXIT")==0){
          break;
        }
        else{
            //envia detalhes do utilizador
            if (strcmp(buffer,"DETALHES")==0){
                envia_detalhes_cliente(num_client);
            }

            else{
                char * token=NULL;
                token=strtok(buffer," ");
                if(strcmp(token,"COMPRAR")==0 || strcmp(token,"VENDER")==0){
                    char opt[200];
                    strcpy(opt,token);
                    int n_mercado,n_bolsa,n_acoes;
                    int count_token=0;
                    while(token!=NULL){
                    if (count_token==1) n_mercado=atoi(token);
                    if (count_token==2) n_bolsa=atoi(token);
                    if (count_token==3) n_acoes=atoi(token); 
                    count_token+=1;
                    token=strtok(NULL," ");
                    }
                    if (comprar_vender(num_client,opt[0],n_mercado,n_bolsa,n_acoes))
                        sprintf(envio,"%s BEM SUCEDIDA",opt);
                    else sprintf(envio,"%s NAO SUCEDIDA",opt);

                    write(clients[num_client].fd,envio,strlen(envio)+1);
                }
            }


        }
     }
     bzero(buffer,BUF_SIZE);
   }

   clients[num_client].fd=-1;
   printf("CLIENT %d DESCONECTOU-SE\n",num_client);
   pthread_exit(NULL);
}

void * gerador_acoes(){
    char time_[10];
    while (1){
        sem_wait(&acesso_mercados);
        for (int i=0;i<2;i++){
            for (int j=0;j<3;j++){
                //randomiza 6 acoes (3 de cada mercado)
                random_acao(&mercados[i].acoes[j]);
            }
        }
        for (int i=0;i<2;i++){
            print_acoes_mercado(mercados[i]);
        }
        sem_post(&acesso_mercados);
        sleep(REFRESH_TIME);
    }
}

void random_acao(acao * acao_r){
    int opt=rand()%2;

    acao_r->acoes_compra=(rand()%10+1)*10;
    acao_r->acoes_venda=(rand()%10+1)*10;

    if (opt==0){
        acao_r->preco_vendedor-=0.01;
        if (acao_r->preco_vendedor<0) acao_r->preco_vendedor=0.00;
        acao_r->preco_comprador=acao_r->preco_vendedor+0.02;
    }

    else if (opt==1){
        acao_r->preco_vendedor+=0.01;
        acao_r->preco_comprador=acao_r->preco_vendedor-0.02;
        if (acao_r->preco_comprador<0){
            acao_r->preco_comprador=0;
            acao_r->preco_vendedor=0.02;
        }
    }
}

void print_acoes_mercado(mercado mercado_aux){
    char header[300];
    char envio[2000];
    char nova_string[1500];
    char acao[1024];

    time_t t=time(NULL);
    struct tm tm;
    tm = *localtime(&t);

    sprintf(header,"-----------------------------\n       %02d:%02d:%02d  %s\n-----------------------------\n", tm.tm_hour, tm.tm_min, tm.tm_sec,mercado_aux.nome_mercado);
    //printf("%s\n",header);

    for (int i=0;i<3;i++){
        sprintf(acao,"%s\n%f\n%d\n%f\n%d\n",mercado_aux.acoes[i].nome_acao,mercado_aux.acoes[i].preco_vendedor,mercado_aux.acoes[i].acoes_venda,mercado_aux.acoes[i].preco_comprador,mercado_aux.acoes[i].acoes_compra);
        strcat(nova_string,acao);
        bzero(acao,1024);
    }
    // sprintf(envio,"%s\n%s\n",header,nova_string);
    sprintf(envio,"%s%s\n",header,nova_string);
    //printf("%s\n\n",envio);
    
    int cnt;
    if (strcmp(mercado_aux.nome_mercado,mercados[0].nome_mercado)==0){
        cnt=sendto(sock,envio,sizeof(envio),0,(struct sockaddr *) &conex.addr[0], conex.len[0]);
        if (cnt < 0) {
 	    perror("sendto");
	    exit(1);}
    }
    if (strcmp(mercado_aux.nome_mercado,mercados[1].nome_mercado)==0){
        cnt = sendto(sock, envio, sizeof(envio) ,0,(struct sockaddr *) &conex.addr[1], conex.len[1]);
        if (cnt < 0) {
 	    perror("sendto");
	    exit(1);}
    }
    bzero (header,300);
    bzero(envio,2000);
    bzero(nova_string,1500);
    bzero(acao,1024);
};
    
int verificar_login(char username[20],char password[20],int client){
    for (int i=0;i<strlen(username);i++){
        if (username[i]=='\n') username[i]='\0';
    }
    for (int i=0;i<strlen(password);i++){
        if (password[i]=='\n') password[i]='\0';
    }

    for (int i=0;i<10;i++){
        //login confirmado
        if (strcmp(username,clients[i].name)==0 && strcmp(password,clients[i].password)==0){
            printf("LOGIN CONHECIDO");
            //estabelece ligacao
            clients[i].fd=client;
            return i;
        }
   }
   return -1;
}

//leitura ficheiro
void leitura_ficheiro(char * file_name){
    char buffer[1024];
    int num_clients;
    FILE*fp;
    // fp=fopen("config.txt","r");
    fp=fopen(file_name,"r");
    
    int num_stock=0;
    int num_mercado=0;


    
    char * ptr=NULL;
    long num;

    int count_lines=1;
    while(fgets(buffer,1023,fp)){

        if (count_lines==1){
            int count_token=0;
            char * token=strtok(buffer,"/");
            while (token!=NULL){
                if (count_token==0) strcpy(admin1.nome_admin,token);
                if (count_token==1) {
                    token[strlen(token)-2]='\0';
                    strcpy(admin1.password_admin,token);
                }
                token=strtok(NULL,"/");
                count_token++;
            }
        }

        if (count_lines==2){
            num=strtol(buffer,&ptr,10);
            num_clients=(int) num;
        }
        
        if (count_lines>2 & count_lines<=2+num_clients){
            int num_client=count_lines-num_clients-1;
            int count_token=0;
            char * token=strtok(buffer,";");
            while (token!=NULL){
                if (count_token==0) strcpy(clients[num_client].name,token);
                if (count_token==1) strcpy(clients[num_client].password,token);
                if (count_token==2){
                     clients[num_client].saldo=(int) atof(token);
                     clients[num_client].ocupado=true;
                }
                token=strtok(NULL,";");
                count_token++;
            }
        }

        if (count_lines>2+num_clients){
            if (num_stock==3 && num_mercado==0) {
                num_stock=0;
                //printf("HERE\n");
                num_mercado=1;
            }
            if (num_stock!=3){
                int count_token=0;
                char * token=strtok(buffer,";");
                while (token!=NULL){
                    if (count_token==0) {
                        strcpy(mercados[num_mercado].nome_mercado,token);
                    }
                    if (count_token==1) {
                        strcpy(mercados[num_mercado].acoes[num_stock].nome_acao,token);
                    }
                    if (count_token==2){
                        mercados[num_mercado].acoes[num_stock].preco_comprador = (int)atof(token);
                        mercados[num_mercado].acoes[num_stock].preco_vendedor = (int) atof(token);
                    }
                    count_token++;
                    token=strtok(NULL,";");
            }
            num_stock+=1;
            }
        }
        count_lines++;
    }
}
void add_user(char * username,char*password,bool mercado1,bool mercado2,float saldo){
    bool existe=false;
    for (int i=0;i<10;i++){
        //se existir so altera os dados
        if (clients[i].ocupado==true && strcmp(username,clients[i].name)==0){
            clients[i].saldo=saldo;
            clients[i].mercado_subscrito[0]=mercado1;
            clients[i].mercado_subscrito[1]=mercado2;
            existe=true;
            break;
        }
    }

    if (existe==false){
        for (int i=0;i<10;i++){
            if (clients[i].ocupado==false){
                strcpy(clients[i].name,username);
                strcpy(clients[i].password,password);
                clients[i].mercado_subscrito[0]=mercado1;
                clients[i].mercado_subscrito[1]=mercado2;
                clients[i].saldo=saldo;
                clients[i].ocupado=true;
                break;
            }
        }
    }
}

void del (char * username){
    for (int i=0;i<10;i++){
        if (strcmp(clients[i].name,username)==0){
            clients[i].fd=-1;
            strcpy(clients[i].name,"");
            strcpy(clients[i].password,"");
            clients[i].ocupado=false;
            clients[i].mercado_subscrito[0]=false;
            clients[i].mercado_subscrito[0]=false;
            clients[i].saldo=0;
            //debug
            printf("APAGUEI\n");
            break;
        }
    }
}

void list(){
    for (int i=0;i<10;i++){
        if (clients[i].ocupado==true){
            printf("%s\n%s\n%f\n\n",clients[i].name,clients[i].password,clients[i].saldo);
        }
    }
}

void * server_udp(){
    //printf("THREAD SERVER UDP\n");
    struct sockaddr_in server,client;
    int s,recv_len,recv_msg_len;
	socklen_t slen = sizeof(client);
	char buf[1024];
    char mess[1024];
    bool admin_confirmado=false;

    if((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		perror("Erro na criação do socket");
        exit(0);
	}

    server.sin_family = AF_INET;
	server.sin_port = htons(SERVER_CONFIG);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	// Associa o socket à informação de endereço
	if(bind(s,(struct sockaddr*)&server, sizeof(server)) == -1) {
		perror("Erro no bind");
        exit(0);
	}

    bzero(buf,1024);

    int control_inputs=1;
    typedef struct{
        char username[20];
        char password[20];
    }inputs;

    inputs input_s;
    int contador=0;
    
    while(contador!=5){
        recv_len = recvfrom(s, buf, 1024, 0, (struct sockaddr *) &client, (socklen_t *)&slen);
        contador++;
    }

    while(1){    
    sprintf(buf,"LOGIN\n");
    sendto(s,(const char *)buf,strlen(buf),0,(struct sockaddr*)&client,slen);

    while (admin_confirmado==false){
        if (control_inputs!=3){
            if((recv_len = recvfrom(s, buf, 1024, 0, (struct sockaddr *) &client, (socklen_t *)&slen)) == -1) {
                perror("Erro no recvfrom");
                exit(0);}
                
            if (strcmp(buf,"X")!=0){
                if (control_inputs==1){
                    buf[strlen(buf)-1]='\0';
                    strcpy(input_s.username,buf);
                    printf("---username:%s\n",input_s.username);
                    control_inputs++;
                    continue;
                }
                if (control_inputs==2){
                    buf[strlen(buf)-1]='\0';
                    strcpy(input_s.password,buf);
                    printf("---password:%s\n",input_s.password);
                    control_inputs++;
                }
            }
        }

        bzero(buf,1024);

        if (control_inputs==3){
            //verificar user e password
            //login valido

            printf("DADOS LOGIN:\n%s\n%s\n",input_s.username,input_s.password);
            printf("DADOS ADMIN:\n%s\n%s\n",admin1.nome_admin,admin1.password_admin);

            bzero(buf,1024);

            if (strcmp(input_s.username,admin1.nome_admin)==0 && strcmp(input_s.password,admin1.password_admin)==0){
                sprintf(buf,"LOGIN VALIDO\n");
                sendto(s,(const char *)buf,strlen(buf),0,(struct sockaddr*)&client,slen);
                bzero(buf,1024);
                printf("LOGIN FEITO:\n%s\n%s\n",input_s.username,input_s.password);
                admin_confirmado=true;
                break;
            }
            else{
                sprintf(buf,"LOGIN INVALIDO\n");
                sendto(s,(const char *)buf,strlen(buf),0,(struct sockaddr*)&client,slen);
                bzero(buf,1024);
                control_inputs=1;
            }
        } 
    }
    control_inputs=1;
    //admin validado
    while (1){
        char * token;
        if((recv_len = recvfrom(s, buf, 1024, 0, (struct sockaddr *) &client, (socklen_t *)&slen)) == -1) {
                perror("Erro no recvfrom");
                exit(0);
            }
                
        if (strcmp(buf,"X")!=0){
            token=NULL;
            token=strtok(buf," ");
            printf("token:%s\n",token);

            if (strcmp(token,"ADD_USER")==0){
                char username[20],password[20];
                bool bolsa1,bolsa2;
                float saldo;

                printf("ADD USER FUNCTION\n");
                //5 tokens
                int count_token=0;
                while(token!=NULL){
                    if (count_token==1) strcpy(username,token);
                    else if (count_token==2) {
                        strcpy(password,token);
                    }
                    else if (count_token==3){
                        if (strcmp(token,"1")==0) bolsa1=true;
                        else if (strcmp(token,"0")==0) bolsa1=false;
                    }
                    else if (count_token==4){
                        if (strcmp(token,"1")==0) bolsa2=true;
                        else if (strcmp(token,"0")==0) bolsa2=false;
                    }
                    else if (count_token==5){
                        saldo=atof(token);
                    }
                    count_token++;
                    token=strtok(NULL," ");
                }
                if (count_token==6) {
                    add_user(username,password,bolsa1,bolsa2,saldo);
                }
                else{
                    char message[100];
                    sprintf(message,"ADD_USER FORMAT INCORRECT <username><password><bool bolsa1><bool bolsa2><saldo>\n");
                    sendto(s,(const char *)message,strlen(message),0,(struct sockaddr*)&client,slen);
                    bzero(message,100);
                }
            }
            else if (strcmp(token,"DEL")==0){
                char argument_del[20];
                printf("DEL FUNCTION\n");
                while (token!=NULL){
                    strcpy(argument_del,token);
                    token=strtok(NULL," ");
                }
                argument_del[strlen(argument_del)-1]='\0';
                del(argument_del);
                bzero(argument_del,20);
            }

            else if (strcmp(token,"LIST\n")==0){
                char output_line[200]="";
                printf("LIST FUNCTION\n");
                for (int i=0;i<10;i++){
                    if (clients[i].ocupado==true){
                        sprintf(output_line,"%s\n%s\n%f\n\n",clients[i].name,clients[i].password,clients[i].saldo);
                        sendto(s,(const char *)output_line,strlen(output_line),0,(struct sockaddr*)&client,slen);
                        bzero(output_line,200);
                    }
                } 
            }

            else if (strcmp(token,"REFRESH")==0){
                int ref;
                while (token!=NULL){
                    REFRESH_TIME=atoi(token);
                    ref=REFRESH_TIME;  
                    token=strtok(NULL," ");
                }
                sprintf(buf,"REFRESH_TIME ATUALIZADO PARA %d s\n",ref);
                sendto(s,(const char *)buf,strlen(buf),0,(struct sockaddr*)&client,slen);
            }

            else if (strcmp(token,"QUIT\n")==0){
                printf("QUIT FUNCTION\n");
                admin_confirmado=false;
                sprintf(buf,"LOGOUT\n\n");
                sendto(s,(const char *)buf,strlen(buf),0,(struct sockaddr*)&client,slen);
                break;
            }
            
            else if (strcmp(token,"QUIT_SERVER\n")==0){
                printf("QUIT_SERVER FUNCTION\n");
                close(s);
                termina_server();
                continue;
            }
        }

        bzero(buf,1024);
    }
    bzero(input_s.password,20);
    bzero(input_s.username,20);
    }

    close(s);
}

void termina_server(){
    pthread_detach(thread_gerador_acoes);
    pthread_detach(thread_server_udp);
    kill(parent_process,9);
}