#include "server_head.h"

//listChatRoom glChatRoom;            //记录所有的聊天室

int main()
{
	int sockfd;
	struct sockaddr_in servaddr;

	//初始化聊天室列表
	glChatRoom.firstRoom = NULL;
	glChatRoom.lastRoom = NULL;
	glChatRoom.size = 0;
	
	//创建Common聊天室
	createRoom("Common");

	//开始socket编程
	if((sockfd = socket(AF_INET,SOCK_DGRAM,0)) < 0)
	{
		errSys("socket error")	;
	}

	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERPORT);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0)
	{
		errSys("bind error");
	}
	
	fd_set scanfd;           //侦听字符集
	char buff[BUFF_MAX];
	struct sockaddr_in clientAddr;
	int n;
	int len;
	threadPara request;      //存储连接时的信息
	pthread_t thread;         //线程ID
	while(1)
	{
		memset(buff,0,sizeof(buff));
		memset(&clientAddr,0,sizeof(clientAddr));
		FD_ZERO(&scanfd);       
		FD_SET(sockfd,&scanfd);
		select(sockfd + 1,&scanfd,NULL,NULL,NULL);  //select阻塞，等待有连接过来
		if(FD_ISSET(sockfd,&scanfd))
		{
			if((n = recvfrom(sockfd,buff,BUFF_MAX - 1,0,(struct sockaddr*)&clientAddr,&len)) < 0)
			{
				errSys("recvfrom error");
			}
			//printf("n=%d\n",n);
			buff[n] = '\0';
			
			//设置将要传递的线程参数
			memcpy(&request.clAddr,&clientAddr,sizeof(clientAddr));
			strcpy(request.content,buff);
			request.fd = sockfd;
			//创建线程
			int err;   //记录错误编码	
			if((err = pthread_create(&thread,NULL,handleRequest,(void*)&request)) != 0)
			{
				fprintf(stderr,"thread create error:%s\n",strerror(err));
				exit(1);
			}
		}
	}
	close(sockfd);
	return 0;
}
