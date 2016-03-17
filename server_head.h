#include "share_head.h"
#include <pthread.h>
#include <time.h>
#include <sys/select.h>
#include <errno.h>


//定义限制值
#define BUFF_MAX 1024    //缓存最大值
#define CHANNEL_MAX  32    //聊天室数量最大值
#define USERNAME_MAX 32    //用户名长度
#define USER_MAX 64     //单聊天室用户数量
#define FD_MAX 64       //描述符数量最大值 
#define CHANNELNAME_MAX 32   //聊天室名字长度
#define SAY_MAX 64       //每次聊天的最大字节数
 
#define SERPORT  8888    //服务器端口号

//定义用户类型,采用链表的形式
typedef struct user
{
	struct sockaddr_in ipAddr;   //用户ip地址
	char userName[USERNAME_MAX];  //用户名 
	struct user *next;          //指向下一个用户
	int flag;                   //若为1，则该用户当前频道是该聊天室，若为0，则该用户当前频道不是该聊天室;
}user;

//定义聊天室结构
typedef struct chatRoom
{
	char roomName[CHANNELNAME_MAX];   //聊天室名字,聊天室唯一标识符
	pthread_mutex_t roomMutex;        //互斥锁
	struct chatRoom *next;     
	user *userList;                  //指向该聊天室的第一个用户
}chatRoom;


//定义该服务器中所有聊天室
typedef struct listChatRoom 
{
	chatRoom *firstRoom;   //指向第一个聊天室
	chatRoom *lastRoom;    //指向最后一个聊天室
	int size;              //聊天室数量
}listChatRoom;

//定义线程参数类型
typedef struct threadPara 
{
	struct sockaddr_in clAddr;  //客户端IP地址
	char content[BUFF_MAX];     //客户发来的内容
	int fd;                     //文件描述符
}threadPara;


listChatRoom glChatRoom;            //记录所有的聊天室

//错误处理函数
void errSys(const char *str)
{
	fprintf(stderr,"%s:%s\n",str,strerror(errno));
	exit(1);
}

//创建一个聊天室
void createRoom(const char *name)
{
	chatRoom *room = (chatRoom *)malloc(sizeof(chatRoom));

	//初始化该聊天室
	strcpy(room->roomName,name);
//	pthread_mutexattr_t attr;
//	pthread_mutexattr_init(&attr);
//	pthread_mutexattr_setpshared(&attr,PTHREAD_PROCESS_PRIVATE);
	room->next = NULL;
	room->userList = NULL;
	pthread_mutex_init(&room->roomMutex,NULL);
	
	//将该聊天室加入列表中
	if(glChatRoom.size == 0)
	{
		glChatRoom.size++;
		glChatRoom.firstRoom = room;
		glChatRoom.lastRoom = room;
	}
	else
	{
	
		glChatRoom.size++;
		glChatRoom.lastRoom -> next = room;
		glChatRoom.lastRoom = room;
	}

}

//判断两个地址是否相等
int addrEqual(struct sockaddr_in addr1,struct sockaddr_in addr2)
{
	if(addr1.sin_family == addr2.sin_family && ntohs(addr1.sin_port) == ntohs(addr2.sin_port) && (!strcmp(inet_ntoa(addr1.sin_addr),inet_ntoa(addr2.sin_addr))))
	{
		return 1;
	}	
	return 0;
}

//线程出错处理函数
void errThread(const char *str,int err)
{
	fprintf(stderr,"%s:%s\n",str,strerror(err));
	pthread_exit(NULL);
}


//查找某用户是否在聊天室列表中
chatRoom* findUserFromList(struct sockaddr_in addr)
{
	//查找到该用户，返回该用户所在的聊天室地址，否则返回NULL;	
	chatRoom *curRoom = glChatRoom.firstRoom;
	user *curUser;
	int err;
	while(curRoom)
	{
		//对该聊天室上锁
		if((err = pthread_mutex_lock(&curRoom->roomMutex)) != 0)
		{
			errThread("findUserFromList lock error",err);
		}
		curUser = curRoom->userList;
		while(curUser)		
		{
			if(addrEqual(addr,curUser->ipAddr) && curUser->flag)
			{
				if((err = pthread_mutex_unlock(&curRoom->roomMutex)) != 0)
				{
					errThread("findUserFromList unlock error",err);
				}
				return curRoom;	
			}
			curUser = curUser->next;
		}
		if((err = pthread_mutex_unlock(&curRoom->roomMutex)) != 0)
		{
			errThread("findUserFromList unlock error",err);
		}
		curRoom = curRoom->next;
		if(curRoom == NULL)
			break;
	}
	return NULL;
}

