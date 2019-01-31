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
#define BUFFER_SIZE 256

//....COMMANDS....
#define INIT 1
#define SUCCESSFUL_CONNECTION 2
#define WAIT_FOR_COMMAND 3
#define WAIT_ANSWER 4
#define ALL_DEVICE_REQUEST 5
#define INCORRECT_COMMAND 6
#define CONNECTION_CLOSED 7
#define SHUTDOWN_SERVER 8
#define INVALID_CRC 9
//....COMMANDS....

//OFFSETS
#define PRIORITY_OFFSET 0
#define SRC_ID_OFFSET 1
#define ADR_ID_OFFSET 2
#define COMMAND_OFFSET 3
#define DATA_SIZE_OFFSET 4
#define DATA_OFFSET 5

//Priorities
#define PRIORITY_NOTICE 1
#define PRIORITY_COMMAND 2

using namespace std;

static uint8_t currentId;
bool s_access = 1;
bool is_shutdown = false;
int listener;//Socket-listener

struct S
{
	uint8_t array[BUFFER_SIZE];
};

struct Client //Структура клиента
{
	int socket;
	sockaddr_in cl_addr;

	uint8_t id;
	uint8_t type;

	//uint8_t answer[BUFFER_SIZE] = {0};
	vector<S>* answer = {0};
	uint8_t response[BUFFER_SIZE] = {0};
	char name[64];
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
uint8_t check_crc(const char* data, const uint8_t size)
{
      uint8_t crc = 0;
      uint8_t i, j;
      for (i = 0; i < size; i++)
      {
            uint8_t inbyte = data[i];
            for (j = 0 ; j < 8 ; j++)
            {
                  uint8_t mix = (crc ^ inbyte) & 1;
                  crc >>= 1;
                  if (mix) crc ^= 0x8C;
                  inbyte >>= 1;
            }
      }
      return crc;
}
struct Params //Параметры для передачи в поток
{
	Client* cl;
	vector<Client*>* clients;
	Params(Client*& client,vector<Client*>* all_clients)
	{
		cl = client;
		clients=all_clients;
	}
};
void * newConnection(void *_params) //Функция для работы с клиентом
{
	Params* params = (Params*)_params;
	struct sockaddr_in c_addr;
	unsigned int size;//params->cl_addr
	getpeername(params->cl->socket, (struct sockaddr *)&c_addr, &size);
	cout<<"New connection: "<<inet_ntoa(c_addr.sin_addr)<<endl;
	recv(params->cl->socket,params->cl->response,DATA_OFFSET,0);
	uint8_t ans[BUFFER_SIZE];
	uint8_t data_size =params->cl->response[DATA_SIZE_OFFSET];
	cout<<data_size<<endl;
	if(params->cl->response[DATA_OFFSET+data_size]!=check_crc((char*)params->cl->response,DATA_OFFSET+data_size))
	{
		cout<<"Invalid crc "<<(int)params->cl->response[DATA_OFFSET+data_size]<<" | "<<(int)check_crc((char*)params->cl->response,DATA_OFFSET+data_size) <<endl;
		ans[PRIORITY_OFFSET]=PRIORITY_COMMAND;
		ans[SRC_ID_OFFSET]=0xFF;
		ans[COMMAND_OFFSET]=CONNECTION_CLOSED;
		ans[DATA_SIZE_OFFSET]=0;
		ans[DATA_OFFSET+0]=check_crc((char*)ans,DATA_OFFSET);
		send(params->cl->socket,ans,DATA_OFFSET+1,0);
		delete(((Params*)_params)->cl->answer);
		delete(((Params*)_params)->cl); // Освобождаем память
		delete((Params*)_params); // Освобождаем память
		s_access=1;
		return 0;
	}
	if(params->cl->response[PRIORITY_OFFSET]==PRIORITY_COMMAND&&params->cl->response[COMMAND_OFFSET]==INIT&&data_size>1)
	{
		recv(params->cl->socket,params->cl->response+DATA_OFFSET,params->cl->response[DATA_SIZE_OFFSET]+1/*CRC OFFSET*/,0);
		cout<<"Type: "<<(int)params->cl->response[DATA_OFFSET]<<endl;
		params->cl->type=params->cl->response[DATA_OFFSET];
		for(int i = 0;i<data_size-1;i++) params->cl->name[i]=params->cl->response[i];
		vector<Client*>::iterator iter;
		ans[ADR_ID_OFFSET]=0xff;
		uint8_t j=0;
		for(iter = params->clients->begin();iter!=params->clients->end();iter++)
		{
			if((*iter)->id!=j)ans[ADR_ID_OFFSET]=j;
		}
		if(j==0xff)
		{
			//Отвечаем клиенту, что соединение закрыто
			ans[PRIORITY_OFFSET]=PRIORITY_COMMAND;
			ans[SRC_ID_OFFSET]=0xFF;
			ans[COMMAND_OFFSET]=CONNECTION_CLOSED;
			ans[DATA_SIZE_OFFSET]=0;
			ans[DATA_OFFSET+0]=check_crc((char*)ans,DATA_OFFSET);
			send(params->cl->socket,ans,DATA_OFFSET+1,0);
			delete(((Params*)_params)->cl->answer);
			delete(((Params*)_params)->cl); // Освобождаем память
			delete((Params*)_params); // Освобождаем память
			s_access=1;
			return 0;

			//CLOSE CONNECTION//
		}else if(ans[ADR_ID_OFFSET]==0xff){ans[ADR_ID_OFFSET]=j;}
		params->clients->push_back(params->cl);
		ans[PRIORITY_OFFSET]=PRIORITY_COMMAND;
		ans[SRC_ID_OFFSET]=0xFF;
		params->cl->id=ans[ADR_ID_OFFSET];
		ans[COMMAND_OFFSET]=SUCCESSFUL_CONNECTION;
		ans[DATA_SIZE_OFFSET]=0;
		ans[DATA_OFFSET+0]=check_crc((char*)ans,DATA_OFFSET);
		send(params->cl->socket,ans,DATA_OFFSET+1,0);
		s_access=1;
	}
	/*if(params->cl->response[0]==SET_TYPE) //Wait for type of device //Если клиент хочет сообщить тип устройства
	{
		cout<<"Type: "<<(int)params->cl->response[1]<<endl;
		if(params->cl->response[1]<=0||params->cl->response[1]>=TYPES_COUNT){cout<<"Ne to"<<endl;params->cl->answer[0]=6;}//Ошибка в аргументах
		params->cl->type=params->cl->response[1];//Присваиваем тип клиенту
		params->clients->push_back(params->cl); //Пушим клиента в вектор всех клиентов
		params->cl->answer[0]=SUCCESSFUL_CONNECTION; //Отвечаем клиенту, что подключение успешно
		send(params->cl->socket,params->cl->answer,sizeof(params->cl->answer),0);
		memset(params->cl->answer,0,sizeof(params->cl->answer)); // Чистим (Вообще это можно убрать, но руки не доходят)
		s_access=1; // Разрешить подключение
		//LOG THIS COMMAND TO FILE//
	}*/
	else
	{
		//Отвечаем клиенту, что соединение закрыто
		ans[PRIORITY_OFFSET]=PRIORITY_COMMAND;
		ans[SRC_ID_OFFSET]=0xFF;
		ans[COMMAND_OFFSET]=CONNECTION_CLOSED;
		ans[DATA_SIZE_OFFSET]=0;
		ans[DATA_OFFSET+0]=check_crc((char*)ans,DATA_OFFSET);
		send(params->cl->socket,ans,DATA_OFFSET+1,0);
		delete(((Params*)_params)->cl->answer);
		delete(((Params*)_params)->cl); // Освобождаем память
		delete((Params*)_params); // Освобождаем память
		s_access=1;
		return 0;
	}
	params->cl->response[0]=0;
	while(1)
	{
		vector<Client*>::iterator iter;
		if(params->cl->response[0]==0) //Если на все команды был выдан ответ
		{
			int result = recv(params->cl->socket,params->cl->response,DATA_OFFSET,0);
			if(result<=0){break;}
			result = recv(params->cl->socket,params->cl->response+DATA_OFFSET,params->cl->response[DATA_SIZE_OFFSET]+1,0);
			if(result<=0){break;}
			data_size=params->cl->response[DATA_SIZE_OFFSET];
			if(params->cl->response[DATA_OFFSET+data_size]!=check_crc((char*)params->cl->response,DATA_OFFSET+data_size))
			{
				cout<<"INVALID CRC ON " << params->cl->id << " id" <<endl;
				ans[PRIORITY_OFFSET]=PRIORITY_COMMAND;
				ans[SRC_ID_OFFSET]=0xff;
				ans[ADR_ID_OFFSET]=params->cl->id;
				ans[COMMAND_OFFSET]=INVALID_CRC;
				ans[DATA_SIZE_OFFSET]=0;
				ans[DATA_OFFSET]=check_crc((char*)ans,DATA_OFFSET);
				send(params->cl->socket,ans,DATA_OFFSET+ans[DATA_SIZE_OFFSET]+1,0);
				continue;
			}
		}

		if(params->cl->response[COMMAND_OFFSET]==SHUTDOWN_SERVER)
		{
			is_shutdown=true;
			cout<<"Shutdowning.."<<endl;
		}
		if(is_shutdown)
		{
			ans[PRIORITY_OFFSET]=PRIORITY_COMMAND;
			ans[SRC_ID_OFFSET]=0xFF;
			ans[COMMAND_OFFSET]=CONNECTION_CLOSED;
			ans[DATA_SIZE_OFFSET]=0;
			ans[DATA_OFFSET+0]=check_crc((char*)ans,DATA_OFFSET);
		}
		else
		{
			if(params->cl->answer->empty())
				{
					ans[PRIORITY_OFFSET]=PRIORITY_COMMAND;
					ans[SRC_ID_OFFSET]=0xFF;
					ans[ADR_ID_OFFSET]=params->cl->id;
					ans[COMMAND_OFFSET]=WAIT_FOR_COMMAND;
					ans[DATA_SIZE_OFFSET]=0;
					ans[DATA_OFFSET]=check_crc((char*)ans,DATA_OFFSET);
				}
			else
			{
				S s = *params->cl->answer->begin();
				for(int i = 0; i < DATA_OFFSET + s.array[DATA_SIZE_OFFSET];i++)
				{
					ans[i] = s.array[i];
				}
			}
			params->cl->answer->erase(params->cl->answer->begin());
		}
		if(params->cl->response[SRC_ID_OFFSET]==0xFF)
		{
			for(iter=params->clients->begin();iter!=params->clients->end();iter++)
			{
					//if((*iter)->id==params->cl->id)continue;
					S s;
					for(int i = 0; i<DATA_OFFSET+params->cl->response[DATA_SIZE_OFFSET];i++)
					{
						s.array[i]=params->cl->response[i];
					}

					(*iter)->answer->push_back(s);
			}
		}

		//log//
		cout<<(int)ans[PRIORITY_OFFSET]<<" "<<(int)ans[SRC_ID_OFFSET]<<" "<<(int)ans[ADR_ID_OFFSET]<<" "<<(int)ans[COMMAND_OFFSET]<<" ";
		for(int i = 0; i < ans[DATA_SIZE_OFFSET];i++) cout << ans[DATA_OFFSET+i]<< " ";
		cout<<ans[DATA_OFFSET+ans[DATA_SIZE_OFFSET]]<<endl;
		send(params->cl->socket,ans,DATA_OFFSET+ans[DATA_SIZE_OFFSET]+1,0);
	}//asd

	cout<<"Client disconnected"<<endl;

	vector<Client*>::iterator i;
	for(i = params->clients->begin();i!=params->clients->end();++i)
	{
		if((*i)->id==params->cl->id)
		{
			params->clients->erase(i);
			delete(((Params*)_params)->cl);
			delete((Params*)_params);
			break;
		}
	}

	return (void *)0;
}
bool isDone = false;
struct ac_params
{
	int socket;
	sockaddr_in cl_addr;
};
void * t_accept(void * _params)
{
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
	ac_params* params = (ac_params*) _params;

	params->socket = accept(listener, NULL, NULL);
	s_access = 0;
	isDone=true;
	return (void *) 0;
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
	memset(&(addr.sin_zero),0,8); //
	int err = bind(listener, (struct sockaddr *)&addr,sizeof(addr));
	if(err<0)
	{
		cout << "bind error "<< endl;
		return 2;
	}

	listen(listener,3);
	cout<<"listening on fd "<< listener << "..."<<endl;
	vector<Client*> clients;
	cout << "wait for connection.." << endl;

	while (1)
	{
		pthread_t ac_thread;
		cout<<"[Main]: Ready for accept"<<endl;
		struct ac_params ac_par;
		pthread_create(&ac_thread,NULL,t_accept,(void *) &ac_par);
		while(1)
		{
			if(is_shutdown)
			{
				pthread_cancel(ac_thread);
				if(clients.size()==0)break;
			}
			else if(isDone)
			{
				isDone=false;
                 break;
			}
		}
		if(ac_par.socket<0||is_shutdown){break;}
		Client* client = new Client(ac_par.socket,ac_par.cl_addr);
		Params* par = new Params(client,&clients);
		pthread_t thread;
		pthread_create(&thread,NULL,newConnection,(void *)par);
		while(1){
			if(s_access)break;
		}
	}
	cout<<"[Main]: Socket closing"<<endl;
	cout<<close(listener)<<endl;

	return 0;
}
