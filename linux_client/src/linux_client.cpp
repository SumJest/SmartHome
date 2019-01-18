#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SET_TYPE 1
#define SUCCESSFUL_CONNECTION 2
#define WAIT_FOR_COMMAND 3
#define WAIT_ANSWER 4
#define ALL_DEVICE_REQUEST 5
#define INCORRECT_COMMAND 6
#define CONNECTION_CLOSED 7
#define SHUTDOWN_SERVER 8

using namespace std;

int main() {

	int sock;
	struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0)
	{
		cout<<"sock error"<<endl;
		return 1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(25564);
	addr.sin_addr.s_addr = inet_addr("192.168.1.106");

	if(connect(sock,(struct sockaddr *)&addr,sizeof(addr))<0)
	{
		cout<<"connection error"<<endl;
		close(sock);
		return 2;
	}

	char response[512] = {0};
	char answer[512] = {0};

	response[0] = 1;
	response[1] = 2;

	send(sock,response,sizeof(response),0);
	recv(sock,answer,sizeof(answer),0);
	if(answer[0]==2)cout<<"Answer: connection successful"<<endl;
	else cout<<"Answer: "<<(int)answer[0]<<endl;


	if(answer[0]==CONNECTION_CLOSED)
	{
		cout<<"Server closed connection."<<endl;
		close(sock);
		return 0;
	}
	response[0]=5;
	send(sock,response,sizeof(response),0);
	recv(sock,answer,sizeof(answer),0);
	cout<<"Answer: "<<(int)answer[0]<<endl;
	int count = answer[1];
	cout<<"Device count: "<<count<<endl;
	int index = 2;
	for(int i = 0; i < count; i++)
	{
		int id = *(answer+index);
		index+=sizeof(int);
		cout<<"Id: "<<id<<" ";
		cout<<"Type: "<<(int)answer[index++]<<endl;
	}
	response[0]=SHUTDOWN_SERVER;

	send(sock,response,sizeof(response),0);
	recv(sock,answer,sizeof(answer),0);

	cout<<(int)answer[0]<<endl;

	close(sock);
	return 0;
}
