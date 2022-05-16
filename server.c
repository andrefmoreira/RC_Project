//aceder como cliente usando nc -u 127.0.0.1 config.txt
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFLEN 512	// Tamanho do buffer

typedef struct{
	char nome[50];
	int valor;
} acao;

typedef struct{
	char nome[50];
	acao acoes[3];
} mercado;

typedef struct{
	char username[50];
	char password[50];
	int saldo;
	char bolsa[15];
	bool adminis;
} utilizador;

void erro(char *s) {
	perror(s);
	exit(1);
}

void consolaconfig(int s_admin, struct sockaddr_in si_outra, socklen_t slen, int num);
void processa_client(int client_fd, int num);

utilizador utilizadores[11];
mercado mercados[2];
int tempo;

int main(int argc, char *argv[]) {
    struct sockaddr_in config, si_outra;
	utilizador admin;
	admin.adminis = true;
	admin.saldo = 0;

  //Portas
	int port_bolsa = strtol(argv[1],NULL,10);
	int port_config = strtol(argv[2],NULL,10);

  //Leitura ficheiro de configuração
	FILE * f = fopen(argv[3],"r");
	if(!f)
		exit(EXIT_FAILURE);
	
	fscanf(f,"%[^/]/%s",admin.username,admin.password);
	int aux = 0;
	fscanf(f,"%d",&aux);
	utilizadores[0] = admin;
	int i,a;
	char aux2[50];
	for(i=1;i<aux+1;i++){
		fscanf(f,"%[^;];%[^;];%s",utilizadores[i].username,utilizadores[i].password,aux2);
		int b;
		//tivemos de remover os dois primeiros indices de cada username pois nos ficheiro txt em windows existem 2 carateres especiais no inicio de cada linha
		for(b=0;b<strlen(utilizadores[i].username)-2;b++){
			utilizadores[i].username[b] = utilizadores[i].username[b+2];
		}
		utilizadores[i].username[strlen(utilizadores[i].username)-2] = '\0';
		utilizadores[i].saldo = strtol(aux2,NULL,10);
		utilizadores[i].adminis = false;
	}
	for(i=0;i<2;i++){
		for(a=0;a<3;a++){
			//mesmo problema de cima
			fscanf(f,"%[^;];%[^;];%s",mercados[i].nome,mercados[i].acoes[a].nome,aux2);
			mercados[i].acoes[a].valor= strtol(aux2,NULL,10);
		}
	}

	int id;
	if((id = fork())!=0){
	//Criar um socket para administração UDP
		
		int s_admin,recv_len;
		socklen_t slen = sizeof(si_outra);
		char buf[BUFLEN];

		// Cria um socket para recepção de pacotes UDP
		if((s_admin=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
			erro("Erro na criação do socket");
		}

		// Preenchimento da socket address structure
		config.sin_family = AF_INET;
		config.sin_port = htons(port_config);
		config.sin_addr.s_addr = htonl(INADDR_ANY);

		// Associa o socket à informação de endereço
		if(bind(s_admin,(struct sockaddr*)&config, sizeof(config)) == -1) {
			erro("Erro no bind");
		}
		char nome_introduzido[50];
		char password_introduzido[50];
		if((recv_len = recvfrom(s_admin, nome_introduzido, 50, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1)  
			erro("Erro no recvfrom");
		//eliminar \n
		nome_introduzido[recv_len-1] = '\0';

		char *string;
		//local onde lemos o username e password
		if(strcmp(nome_introduzido , admin.username) == 0){
			string = "Insira a password:\n";
			sendto(s_admin, string, strlen(string),0,(struct sockaddr *)&si_outra,slen);
			if((recv_len = recvfrom(s_admin, password_introduzido, 50, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1)
				erro("Erro no recvfrom");
			password_introduzido[recv_len-1] = '\0';

			if(strcmp(password_introduzido , admin.password) == 0)
			{
				string = "Autenticacao com sucesso!\n";
				sendto(s_admin, string, strlen(string),0,(struct sockaddr *)&si_outra,slen);
				consolaconfig(s_admin,si_outra,slen, aux);
				close(s_admin);
				exit(0);
			}
			else
				erro("palavra passe incorreta"); 
		}else{
			bool encontrado = false;
			for(i = 0; i < aux+1 ; i++){
				if(strcmp(nome_introduzido , utilizadores[i].username) == 0){
					encontrado = true;
					erro("O utilizador não é administrador\n");
				}
			}
			if(!encontrado){
				string = "Utilizador nao encontrado\n\0";
				sendto(s_admin, string, strlen(string), 0, (struct sockaddr *)&si_outra,slen);
			}
		}
	// Fecha socket e termina programa
	close(s_admin);
	}
	else{
	//Criar um socket TCP para os clientes
		int s_tcp, client;
		struct sockaddr_in addr, client_addr;
		int client_addr_size;

		bzero((void *) &addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(port_bolsa);

		if ( (s_tcp = socket(AF_INET, SOCK_STREAM, 0)) < 0) erro("na funcao socket");
		if ( bind(s_tcp,(struct sockaddr*)&addr,sizeof(addr)) < 0) erro("na funcao bind");
		if( listen(s_tcp, 5) < 0) erro("na funcao listen");
		client_addr_size = sizeof(client_addr);

		while (1) {
			//clean finished child processes, avoiding zombies
			//must use WNOHANG or would block whenever a child process was still working
			while(waitpid(-1,NULL,WNOHANG)>0);
			// wait for new connection
			client = accept(s_tcp,(struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
			if (client > 0) {
				if (fork() == 0) {
					close(s_tcp);
					processa_client(client,aux);
					exit(0);
				}
				close(client);
			}
		}
	}
	return 0;
}

void consolaconfig(int s_admin, struct sockaddr_in si_outra, socklen_t slen, int num){
	int aux = num+1;
	int recv_len;
	char *string;
	while(1){
		char opcao[100];
		string = "Insira opcao:\n\0";
		sendto(s_admin, string, strlen(string), 0, (struct sockaddr *)&si_outra,slen);
		if((recv_len = recvfrom(s_admin, opcao, 100, 0, (struct sockaddr *) &si_outra, (socklen_t *)&slen)) == -1)
			erro("Erro no recvfrom");
		opcao[recv_len-1]='\0';
		char *token = strtok(opcao, " ");
		if(strcmp(token,"ADD_USER")==0){
			utilizador novo;
			token = strtok(NULL, " ");
			strcpy(novo.username,token);
			token = strtok(NULL, " ");
			strcpy(novo.password,token);
			token = strtok(NULL, " ");
			strcpy(novo.bolsa,token);
			token = strtok(NULL, " ");
			novo.saldo = (int) strtol(token,NULL,10);
			novo.adminis = false;
			utilizadores[aux] = novo;
			aux++;
			string = "Utilizador criado com sucesso!\n\0";
			sendto(s_admin, string, strlen(string), 0, (struct sockaddr *)&si_outra,slen);
		}
		if(strcmp(token,"DEL")==0){
			token = strtok(NULL, " ");
			int size = strlen(token);
			int i, pos=0;
			for(i=1;i<aux;i++){
				if(strcmp(token, utilizadores[i].username)==0){
					pos = i;
				}
			}
			if(pos==0){
				string = "Utilizador nao encontrado!\n\0";
				sendto(s_admin, string, strlen(string), 0, (struct sockaddr *)&si_outra,slen);
			}
			else{
				for(i=pos;i<aux-1;i++){
					utilizadores[i] = utilizadores[i+1];
				}
				aux--;
				string = "Utilizador eliminado!\n\0";
				sendto(s_admin, string, strlen(string), 0, (struct sockaddr *)&si_outra,slen);
			}
		}
		if(strcmp(token,"LIST")==0){
			int i;
			char stringaux[50];
			for(i=0;i<aux;i++){
				strcpy(stringaux,utilizadores[i].username);
				strcat(stringaux,"\n");
				sendto(s_admin, stringaux, strlen(stringaux), 0, (struct sockaddr *)&si_outra,slen);
			}
		}
		if(strcmp(token,"REFRESH")==0){
			token = strtok(NULL, " ");
			tempo = strtol(token, NULL, 10);
		}

		if(strcmp(token,"QUIT")==0){
			break;
		}
		if(strcmp(token,"QUIT_SERVER")==0){
			close(s_admin);
			exit(0);
		}
	}
}

void processa_client(int client_fd, int num){
	int nread = 0;
	char buffer[BUFLEN];
	do {
		strcpy(buffer,"Insira nome de utilizador: \n\0");
		write(client_fd,buffer,strlen(buffer));
		nread = read(client_fd, buffer, BUFLEN-1);
		buffer[nread] = '\0';
		char nome[nread];
		strcpy(nome,buffer);
		nome[nread-1] = '\0';
		bool encontrado = false;
		for (int i = 1; i < num; i++){
			if (strcmp(nome,utilizadores[i].username)==0){
				encontrado = true;
				strcpy(buffer,"Insira password: \n\0");
				write(client_fd,buffer,strlen(buffer));
				nread = read(client_fd, buffer, BUFLEN-1);
				buffer[nread-1] = '\0';
				if (strcmp(buffer,utilizadores[i].password) == 0){
					printf("Autenticação com sucesso\n");
					strcpy(buffer,"Autenticação com sucesso\n\0");
					write(client_fd,buffer,strlen(buffer));
				}
				else{
					printf("Password Incorreta!\n");
					strcpy(buffer,"Password Incorreta!\n\0");
					write(client_fd,buffer,strlen(buffer));
				}
			}
		}
		if (encontrado == false){
			printf("Utilizador não registado!\n");
			strcpy(buffer,"Utilizador não registado!\n\0");
			write(client_fd,buffer,strlen(buffer));
		}
		fflush(stdout);
	} while (nread > 0);
	close(client_fd);
}