#include "share_head.h"

#define BUFF_MAX 1024

#define USERNAME_MAX 32    //用户名长度


void useGuide()
{
	printf("温馨提示:\n");
	printf("1.每个用户可以加入多个聊天室;\n");
	printf("2.启动客户端的时候默认加入Common聊天室;\n");
	printf("3./join ChannelName是加入聊天室，如果聊天室不存在，则创建该聊天室.\n");
	printf("4./leave ChannelName是退出聊天室，Commo聊天室不可以退出.\n");
	printf("5./switch ChannelName是切换聊天室，如果聊天室不存在，则切换失败.\n");
	printf("6./list列出当前存在的聊天室.\n");
	printf("7./who ChannelName列出指定的聊天室的成员.\n");
	printf("8.不加\"/\"表示用户发聊天内容.\n");
}

int main(int argc,char *argv[])
{
	int clientfd;
	struct sockaddr_in servaddr;

	if(argc != 4 || strlen(argv[3]) >= USERNAME_MAX)    //用户名不能过长
	{
		printf("请输入四个参数，第一个为程序名，第二个为服务器端IP地址，第三个为端口号，第四个为用户名.\n");
		fprintf(stderr,"usage:%s\n",argv[0]);
		exit(1);
	}

	useGuide();
	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET,argv[1],&servaddr.sin_addr);

	if((clientfd = socket(AF_INET,SOCK_DGRAM,0)) < 0)
	{
		fprintf(stderr,"socket error:%s\n",strerror(errno));
		exit(1);
	}
	
	sendto(clientfd,argv[3],strlen(argv[3]),0,(struct sockaddr*)&servaddr,sizeof(servaddr));
//	printf("size:%ld\n",strlen(argv[3]));
	pid_t pid = fork();
	if(pid > 1)   //父进程,发送消息给服务器
	{
		char buff[BUFF_MAX] = {'\0'};
		int n;	
		while(n = read(STDOUT_FILENO,buff,BUFF_MAX))
		{
			if(n == 1)
			{
				printf("发出的指令不能为空,请重新输入。\n")	;
				continue;
			}
			buff[n-1] = '\0';
			n--;
			//printf("num:%d\n",n);
			sendto(clientfd,buff,n,0,(struct sockaddr*)&servaddr,sizeof(servaddr));
			memset(buff,0,n);
		}
		wait(NULL);
	}
	else if(pid == 0) //子进程,从服务器接收信息
	{
		char clBuff[1059] = {'\0'};   //1059 = BUFF_MAX + USERNAME_MAX + 3
		int n;
		struct sockaddr addr;
		int len;	
		while(1)
		{
			n = recvfrom(clientfd,clBuff,1058,0,&addr,&len);
			//printf("n = %d\n", n);
			clBuff[n] = '\0';
			//printf("%s",clBuff);
			write(STDIN_FILENO,clBuff,n);

		}
	}
	else
	{
		fprintf(stderr,"fork error:%s\n",strerror(errno));
		exit(1);
	}

	
	return 0;
}
