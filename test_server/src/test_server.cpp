#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <vector>
#include <pthread.h>
#define TYPES_COUNT 5
#define SET_TYPE 1
#define SUCCESSFUL_CONNECTION 2
#define WAIT_FOR_COMMAND 3
#define WAIT_ANSWER 4
#define ALL_DEVICE_REQUEST 5
#define INCORRECT_COMMAND 6
#define CONNECTION_CLOSED 7
#define SHUTDOWN_SERVER 8

using namespace std;
static int currentId;
char accesss = 1;
int listener;

struct Client
{
	int id;
	int socket;
	unsigned char type;
	sockaddr_in cl_addr;
	char answer[512] = {0};
	char response[512] = {0};
	Client(int cl_socket,sockaddr_in client_addr)
	{
		id=currentId++;
		socket = cl_socket;
		cl_addr.sin_addr = client_addr.sin_addr;
		cl_addr.sin_family = client_addr.sin_family;
		cl_addr.sin_port = client_addr.sin_port;
		memset(&(cl_addr.sin_zero),0,8);
		type=-1;
	}
};
struct Params
{
	Client* cl;
	vector<Client*>* clients;
	Params(Client*& client,vector<Client*>* all_clients)
	{
		cl = client;
		clients=all_clients;
	}
};
void * newConnection(void *_params)
{
	Params* params = (Params*)_params;
	cout<<"New connection: "<<inet_ntoa(params->cl->cl_addr.sin_addr)<<endl;
	recv(params->cl->socket,params->cl->response,sizeof(params->cl->response),0);
	if(params->cl->response[0]==SET_TYPE) //Wait for type of device
	{
		cout<<"Type: "<<(int)params->cl->response[1]<<endl;
		if(params->cl->response[1]<=0||params->cl->response[1]>=TYPES_COUNT){cout<<"Ne to"<<endl;params->cl->answer[0]=6;}
		params->cl->type=static_cast<unsigned char>(params->cl->response[1]);
		params->clients->push_back(params->cl);
		params->cl->answer[0]=SUCCESSFUL_CONNECTION;
		send(params->cl->socket,params->cl->answer,sizeof(params->cl->answer),0);
		memset(params->cl->answer,0,sizeof(params->cl->answer));
		accesss=1;
		//LOG THIS COMMAND TO FILE//
	}else
	{
		params->cl->answer[0]=CONNECTION_CLOSED;
		send(params->cl->socket,params->cl->answer,sizeof(params->cl->answer),0);
		delete((Params*)_params);
		return 0;
	}
	params->cl->response[0]=0;
	params->cl->answer[0]=0;
	while(1)
	{
		vector<Client*>::iterator iter;
		if(params->cl->response[0]==0)
		{
			int result = recv(params->cl->socket,params->cl->response,sizeof(params->cl->response),0);
			if(result<=0){break;}
		}
		if(params->cl->response[0]==WAIT_FOR_COMMAND)
		{
			if(params->cl->answer[0]==0) params->cl->answer[0]=WAIT_FOR_COMMAND;
		}
		else if(params->cl->response[0]==ALL_DEVICE_REQUEST)
		{
			cout<<"Запрос всех устройств!"<<endl;
			params->cl->answer[0]=ALL_DEVICE_REQUEST;
			params->cl->answer[1]=params->clients->size();
			int index = 2;
			for(iter = params->clients->begin();iter!=params->clients->end();++iter)
			{
				memset(params->cl->answer+index,(*iter)->id,sizeof(int));
				index+=sizeof(int);
				params->cl->answer[index]=(*iter)->type;
				index++;
			}
		}
		else if(params->cl->response[0]==SHUTDOWN_SERVER)
		{
			for(iter = params->clients->begin();iter!=params->clients->end();++iter)
			{
                 (*iter)->answer[0]=CONNECTION_CLOSED;
			}
			cout<<"Shutdowning"<<endl;
			close(listener);
		}
		else
		{
			params->cl->answer[0]=INCORRECT_COMMAND;
		}

		send(params->cl->socket,params->cl->answer,sizeof(params->cl->answer),0);
		if(params->cl->answer[0]==CONNECTION_CLOSED){delete((Params*)_params);break;}
		memset(params->cl->answer,0,sizeof(params->cl->answer));
		memset(params->cl->response,0,sizeof(params->cl->response));
	}

	cout<<"Client disconnected"<<endl;
	vector<Client*>::iterator i;
	for(i = params->clients->begin();i!=params->clients->end();++i)
	{
		if((*i)->id==params->cl->id)
		{
			delete((*i));
			params->clients->erase(i);
		}
	}
	return 0;
}
int main()
{

	struct sockaddr_in addr;
	listener = socket(AF_INET, SOCK_STREAM, 0);

	if(listener < 0)
	{
		cout << "socket error" << endl;
		return 1;
	}
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1132);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(&(addr.sin_zero),0,8);
	int err = bind(listener, (struct sockaddr *)&addr,sizeof(addr));
	if(err<0)
	{
		cout << "bind error "<< endl;
		return 2;
	}

	listen(listener,3);
	cout<<"listening on socket "<< listener << "..."<<endl;
	vector<Client*> clients;
	cout << "wait for connection.." << endl;

	/*while(1)
	    {
	           	            accesss = false;
	            while(true)
	            {
	                if(accesss == true)
	                {
	                    clients.push_back(new Client());
	                    break;
	                }
	            }
	        }*/
	while (1)
	{
		pthread_t thread;
		struct sockaddr_in c_addr;
		unsigned int size;
		cout<<"[Main]: Ready for accept"<<endl;
		int c_sock = accept(listener, (struct sockaddr *)&c_addr, &size);
		if(c_sock<0){break;}//&(new Params(new Client(c_sock,c_addr)))
		Client* client = new Client(c_sock,c_addr);
		Params* par = new Params(client,&clients);
		pthread_create(&thread,NULL,newConnection,(void *)par);

		accesss = 0;
		while(1){if(accesss)break;}
	}
	cout<<"[Main]: Socket closing"<<endl;
	close(listener);
	return 0;
}