//根据聊天室名字查找该聊天室，若能找到，返回聊天室地址，反之，返回NULL
chatRoom *findRoomFromList(const char* roomName)
{
	chatRoom *curRoom = glChatRoom.firstRoom;
	int listSize = glChatRoom.size;
	while(listSize--)
	{
		if(strcmp(roomName,curRoom->roomName) == 0)
		{
			return curRoom;
		}
		curRoom = curRoom->next;
	}
	return NULL;
}

//添加用户到聊天室中，若该聊天室不存在，则返回0，若添加成功，则返回1
int addUserToRoom(struct sockaddr_in addr,const char *userName,const char *roomName)
{
	chatRoom *room = findRoomFromList(roomName);
	if(room == NULL)
	{
		return 0;
	}	
	

	//对该聊天室上锁
	int err;
	if((err = pthread_mutex_lock(&room->roomMutex)) != 0)
	{
		errThread("addUserToRoom lock error",err);
	}
	
	user *curUser = (user*)malloc(sizeof(user));
	memcpy(&(curUser->ipAddr),&addr,sizeof(addr));
	//curUser->ipAddr = addr;
	printf("%s\n",inet_ntoa(curUser->ipAddr.sin_addr));
	strcpy(curUser->userName,userName); 
	curUser->next = NULL;
	curUser->flag = 1;
	
	if(room->userList == NULL)
	{
		room->userList = curUser;
	}
	else
	{
		user *begin = room->userList;
		while(begin->next)
		{
			begin = begin->next;
		}
		begin->next = curUser;
	}
	if((err = pthread_mutex_unlock(&room->roomMutex)) != 0)
	{
		errThread("addUserToRoom lock error",err);
	}

	printf("add user %s to room %s\n",userName,roomName);
	return 1;

}


//从一个聊天室中根据地址查找到该用户，并返回该用户指针
user *findUserFromRoom(struct sockaddr_in addr,chatRoom *room)
{
	user *curUser = room->userList;
	if(curUser == NULL)
	{
		printf("Room:%s为空\n",room->roomName);
		return NULL;
	}
	while(curUser)
	{
		if(addrEqual(addr,curUser->ipAddr))
			return curUser;
		curUser = curUser->next;
	}
	return NULL;
}

//将聊天内容发送到该聊天室中所有用户
void reqSay(threadPara request,chatRoom *userRoom)
{
	//先对该聊天室加锁
	int err;
	printf("name:%s\n",userRoom->roomName);
	if((err = pthread_mutex_lock(&userRoom->roomMutex)) != 0)
	{
		errThread("reqSay lock error",err);
	}
	user* curUser = userRoom->userList;
	if(curUser == NULL)	
	{
		printf("reqSay error:no user.\n");
		if((err = pthread_mutex_unlock(&userRoom->roomMutex)) != 0)
		{
			errThread("reqSay unlock err",err);
		}
		return;
	}

	//根据地址，查找到该用户的位置
	user *srcUser = findUserFromRoom(request.clAddr,userRoom);
	if(srcUser == NULL)
	{
		printf("no this user in %s\n",userRoom->roomName);
		if((err = pthread_mutex_unlock(&userRoom->roomMutex)) != 0)
		{
			errThread("reqSay unlock err",err);
		}

		return;
	}
	//发送消息的用户名
	char *userName = srcUser->userName;
	int len = strlen(request.content) + 3 + strlen(userName);   //加3的原因是一个':'，一个'\n',一个'\0';
	char *buff = (char *)malloc(len);   //改为动态数组
	snprintf(buff,len,"%s:%s\n",userName,request.content);
	printf("%s\n",curUser->userName );
	printf("%s\n",inet_ntoa(curUser->ipAddr.sin_addr));
	while(curUser)
	{
		if(curUser->flag == 1) 
		{
			err = sendto(request.fd,buff,len,0,(struct sockaddr*)&(curUser->ipAddr),sizeof(curUser->ipAddr));
			if(err < 0)
			{
				printf("send to %s error\n",inet_ntoa(curUser->ipAddr.sin_addr));
			}

		}
		curUser = curUser->next;

	//	printf("fd:%d\n",request.fd);
	//	printf("flag:%d\n",flag);
	//	printf("%s fd:%d flag:%d  ",buff,request.fd,flag);
		//printf("%ld\n",sizeof(curUser->ipAddr) );
		//printf("%s\n",inet_ntoa(curUser->ipAddr.sin_addr));
	}
	if((err = pthread_mutex_unlock(&userRoom->roomMutex)) != 0)
	{
		errThread("reqSay unlock err",err);
	}
	free(buff);
	
}

