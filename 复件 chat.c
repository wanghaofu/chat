#include <iostream> //流类
#include <map> //用来存放用户的容器类
#include <set> //用来存放用户的容器类
#include <string>
#include <sstream>
#include <fstream> //日志记录部分
#include <sys/socket.h>  //scoket类
#include <sys/epoll.h>  //处理大并发连接
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h> //进程类
#include <openssl/md5.h> //MD5类
#include <signal.h> //捕捉信号用的
#include <time.h>     //取得系统当前时间
#include "IniFile.h"  //获取配置文件用

//提供详细的报错信息
#define MYPRINTF(format,args...)    printf("%s-%s-%d:" format "\n",__FILE__,__FUNCTION__,__LINE__,##args)

//最大长度
#define MAXLINE 810
//最大连接
#define OPEN_MAX 10240
//最大未决处理量，backlog指定了套接字可以有多少个未决的连接
#define LISTENQ 10

#define V "2.5.0.0"
using namespace std;
/*
 ---------------------------------------------------
 公司：9维互动
 用途：聊天系统SOCKET服务端linux版
 编译命令：g++ -o chat chat.c -lpthread -lcrypto -lm IniFile.cpp -O2 -Wall
 开发人员：王伟
 完成状态：监听-->接受-->发送-->解析字符串-->加密key-->根据不同数据的分别处理-->配置文件
 使用的关联容器:multimap，用来处理频道等一键多值情况，更换了split方式。避免恶意构造数据导致崩溃
 ---------------------------------------------------
 */

/*
 协议：
 ---------------------------------------------------
 C++版本聊天系统

 请查看详细的参数文档
 ---------------------------------------------------
 */

//返回值定义
#define EPOLL_FAILED            -1   //失败
//服务器端负责绑定的几部分。
//例子UinfoS.erase(1);  删除UID为1的值   int oo=UinfoS[1];取得用户1的socketid

//map UinfoS 用户MAP结构 UID->SOCKETID
map<string, int> UinfoS;

//map Sinfo 用户MAP结构 SOCKETID->UID
map<int, string> Sinfo;

//map Kinfo 用户KEY MAP结构 fd->KEY   用来保存用户每次的key，来加密每次的消息
map<int, string> Kinfo;

//set Linfo 锁用户列表
set<string> Linfo;

//用于生成CHANNEL ID和它下面所有连接的关联MAP  channelid->fd;
typedef std::multimap<string,int> Channelmap;
Channelmap channelmap;

//用于生成CHANNEL ID和它下面所有连接的关联MAP  UID->CID
typedef std::multimap<string,string> UinfoC;
UinfoC uinfoc;

//登陆KEY等部分
string LOGIN_KEY, INI_WKEY, INI_CKEY, INI_MKEY, INI_OOXX, P_LIST, INI_POLICY,INI_LOG;

//线程池任务队列结构体

struct task {
	int fd; //需要读写的文件描述符
	struct task *next; //下一个任务
};

//用于读写两个的两个方面传递参数的结构体

struct user_data {
	int fd;
	int tofd;
	string senddata;
};

//线程的任务函数

void * readtask(void *args);

void * writetask(void *args);

//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件

struct epoll_event ev, events[20];

int epfd;

pthread_mutex_t mutex;

pthread_cond_t cond1;

struct task *readhead = NULL, *readtail = NULL, *writehead = NULL;

//设置为非阻塞状态函数
void setnonblocking(int sock) {
	int opts;
	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		perror("fcntl(sock,GETFL)");
		exit(1);
	}
	opts = opts | O_NONBLOCK;
	if (fcntl(sock, F_SETFL, opts) < 0) {
		perror("fcntl(sock,SETFL,opts)");
		exit(1);
	}
}

//one_delete_C(Cid2,fd);
//one_delete_U(Uid,Cid2);

//删除multimap Channelmap 里的数据
void one_delete_C(string Ccid, int Ffd) {
	Channelmap::iterator position = channelmap.lower_bound(Ccid);
	while (position != channelmap.upper_bound(Ccid)) {
		if (position->second == Ffd) {
			channelmap.erase(position);
			break;
		} else {
			++position;
		}
	}
}

//删除multimap UinfoC 里的数据
void one_delete_U(string tempUid, string tempCid) {
	//断掉的时候要遍历对方所有的频道。并一一退出
	UinfoC::iterator positionu = uinfoc.lower_bound(tempUid);
	while (positionu != uinfoc.upper_bound(tempUid)) {
		if (positionu->second == tempCid) {
			uinfoc.erase(positionu);
			break;
		} else {
			++positionu;
		}
	}
}

//删除multimap UinfoC 里的数据
void one_delete_U_All(string tempUid) {
	//断掉的时候要遍历对方所有的频道。并一一退出
	UinfoC::iterator positionu = uinfoc.lower_bound(tempUid);
	while (positionu != uinfoc.upper_bound(tempUid)) {
		if (positionu->first == tempUid) {
			uinfoc.erase(positionu);
			break;
		} else {
			++positionu;
		}
	}
}


//对用户的频道做检查，如果有需要通知的频道。需要通知到
void loop_call_U(string tempUid) {
	//断掉的时候要遍历对方所有的频道。并一一退出
	UinfoC::iterator positionu_i = uinfoc.lower_bound(tempUid);
	while (positionu_i != uinfoc.upper_bound(tempUid)) {

		//如果频道是需要通知的频道，通知频道里所有人，XX退出了
		int first = P_LIST.find_first_of(positionu_i->second.substr(0, 1));
		if (first!=-1){
				//通知所有人
				string PInfo = "aaa|L|" + tempUid+ "|" + positionu_i->second + "|" + "99";
				Channelmap::iterator position =channelmap.lower_bound(positionu_i->second);
				const char *mysend1 = PInfo.c_str();
				int mlen1 = strlen(mysend1) + 1;
				while (position!= channelmap.upper_bound(positionu_i->second)) {
					int s = position->second;
					send(s, mysend1, mlen1, MSG_NOSIGNAL);
					++position;
				}

			}
			++positionu_i;
	}
}

//用户断线后，清理所有信息。并退出
void clearinfo(int tmpfd) {
		
	if(Sinfo.find(tmpfd) != Sinfo.end()){
  	    string tempUid = Sinfo[tmpfd];
      
  	  //清理Sinfo
  	    Sinfo.erase(tmpfd);
  	  //清理Kinfo
  	  //Kinfo.erase(tmpfd);
      //清理UinfoS
  	    UinfoS.erase(tempUid);
  	  
  	  //断掉的时候要遍历对方所有的频道。并一一退出
  	  UinfoC::iterator positionu = uinfoc.lower_bound(tempUid);
  	  while (positionu != uinfoc.upper_bound(tempUid)) {
  	  	string s = positionu->second;
  	  	one_delete_C(s, tmpfd);
  	  	++positionu;
  	  }
  	  
  	  loop_call_U(tempUid);
  	  
  	  uinfoc.erase(tempUid);
    }
    Kinfo.erase(tmpfd);
  	close(tmpfd);
}

//获取当前时间函数
string getnowtime(string type) {
	time_t t = time(0);
	char tmp[64];
	if (type == "a") {
		strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&t));
	} else {
		strftime(tmp, sizeof(tmp), "%Y-%m-%d", localtime(&t));
	}
	string mytime(tmp);
	return mytime;
}

//求最小值函数，临时
int MIN(int a, int b) {
	return a >= b ? b : a;
}

//MD5加密函数

char *MD5String(const char *string) {
	int i;
	MD5_CTX context;
	unsigned char digest[16];
	char *result = (char *) malloc(33);

	MD5_Init(&context);
	MD5_Update(&context, string, strlen(string));
	MD5_Final(digest, &context);

	for (i = 0; i < 16; i++)
		sprintf(result + 2 * i, "%02x", digest[i]);
	result[32] = 0;
	return result;
}

char *MD5String2(const char *string) {
	int i;
	MD5_CTX context;
	unsigned char digest[16];
	char *result = (char *) malloc(33);

	MD5_Init(&context);
	MD5_Update(&context, string, strlen(string));
	MD5_Final(digest, &context);

	for (i = 0; i < 16; i++)
		sprintf(result + 2 * i, "%02x", digest[i]);
	result[20] = 0;
	return result;
}

//int 转 string函数
string getstring(const int n) {
	std::stringstream newstr;
	newstr << n;
	return newstr.str();
}

//返回一个随机数函数
int getrandstr() {
	int t = rand() % 100 + 1;
	return t;
}

//获得本次产生的日志名函数
string getlogname() {
	string dname("LOG/");
	string tname = getnowtime("b");
	string fname("_chat_debug.log");
	string name = dname + tname + fname;
	return name;
}

//用来记录日志的句柄
//string namefile = getlogname;

	
	