//列出当前存在的聊天室
void reqList(int fd,struct sockaddr_in addr)
{
	chatRoom *curRoom = glChatRoom.firstRoom;
	char buff[1067] = "Room List:\n";   //1067 = 11 + CHANNEL_MAX * (CHANNELNAME_MAX + 1) ,存储列表名字
	while(curRoom)	
	{
		sprintf(buff + strlen(buff),"%s\n",curRoom->roomName);
		curRoom = curRoom->next;
	}
	int flag = sendto(fd,buff,strlen(buff),0,(struct sockaddr*)&(addr),sizeof(addr));
	if(flag < 0)
	{
		printf("send to %s error\n",inet_ntoa(addr.sin_addr));
	}

}

//列出某聊天室的所有成员
void reqWho(const char* cmd,int fd,struct sockaddr_in addr)
{
	chatRoom* room = findRoomFromList(cmd);
	int flag;
	char buff[2153];    //2153 = CHANNELNAME_MAX + 9 + USER_MAX * (USERNAME_MAX + 1) ,存储聊天室用户名字
	if(room == NULL)
	{
		sprintf(buff,"No room %s in room list.\n",cmd);
	}
	else
	{
		int err;
		if((err = pthread_mutex_lock(&room->roomMutex)) != 0)
		{
			errThread("reqWho lock err",err);
		}
		user* curUser = room->userList;
		sprintf(buff,"%s member:\n",cmd);
		while(curUser)
		{
			sprintf(buff + strlen(buff),"%s\n",curUser->userName);
			curUser = curUser->next;
		}
		if((err = pthread_mutex_unlock(&room->roomMutex)) != 0)
		{
			errThread("reqWho unlock err",err);
		}
	}
	flag = sendto(fd,buff,strlen(buff),0,(struct sockaddr*)&(addr),sizeof(addr));
	if(flag < 0)
	{
		printf("send to %s error\n",inet_ntoa(addr.sin_addr));
	}

}


//执行/join 指令，将用户加入指定的聊天室，若该聊天室不存在，则创建该聊天室
void reqJoin(struct sockaddr_in addr,const char *roomName,chatRoom *srcRoom)
{
	if(strcmp(roomName,srcRoom->roomName) == 0)	 //如果要加入的聊天室就是当前聊天室，则不做处理
		return;
	
	//如果将要加入的聊天室中已经有该用户了，那么直接将该用户的flag变为1即可。
	chatRoom* desRoom = findRoomFromList(roomName);
	int err;
	int jud = 0;

	 //若该要加入的聊天室不存在，则新建一个
	if(desRoom == NULL)
	{
		createRoom(roomName);
	}
	else
	{
		//查找该用户是否已经存在于将要加入的聊天室中了
		//先对该聊天室上锁

		if((err = pthread_mutex_lock(&desRoom->roomMutex)) != 0)
		{
			errThread("reqJoin lock err",err);
		}

		user *tmpUser = findUserFromRoom(addr,desRoom);
		if(tmpUser)   //若该用户已经存在于该聊天室中了
		{
			tmpUser->flag = 1;
			jud = 1;
		}

		if((err = pthread_mutex_unlock(&desRoom->roomMutex)) != 0)
		{
			errThread("reqJoin unlock err",err);
		}

	}

	//将该用户从原聊天室中取出
	if((err = pthread_mutex_lock(&srcRoom->roomMutex)) != 0)
	{
		errThread("reqJoin lock err",err);
	}

	user* desUser = findUserFromRoom(addr,srcRoom);    //指向要加入其它聊天室的用户
	desUser->flag = 0;
	if((err = pthread_mutex_unlock(&srcRoom->roomMutex)) != 0)    //将用户从原来的聊天室提取出来后解锁原聊天室
	{
		errThread("reqJoin unlock err",err);
	}

	//如果该用户不存在于将要加入的聊天室中，那么新建立一个用户存于该聊天室中
	if(jud == 0)
	{
		addUserToRoom(addr,desUser->userName,roomName);
	}

	printf("%s join %s\n",desUser->userName,roomName);
	
}