//主体函数
int main(int argc, char *argv[]) {
	int i, listenfd, connfd, sockfd, nfds, iRet,iRet2, socktofd, INI_PORT;
	pthread_t tid1;
    string cpath;
	if (argc==3) 
    { 
      //printf("参数1： %s", argv[2]); 
	  cpath=string(argv[2]);
    }else {
      printf("Usage: ./chat -c /aaa/bbb/config.ini\n");
	  return -1;
    }
    

	struct task *new_task = NULL;
	struct user_data *rdata = NULL;
	socklen_t clilen;
  
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE,&sa,0);

	//产生随机种子
	//srand((unsigned) time(NULL));

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond1, NULL);
	//初始化用于读线程池的线程

	pthread_create(&tid1, NULL, readtask, NULL);

	//生成用于处理accept的epoll专用的文件描述符   ，同时并发数
	epfd = epoll_create(OPEN_MAX);
	if (-1 == epfd) {
		MYPRINTF("create epoll failed");
		return EPOLL_FAILED;
	}
	struct sockaddr_in clientaddr;
	struct sockaddr_in serveraddr;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	//设置SO_REUSEADDR选项(服务器快速重起)
	int bReuseaddr=1;
	setsockopt(listenfd,SOL_SOCKET ,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr));

    //发送缓冲区
    int nSendBuf=1024*16;//设置为32K
    setsockopt(listenfd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));

	//把socket设置为非阻塞方式

	setnonblocking(listenfd);
	//设置与要处理的事件相关的文件描述符

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	printf("服务启动.............\n");
	IniFile ini(cpath);
	ini.setSection("config");

	INI_PORT = ini.readInt("PORT", -1);
	if(INI_PORT==-1){
	  printf("读取配置文件失败.............\n");
	  return -1;
	}

	printf("读取配置文件成功.............\n");
	//取得登陆key
	LOGIN_KEY = ini.readStr("LOGIN", "");

	INI_MKEY = ini.readStr("MKEY", "");
	//取得需要通知的频道列表
	P_LIST = ini.readStr("LIST", "");

	INI_OOXX=ini.readStr("OOXX", "");
    
	//LOG地址
	INI_LOG=ini.readStr("LOG", "");

	printf("读取策略文件文件成功.............\n");
	string INI_POLICY=ini.readStr("POLICY", "");

	//绑定端口，IP，进行监听
	//char *local_addr = "0.0.0.0";
	inet_aton("0.0.0.0", &(serveraddr.sin_addr));//htons(INI_PORT);
	serveraddr.sin_port = htons(INI_PORT);
	iRet = bind(listenfd, (sockaddr *) &serveraddr, sizeof(serveraddr));
	if (0 > iRet) {
		MYPRINTF("server bind failed,由于进程未完全退出，端口已被占用,请稍后重启");
		(void) close(listenfd);
		return EPOLL_FAILED;
	}

	iRet2=listen(listenfd, LISTENQ);
	if (0 > iRet2) {
		MYPRINTF("server bind failed,由于进程未完全退出，端口已被占用,请稍后重启");
		(void) close(listenfd);
		return EPOLL_FAILED;
	}
	
	ev.data.fd = listenfd;
	//设置要处理的事件类型

	ev.events = EPOLLIN;
	//注册epoll事件

	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
	
	printf("监听开始.............\n");
	printf("监听IP：0.0.0.0..监听端口：%d..\n", INI_PORT);

	const char *filename = INI_LOG.c_str();
    ofstream o_file(filename, ios_base::app);

	if (!o_file) {
		printf("日志打开失败,请确认是否存在LOG子目录\n");
		std::cout << "配置目录： >>" << INI_LOG << std::endl;
		return -1;
	} else {
		printf("日志打开成功....开始记录....\n");
	}
	printf("版本号：%s.....2009-02-24......\n", V);
  
   string starttime=getnowtime("a");
   std::cout << "服务启动于 >>" << starttime << std::endl;
  
	for (;;) {
		//等待epoll事件的发生
		nfds = epoll_wait(epfd, events, LISTENQ, -1);
		//处理所发生的所有事件
				for (i = 0; i < nfds; ++i) {
					if (events[i].data.fd == listenfd) {
						clilen = sizeof(clientaddr);
						connfd = accept(listenfd, (sockaddr *) &clientaddr, &clilen);
						if (connfd < 0) {
							printf("connfd<0");
							//exit(1);
						}else{
						//设置为非阻塞
						setnonblocking(connfd);
#ifdef DEBUG
						char *str = inet_ntoa(clientaddr.sin_addr);
						std::cout << "connec_ from >>" << str << std::endl;
#endif
            
						ev.data.fd = connfd;
						//设置用于注测的读操作事件

						ev.events = EPOLLIN | EPOLLET;
						//注册ev

						epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
					} else if (events[i].events & EPOLLIN) {
						
						//如果有传入事件
						//printf("reading!..id=%d...\n", events[i].data.fd);
						//printf("reading!..text=%s...\n",rdata->line);
						if ((sockfd = events[i].data.fd) < 0)
						continue;
						new_task = new task();
						new_task->fd = sockfd;
						new_task->next = NULL;
						//添加新的读任务

						pthread_mutex_unlock(&mutex);
						pthread_mutex_lock(&mutex);
						if (readhead == NULL) {
							readhead = new_task;
							readtail = new_task;
						} else {
							readtail->next = new_task;
							readtail = new_task;
						}
						//唤醒所有等待cond1条件的线程
						//usleep(3000);
						pthread_cond_broadcast(&cond1);
						pthread_mutex_unlock(&mutex);
						

                        
					} else if (events[i].events & EPOLLOUT) {
						//如果有需要传出事件
						
						rdata = (struct user_data *) events[i].data.ptr;
						sockfd = rdata->fd;
						socktofd = rdata->tofd;
						const char *mysend = rdata->senddata.c_str();
						if(rdata!=NULL){
						  delete rdata;
					  }
						int mlen = strlen(mysend) + 1;
#ifdef DEBUG
						MYPRINTF("发送内容--长度--SOCKETID号:%s--%d--%d..\n", mysend,mlen,socktofd);
#endif

						send(socktofd, mysend, mlen,MSG_NOSIGNAL);

						//设置用于读操作的文件描述符
						ev.data.fd = sockfd;
						//设置用于注测的读操作事件

						ev.events = EPOLLIN |EPOLLET;
						//修改sockfd上要处理的事件为EPOLIN

						epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
					}
				}
			}
		}