//处理/switch指令，切换到另一个聊天室，如果该聊天室不存在，则切换失败。一个用户可以加入多个聊天室。
void reqSwitch(int fd,struct sockaddr_in addr,const char* roomName,chatRoom *srcRoom)
{
	if(strcmp(roomName,srcRoom->roomName) == 0)	 //如果要加入的聊天室就是当前聊天室，则不做处理
		return;


	//如果将要加入的聊天室中已经有该用户了，那么直接将该用户的flag变为1即可。
	chatRoom* desRoom = findRoomFromList(roomName);
	int err;
	int jud = 0;

	 //若该要加入的聊天室不存在，则新建一个
	if(desRoom == NULL)
	{
		char buff[47];  //47 = CHANNELNAME_MAX + 15	
		sprintf(buff,"%s is not exist.\n",roomName);
		err = sendto(fd,buff,strlen(buff),0,(struct sockaddr*)&(addr),sizeof(addr));
		if(err < 0)
		{
			printf("send to %s error\n",inet_ntoa(addr.sin_addr));
		}
		return;

	}
	else
	{
		//查找该用户是否已经存在于将要加入的聊天室中了
		//先对该聊天室上锁

		if((err = pthread_mutex_lock(&desRoom->roomMutex)) != 0)
		{
			errThread("reqJoin lock err",err);
		}

		user *tmpUser = findUserFromRoom(addr,desRoom);
		if(tmpUser)   //若该用户已经存在于该聊天室中了
		{
			tmpUser->flag = 1;
			jud = 1;
		}

		if((err = pthread_mutex_unlock(&desRoom->roomMutex)) != 0)
		{
			errThread("reqJoin unlock err",err);
		}

	}

	//将该用户从原聊天室中取出
	if((err = pthread_mutex_lock(&srcRoom->roomMutex)) != 0)
	{
		errThread("reqJoin lock err",err);
	}

	user* desUser = findUserFromRoom(addr,srcRoom);    //指向要加入其它聊天室的用户
	desUser->flag = 0;
	if((err = pthread_mutex_unlock(&srcRoom->roomMutex)) != 0)    //将用户从原来的聊天室提取出来后解锁原聊天室
	{
		errThread("reqJoin unlock err",err);
	}

	//如果该用户不存在于将要加入的聊天室中，那么新建立一个用户存于该聊天室中
	if(jud == 0)
	{
		addUserToRoom(addr,desUser->userName,roomName);
	}

	printf("%s switch %s\n",desUser->userName,roomName);

}