//任务读取部分
void *readtask (void *args) {

	int fd = -1;
	unsigned int n;
	string s_sp("|");
	string mysplit[8]; //设定一个分割后的数组，给后面使用
	char line[MAXLINE];
	char *founda;
    string Uid = "nonono";

	const char *filename = INI_LOG.c_str();
    ofstream o_file(filename, ios_base::app);

	//用于把读出来的数据传递出去
	struct user_data *data = NULL;
	while (1) {
		//usleep(10);
		pthread_mutex_lock(&mutex);
		//等待到任务队列不为空

		while (readhead == NULL)
		pthread_cond_wait(&cond1, &mutex);

		fd = readhead->fd;
		//从任务队列取出一个读任务

		struct task *tmp = readhead;
		readhead = readhead->next;
		if (tmp != NULL){
		delete tmp;
		}
		pthread_mutex_unlock(&mutex);
		data = new user_data();
		data->fd = fd;
		if ((n = read(fd, line, 799)) < 0) {
				clearinfo(fd);
				if (data != NULL){delete data;}
#ifdef DEBUG
				std::cout << "readline error" << std::endl;
#endif		
		} else if (n <= 0) {
			//增加一个清理函数
			clearinfo(fd);
#ifdef DEBUG
			MYPRINTF("Client close connect...!\n");
#endif
			if (data != NULL){delete data;}
		} else {
			if (n> 750 || n<24) {
               if(n == 23){
#ifdef DEBUG
            MYPRINTF("取策略文件的..连接%d..\n", fd);
#endif
          //连接上来，分配策略文件
          //string INI_POLICY="<cross-domain-policy><site-control permitted-cross-domain-policies=\"all\"/><allow-access-from domain=\"*\" to-ports=\"9999,6666\"/></cross-domain-policy>\0";
          const char *mysend = INI_POLICY.c_str();
          int mlen = strlen(mysend)+1;
          send(fd, mysend, mlen, MSG_NOSIGNAL);
        }else if(n == 2){
#ifdef DEBUG
              MYPRINTF("测试取得key部分..连接%d..\n", fd);
#endif
              
              //连接上来，分配密钥，密钥格式，md5(当前时间+随机数)
              string mykey = getnowtime("a") + "|G";
              const char *outkey = mykey.c_str();
              //加入Kinfo的MAP
              char *outkey2 = MD5String2(outkey);
              string keykey = string(outkey2) + "|G";
              const char *keykey2 = keykey.c_str();
              if(Kinfo.find(fd) != Kinfo.end()){
                Kinfo.erase(fd);
              }
              Kinfo[fd] = string(outkey2);
              free(outkey2);
              int outkey_len = strlen(keykey2) + 1;
              //返回给客户端
              send(fd, keykey2, outkey_len, MSG_NOSIGNAL);
              //data->senddata = keykey;
              //data->tofd = fd;
              //处理结束
              //ev.data.ptr = data;
              //设置用于注册的写操作事件
              //ev.events = EPOLLOUT | EPOLLET;
              //修改sockfd上要处理的事件为EPOLLOUT
              //epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
              if (data != NULL){delete data;}
          }else{
              if (data != NULL){delete data;}
              clearinfo(fd);
#ifdef DEBUG
				MYPRINTF("连接%d由于提供信息过长%d被踢掉了\n", fd,n);
#endif
         }
			} else {
				//设置需要传递出去的数据
#ifdef DEBUG
				MYPRINTF("获取内容---长度---SOCKETID号:%s---%d---%d..\n", line,n, fd);
#endif
				//获取|在传入的字符串的个数，为后面获取信息做准备
        
				founda = strtok(line, "|");
				int y = 0;
				while (founda != NULL) {
					if(y>4){break;}
					int p=y++;
					mysplit[p] = string(founda);
					founda = strtok(NULL, "|");
				}
				//free(founda);
				if (y < 3 || y > 6) { //如果少于3个，应该是恶意攻击，直接踢掉
					if (data != NULL){delete data;}
					clearinfo(fd);
#ifdef DEBUG
					MYPRINTF("连接%d由于提供信息根本不符合规定被踢\n", fd);
#endif
				} else {
					//获取后的信息保存在mysplit里
					string chat_key = mysplit[0]; //转化成 string 方便处理

					if (chat_key != Kinfo[fd] and Sinfo[fd]!="admin") {
						if (data != NULL){delete data;}
						clearinfo(fd);
#ifdef DEBUG
						MYPRINTF("连接%d由于提供的服务器私钥不正确被踢掉\n", fd);
#endif
					} else {
						//分配私钥，更新私钥信息
						string mykey = getnowtime("a");
						const char *outkey = mykey.c_str();
						char *outkey2 = MD5String2(outkey);
						//加入Kinfo的MAP
						string keykey = string(outkey2);
						Kinfo[fd] = keykey;
						free(outkey2);
						string chat_head = string(mysplit[1]); //转化成 string 方便处理
						if (chat_head == "J") {
							//J|1113|key  用户要求加入。绑定到所有的MAP里
							
							Uid=string(mysplit[2]);
							string UKEY = string(mysplit[3]);

							string sign = Uid + LOGIN_KEY + getnowtime("b");
							const char *sign_key = sign.c_str();

							char *outkey=MD5String(sign_key);
							string sign_check=string(outkey);

							free(outkey);
#ifdef DEBUG
							std::cout <<"我的加密前字符串为"<<sign<<"我的加密key为"<<sign_check<<"进来的key为"<<UKEY<<"\n" << std::endl;
#endif
              if(UKEY.length() != 32){UKEY=UKEY.substr(0,32);}
							if(sign_check != UKEY) {
								clearinfo(fd);
								if (data != NULL){delete data;}
#ifdef DEBUG
								MYPRINTF("连接%d由于提供的登陆密钥不正确被踢掉\n", fd);
#endif
							} else {
								//如果重复加入，警告

								if (UinfoS.find(Uid) != UinfoS.end()) {

                  if(UinfoS[Uid]!=fd){
									  //清理掉老用户的信息，并帮助用户重新加入
									  string relogin = keykey + "|J|" + Uid + "|99|A";
									  int oldfd=UinfoS[Uid];
									  const char *mysend = relogin.c_str();
									  int mlen = strlen(mysend) + 1;
									  send(oldfd, mysend, mlen, MSG_NOSIGNAL);
									  clearinfo(oldfd);
                  }
									UinfoS[Uid] = fd;
									//绑定Sinfo
									Sinfo[fd] = Uid;

									//返回一个绑定成功就可以了
									string login = keykey + "|J|" + Uid + "|88|A";
									data->senddata = login;
									data->tofd = fd;

								} else {
									
									//绑定UinfoS
									UinfoS[Uid] = fd;
									//绑定Sinfo
									Sinfo[fd] = Uid;

									//返回一个绑定成功就可以了
									string ss = keykey + "|J|" + Uid + "|88|A";
									data->senddata = ss;
									data->tofd = fd;
									//已完成
								 
								}
								//处理结束
								ev.data.ptr = data;
								//设置用于注册的写操作事件
								ev.events = EPOLLOUT | EPOLLET;
								//修改sockfd上要处理的事件为EPOLLOUT
								epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
							}
						 
						} else if (chat_head == "M") {

							if (y < 4) {
								clearinfo(fd);
								if(data!=NULL){delete data;}
#ifdef DEBUG
								MYPRINTF("连接%d由于提供信息根本不符合规定被踢\n", fd);
#endif		
							} else {

                if (Sinfo.find(fd) != Sinfo.end()) {
								//用户需求为私聊 M|111|INFO|KEY
								   string Uid = mysplit[2];
								   string ChatInfo = mysplit[3];
                   string Uid2 = Sinfo[fd];
								   ChatInfo = keykey + "|M|" + Uid2 + "|" + ChatInfo+"|A";
                   
                   
								   //是否有此人
								   int it = 0;
								   if (UinfoS.find(Uid) != UinfoS.end()) {
								   	it = UinfoS[Uid];
								   	data->senddata = ChatInfo;
								   	//printf("找到了..%d...\n",it);
								   } else {
								   	it = fd;
								   	data->senddata = keykey + "|M|" + Uid2+ "|88|A";
								   	//printf("没这个人..\n");
								   }
								   const char *mysend = ChatInfo.c_str();
								   int mlen = strlen(mysend)+1;
								   send(fd, mysend, mlen, MSG_NOSIGNAL);
                   
								   const char *mysendit = data->senddata.c_str();
								   int mlenit = strlen(mysendit)+1;
								   send(it, mysendit, mlenit, MSG_NOSIGNAL);
								   
                   }else{
#ifdef DEBUG       
                    MYPRINTF("连接%d由于未登陆发私聊消息被踢\n", fd);
#endif             
                   if (data != NULL){delete data;}
                   clearinfo(fd); 
                 }
							}
						} else if (chat_head == "X") {
              //心跳检测
							string Uid = Sinfo[fd];

							string ChatInfo = keykey + "|X|"  + Uid;
							const char *mysendP = ChatInfo.c_str();
							int mlenP = strlen(mysendP) + 1;
              send(fd, mysendP, mlenP, MSG_NOSIGNAL);
              if (data != NULL){delete data;}
					}else if (chat_head == "C") {
							if (y < 4) {
								if (data != NULL){delete data;}
								clearinfo(fd);
#ifdef DEBUG
								MYPRINTF("连接%d由于提供信息根本不符合规定被踢\n", fd);
#endif
							} else {
								//联盟：需要判定对方的连接是否在对应的map里。 C|20|INFO|KEY
								string Cid = mysplit[2];
								string ChatInfo = mysplit[3];
								//string Key=strtok(NULL,"|");
								//是否有此人
								if (Sinfo.find(fd) != Sinfo.end()) {

									//取出用户ID，开始发送
									string Uid = Sinfo[fd];
									//开始迭代
									ChatInfo = keykey + s_sp + "C|" + Uid + s_sp+ Cid + s_sp + ChatInfo;
									const char *mysend = ChatInfo.c_str();
									int mlen = strlen(mysend) + 1;
									if(Linfo.find(Uid) != Linfo.end()){
								  send(fd, mysend, mlen, MSG_NOSIGNAL);
								  }else{
#ifdef DEBUG
									int Acount = channelmap.count(Cid);
									std::cout << "刚才" << Uid << "对" << Cid << "公会的"<< Acount << "人进行了公会喊话，内容是:"<< ChatInfo << "\n" << std::endl;
#endif
									Channelmap::iterator position = channelmap.lower_bound(Cid);
									  while (position != channelmap.upper_bound(Cid)) {
										  int s = position->second;
										  send(s, mysend, mlen, MSG_NOSIGNAL);
										  ++position;
									  }
								  }
									if (data != NULL){delete data;}
								} else {
#ifdef DEBUG
									MYPRINTF("连接%d由于未登陆发频道消息被踢\n", fd);
#endif
                  if (data != NULL){delete data;}
									clearinfo(fd);
								}
							}
						} else if (chat_head == "W") {
							//世界消息

							string ChatInfo = mysplit[2];
							string mkey = mysplit[3];
							//string Key=strtok(NULL,"|");
							//是否有此人
							string sign = chat_key + ChatInfo + INI_MKEY;
							const char *sign_key = sign.c_str();
							char *outkey=MD5String(sign_key);
							string sign_check=string(outkey);

							free(outkey);
              if(sign_check!=mkey){
#ifdef DEBUG
              MYPRINTF("连接%d由于提供的发送世界消息KEY不对，被踢\n", fd);
							std::cout <<"我的加密前字符串为"<<sign<<"我的加密key为"<<sign_check<<"进来的key为"<<mkey<<"\n" << std::endl;
#endif
              if (data != NULL){delete data;}
              clearinfo(fd);
               }else{
							 if (Sinfo.find(fd) != Sinfo.end()) {
                string Uid = Sinfo[fd];
								//开启日志记录
								string FILE_H = "#";
								string File_T = getnowtime("a");//当前时间
								string File_U = Sinfo[fd]; //用户
								string FILE_D = ChatInfo; //发送的消息
								string File_I = File_T + FILE_H + File_U+ FILE_H + FILE_D;
								o_file << File_I << std::endl;

								//开始迭代
								ChatInfo = keykey + s_sp + "W|" + Uid + s_sp+ ChatInfo;

								const char *mysend = ChatInfo.c_str();
								int mlen = strlen(mysend) + 1;
								//如果是被封停用户。发送的东西只有自己能够看见
								if(Linfo.find(Uid) != Linfo.end()){
								  send(fd, mysend, mlen, MSG_NOSIGNAL);
								}else{
									std::map<int, string>::iterator iter;
#ifdef DEBUG
								  int Acount = Sinfo.size();
								  std::cout << "刚才" << Uid << "对" << Acount<< "人进行了世界喊话，内容是:" << ChatInfo << "\n"<< std::endl;
#endif            
								  for (iter = Sinfo.begin(); iter != Sinfo.end(); iter++) {
								  	int s = iter->first;
								  	//printf("发送内容---长度---SOCKETID号:%s---%d---%d..\n",mysend,strlen(mysend),s);
								  	send(s, mysend, mlen, MSG_NOSIGNAL);
								  }
									}
								if (data != NULL){delete data;}
							} else {
								if (data != NULL){delete data;}
								clearinfo(fd);
#ifdef DEBUG
								MYPRINTF("连接%d由于未登陆发世界消息被踢\n", fd);
#endif
							}
                           }
						} else if (chat_head == "XXOO") {
							//系统消息

							string ChatInfo = mysplit[2];
							string pwd = mysplit[3];
							if (pwd == INI_OOXX) {
								//是否有此人
								if (Sinfo.find(fd) != Sinfo.end()) {
									//开始迭代
                  string Uid = Sinfo[fd];
									ChatInfo = keykey + "|S|" + Uid +"|"+ ChatInfo;
									std::map<int, string>::iterator iter;
#ifdef DEBUG
									int Acount = Sinfo.size();
									std::cout << "刚才" << Uid << "对" << Acount<< "人进行了系统喊话，内容是:" << ChatInfo<< "\n" << std::endl;
#endif
									const char *mysend = ChatInfo.c_str();
									int mlen = strlen(mysend) + 1;
									for (iter = Sinfo.begin(); iter != Sinfo.end(); iter++) {
										int s = iter->first;
										//printf("发送内容---长度---SOCKETID号:%s---%d---%d..\n",mysend,strlen(mysend),s);
										send(s, mysend, mlen, MSG_NOSIGNAL);
									}
								} else {
#ifdef DEBUG
									MYPRINTF("没登记就来发言,不返回任何信息..\n");
#endif
								}
							} else {
#ifdef DEBUG
								MYPRINTF("密码错误也来发系统消息！\n");
#endif
                if (data != NULL){delete data;}
								clearinfo(fd);	
							}
						} else if (chat_head == "OOXX") {
							//系统消息

							string Uid = mysplit[2];
							string pwd = mysplit[3];
							string status = mysplit[4];
							if (pwd == INI_OOXX) {
								//是否有此人
								if (Sinfo.find(fd) != Sinfo.end()) {
									//开始迭代
								  if(status=="1"){
#ifdef DEBUG
									   MYPRINTF("封停用户成功..\n");
#endif               
									   Linfo.insert(Uid);
									   //下发删除消息
									   string Uidadmin = Sinfo[fd];
									   string ChatInfo = keykey + "|S|" + Uidadmin + "|D::" + Uid + "|A";
									   std::map<int, string>::iterator iter;
									   const char *mysend = ChatInfo.c_str();
									   int mlen = strlen(mysend) + 1;
									   for (iter = Sinfo.begin(); iter != Sinfo.end(); iter++) {
									   	int s = iter->first;
									   	send(s, mysend, mlen, MSG_NOSIGNAL);
									   }
									}else{
#ifdef DEBUG
									   MYPRINTF("解封用户成功..\n");
#endif
								     Linfo.erase(Uid);
									}
								} else {
#ifdef DEBUG
									MYPRINTF("没登记就来发言,不返回任何信息..\n");
#endif
								}
							} else {
#ifdef DEBUG
								MYPRINTF("密码错误也来发系统消息！\n");
#endif
                if (data != NULL){delete data;}
								clearinfo(fd);	
							}
						}else if (chat_head == "P") {
							if (y < 4) {
#ifdef DEBUG
								MYPRINTF("连接%d由于提供信息根本不符合规定被踢\n", fd);
#endif
                if (data != NULL){delete data;}
								clearinfo(fd);
							} else {
								//加入一个频道
								string Cid1 = mysplit[2];
								string Cid2 = mysplit[3];

								if (Cid1 == Cid2) {
#ifdef DEBUG
									MYPRINTF("玩我那吧，我可不管\n");
#endif
								} else {
						if (Sinfo.find(fd) != Sinfo.end()) {
                          string Uid = Sinfo[fd];
                          //加入一个频道
                          if (Cid1 != "X") {
                          //如果一旦发生重复，可能要考虑先清除下目前的绑定状态。
                          //one_delete_C(Cid1,fd);
                          //one_delete_U(Uid,Cid1);
                          channelmap.insert(make_pair(Cid1, fd));
                          uinfoc.insert(make_pair(Uid, Cid1));
                          int first = P_LIST.find_first_of(Cid1.substr(0,1));
                          if (first!=-1) {
                          	//获取列表并通知所有人
                          	string PInfo = keykey + "|L|" + Uid+ "|" + Cid1 + "|" + "88|A";
                          
                          	Channelmap::iterator
                          	position =channelmap.lower_bound(Cid1);
                          	const char *mysend2 = PInfo.c_str();
                          	int mlen2 = strlen(mysend2) + 1;
                          	string info = "";
                          	while (position!= channelmap.upper_bound(Cid1)) {
                          		int s = position->second;
                          		if (s != fd)
                          			{
                          		     send(s, mysend2, mlen2, MSG_NOSIGNAL);
                          		     info += string(Sinfo[s]) + ",";
                                 }
                                     ++position;
                          	}
                          	string info2 = keykey + "|L|" + Uid + "|"+ Cid1 + "|" + info;
                          	const char *mysend3 = info2.c_str();
                          	int mlen3 = strlen(mysend3) + 1;
                          	send(fd, mysend3, mlen3, MSG_NOSIGNAL);
                          }
                      }


                                    
										//退出一个频道
                                        
										if (Cid2 != "X") {
											//解除该频道。需要做的。删除提交上来的频道
											one_delete_C(Cid2, fd);
											one_delete_U(Uid, Cid2);
											int first = P_LIST.find_first_of(Cid2.substr(0,1));
											if (first!=-1) {
												//通知所有人
												string PInfo = keykey + "|L|" + Uid+ "|" + Cid2 + "|" + "99|A";
												Channelmap::iterator position =channelmap.lower_bound(Cid2);
												const char *mysend1 = PInfo.c_str();
												int mlen1 = strlen(mysend1) + 1;
												while (position!= channelmap.upper_bound(Cid2)) {
													int s = position->second;
													//write(s, mysend , mlen);
													if (s != fd)
														{
													    send(s, mysend1, mlen1, MSG_NOSIGNAL);
												    }
                                                ++position;
												}

											 }
										}
										
										string PLinfo=keykey + "|P|" + Uid+ "|" + Cid1 + "|" + Cid2+ "|88|A";
										const char *mysendP = PLinfo.c_str();
										int mlenP = strlen(mysendP) + 1;
                    send(fd, mysendP, mlenP, MSG_NOSIGNAL);
                    if (data != NULL){delete data;}
										//返回一个标签，并改变状态
										//data->tofd = fd;
										//data->senddata = keykey + "|P|" + Uid+ "|" + Cid1 + "|" + Cid2+ "|88|0";
										//ev.data.ptr = data;
										//ev.events = EPOLLOUT | EPOLLET;
										//epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);

									} else {
										//printf("无此人");
#ifdef DEBUG
										MYPRINTF("连接%d由于未登陆发送频道切换信息被踢\n", fd);
#endif
                                        if (data != NULL){delete data;}
										clearinfo(fd);	
									}
								}
							}
						} else if (chat_head == "U") {

							//解散一个频道
							string Cid = string(mysplit[2]);
							//判断是否加入过
							if (Sinfo.find(fd) != Sinfo.end()) {

								//发消息给该频道所有人，并依次踢出频道
								//取出用户ID，开始发送
                string Uid = Sinfo[fd];
								//开始迭代
								string s_head("U|");
								string ChatInfo = keykey + "|" + s_head + Uid+ "|X|" + Cid + "|88";
#ifdef DEBUG
								int Acount = channelmap.count(Cid);
								std::cout << "刚才" << Uid << "对" << Cid << "公会的"<< Acount << "人进行了解除公会\n" << std::endl;
#endif
								Channelmap::iterator position = channelmap.lower_bound(Cid);
								const char *mysend = ChatInfo.c_str();
								int mlen = strlen(mysend) + 1;
								//给每一个频道内的人发消息，告知被谁解散，并删除每一个在频道内的人。
								while (position != channelmap.upper_bound(Cid)) {
									int s = position->second;
									string Uid_to = Sinfo[s];
									//告知每一个人，谁解散了什么频道
									send(s, mysend, mlen, MSG_NOSIGNAL);
									//清空每个人的此频道列表
									one_delete_U(Uid_to, Cid);
									++position;
								}
								//清空此频道
								channelmap.erase(Cid);
								if (data != NULL){delete data;}
							}else{
								if (data != NULL){delete data;}
                clearinfo(fd);
#ifdef DEBUG
                MYPRINTF("连接%d未登录发频道解散信息被踢\n", fd);
#endif

              }
						} else {
							//用户需求为特殊
							if (y < 5) {
								if (data != NULL){delete data;}
								clearinfo(fd);
#ifdef DEBUG
								MYPRINTF("连接%d由于提供信息根本不符合规定被踢\n", fd);
#endif
							} else {


								string Uid = string(mysplit[2]);
								string ChatInfo = string(mysplit[3]);
								string UKEY = string(mysplit[4]);
								string sign = Uid + ChatInfo + INI_MKEY;
								const char *sign_key = sign.c_str();
								//string sign_check = MD5String(sign_key);

								char *outkey=MD5String(sign_key);
								string sign_check=string(outkey);

								free(outkey);
#ifdef DEBUG
								std::cout <<"我的加密前字符串为"<<sign<<"我的加密key为"<<sign_check<<"进来的key为"<<UKEY<<"\n" << std::endl;
#endif
								if (sign_check != UKEY) {
									if (data != NULL){delete data;}
									clearinfo(fd);
#ifdef DEBUG
									MYPRINTF("连接%d由于提供的其他密钥不正确被踢掉\n", fd);
#endif
								} else {
                  if (Sinfo.find(fd) != Sinfo.end()) {
                                    string Uid2 = Sinfo[fd];
									ChatInfo = keykey + "|" + chat_head + "|" + Uid2+ "|" + ChatInfo;
									//是否有此人
									int it = 0;
									if (UinfoS.find(Uid) != UinfoS.end()) {
										it = UinfoS[Uid];
										data->senddata = ChatInfo;
										//printf("找到了..%d...\n",it);
										string myinfo=keykey + "|N|" + Uid2 + "|88";
										const char *mysend = myinfo.c_str();
										int mlen = strlen(mysend) + 1;
										send(fd, mysend, mlen, MSG_NOSIGNAL);
									} else {
										it = fd;
										data->senddata = keykey + "|N|" + Uid2 + "|99";
										//printf("没这个人..\n");
									}
										const char *mysendP = data->senddata.c_str();
										int mlenP = strlen(mysendP) + 1;
                                        send(it, mysendP, mlenP, MSG_NOSIGNAL);
                                        if (data != NULL){delete data;}
                    }else{
                    	if (data != NULL){delete data;}
                        clearinfo(fd);
#ifdef DEBUG
                      MYPRINTF("连接%d未登录发游戏信息被踢\n", fd);
#endif

                   }

								}
							}
						}
					}
				}
			}
		}
	}
}