//执行/leave指令，将该用户从聊天室删除 
void reqLeave(int fd,struct sockaddr_in addr,const char *roomName,chatRoom *srcRoom)
{
	//先查找该用户是否存在于将要退出的聊天室中，如果不是甚至该聊天室不存在，那么不需要做任何处理。
	//如果存在于该聊天室中，其将其找出并删除，释放空间
	chatRoom *delRoom = findRoomFromList(roomName);
	if(delRoom == NULL)
	{
		return;
	}


	int err;
	//如果将要退出的聊天室就是当前所在的聊天室，那么频道回到Common，若是Common频道，则不退出。
	char buff[BUFF_MAX];
	if(strcmp(roomName,"Common") == 0)	 
	{
		sprintf(buff,"Sorry,Common 聊天室不可退出\n");
		if((err = sendto(fd,buff,strlen(buff),0,(struct sockaddr*)&addr,sizeof(addr))) == -1)	
		{
			printf("sendto %s error.\n",inet_ntoa(addr.sin_addr));
		}

		return;
	}

	if(strcmp(roomName,srcRoom->roomName) == 0)	
	{
		//将频道切换到Common聊天室中
		chatRoom *commonRoom = findRoomFromList("Common");
		if((err = pthread_mutex_lock(&commonRoom->roomMutex)))
		{
			errThread("reqLeave lock err",err);
		}
		user *commonUser = findUserFromRoom(addr,commonRoom);
		commonUser->flag = 1;
		if((err = pthread_mutex_unlock(&commonRoom->roomMutex)))
		{
			errThread("reqLeave unlock err",err);
		}

	}
	
	//将该用户从退出的聊天室中删除
	if((err = pthread_mutex_lock(&delRoom->roomMutex)))
	{
		errThread("reqLeave lock err",err);
	}
	user *desUser = findUserFromRoom(addr,delRoom);
	if(desUser == NULL)
	{
		//该用户不存在于将要退出的聊天室中	
		if((err = pthread_mutex_unlock(&delRoom->roomMutex)))
		{
			errThread("reqLeave unlock err",err);
		}
		return;
	}

	//如果存在，那么将其删除
	user* curUser= delRoom->userList;
	if(addrEqual(curUser->ipAddr,addr))	
	{
		delRoom->userList = curUser->next;	
	}
	else
	{
		while(curUser->next)
		{
		
			if(addrEqual(curUser->next->ipAddr,addr))
			{
				curUser->next = desUser->next;
				break;
			}
			curUser = curUser->next;
		}
	}
	if((err = pthread_mutex_unlock(&delRoom->roomMutex)) != 0)
	{
		errThread("reqLeave unlock err",err);
	}
	sprintf(buff,"退出聊天室%s\n",roomName);
	if((err = sendto(fd,buff,strlen(buff),0,(struct sockaddr*)&addr,sizeof(addr))) == -1)	
	{
		printf("sendto %s error.\n",inet_ntoa(addr.sin_addr));
	}

	free(desUser);

}


//对指令进行处理
void handleCmd(threadPara request,chatRoom *userRoom)
{
	char *cmd = request.content;  //指令内容
	if(cmd[0] != '/')
	{
		reqSay(request,userRoom);   //聊天的内容
		return;
	}
	if(strcmp(cmd,"/list") == 0)
	{
		reqList(request.fd,request.clAddr);
		return;
	}
	if(strncmp(cmd,"/who ",5) == 0)
	{
		reqWho(cmd+5,request.fd,request.clAddr);
		return;
	}
	if(strncmp(cmd,"/join ",6) == 0)
	{
		reqJoin(request.clAddr,cmd+6,userRoom);
		return;
	}
	if(strncmp(cmd,"/switch ",8) == 0)
	{
		reqSwitch(request.fd,request.clAddr,cmd+8,userRoom);
		return;
	}
	if(strncmp(cmd,"/leave ",7) == 0)
	{
		reqLeave(request.fd,request.clAddr,cmd+7,userRoom);
		return;
	}
}

//处理请求
void* handleRequest(void *arg)
{
	threadPara request = *((threadPara*)arg);

	pthread_detach(pthread_self());   //将线程属性设置为分离状态
	printf("%s:%d buff:%s\n",inet_ntoa(request.clAddr.sin_addr),request.clAddr.sin_port,request.content);
	chatRoom *userRoom = findUserFromList(request.clAddr);
	if(userRoom == NULL)     //如果不存在，则将该用户放入Common聊天室中
	{
		if(addUserToRoom(request.clAddr,request.content,"Common") == 0)  //如果Common聊天室不存在了,则重新创建一个
		{
			createRoom("Common");
			addUserToRoom(request.clAddr,request.content,"Common"); 
		}

	}
	else                 //如果存在，则对该接受的指令进行处理
	{
		handleCmd(request,userRoom);
	}

	return NULL;
}
