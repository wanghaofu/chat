#ifndef INI_CHAT_C_H_
#define INI_CHAT_C_H_


#include <iostream> //流类
#include <map> //用来存放用户的容器类
#include <set> //用来存放用户的容器类
#include <string>
#include <sstream>
#include <fstream> //日志记录部分
#include <sys/socket.h>  //scoket类
#include <sys/epoll.h>  //处理大并发连接的epoll
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h> //进程类
#include <openssl/md5.h> //MD5类
#include <signal.h> //捕捉信号用的
#include <time.h>     //取得系统当前时间
#include <ctype.h>     //匹配类型
#include <netinet/tcp.h> //TCP_NODELAY
#include "IniFile.h"  //获取配置文件用

/*
--------------------------------------------------------------------------------------
	本文件信息
	公司：FNQ
	用途：聊天系统SOCKET服务端linux版
	语言:c++
	编译命令:g++ -o chat chat.c -lpthread -lcrypto -lm IniFile.cpp -O2 -Wall
	DEBUG编译:g++ -o chat chat.c -lpthread -lcrypto -lm IniFile.cpp -Wall -DDEBUG -g
	内存泄露:/usr/local/webserver/valgrind/bin/valgrind --tool=memcheck --leak-check=full ./chat -c config.ini
	开发人员:王伟
	修改日期:2010-03-29
--------------------------------------------------------------------------------------
*/

/*
--------------------------------------------------------------------------------------
	协议
	采用:字符串
--------------------------------------------------------------------------------------
*/

/*
--------------------------------------------------------------------------------------
	结构配置部分
	包含:错误提示宏,最大长度,最大连接数,最大未决处理量，版本号
--------------------------------------------------------------------------------------
*/

//提供详细的报错信息
#define MYPRINTF(format,args...)  printf("%s-%s-%d:" format "\n",__FILE__,__FUNCTION__,__LINE__,##args)
//求最小值的宏
//#define MIN(a,b) (a >= b) ? b : a;
//最大长度
#define MAXLINE 163840
//最大连接
#define OPEN_MAX 10240
//最大未决处理量，backlog指定了套接字可以有多少个未决的连接
#define LISTENQ 100
//返回值定义
#define EPOLL_FAILED	-1   //失败
//版本号
#define V "1.1.0.0"

//引入std
using namespace std;


//退出报告
#define EXIT_SUCCESS						0
#define EXIT_SIGNAL_ERROR					1



/*
--------------------------------------------------------------------------------------
	声明部分
	包含:用户信息表
--------------------------------------------------------------------------------------
*/

//struct 用户信息体
struct userInfo
{
	string  nickName;	//昵称
	unsigned int		userId;		//用户id
	unsigned int		userFd;			//对方的fd
	unsigned short int		isVip;		//是否是VIP
	unsigned short int		gender;		//性别
	unsigned short int		faction;	//门派
	unsigned short int		weaponClass; //武器系别
	unsigned short int		state;		//状态 1正常  0锁定
	unsigned short int		wantedState;		//通缉状态 0正常  1 锁定
	unsigned short int		pk_crime; //罪恶值
	short int		channelCnt; //加入的频道数目
	//unsigned int group; //队伍
	set<string> 	channelId; //所加入的频道列表
};


//map 用户组
map<string, userInfo> userList;

//通过用户FD找到用户昵称
map<int, string> fdList;

//用于生成CHANNEL ID和它下面所有连接的关联MAP  channelid->nickName;
typedef std::multimap<string,string> Channel;
Channel channelList;

//线程池任务队列结构体
struct task {
	int fd; //需要读写的文件描述符
	struct task *next; //下一个任务
};

//用于读写两个的两个方面传递参数的结构体
struct userData {
	int fd;
	int tofd;
	string sendData;
};

//配置文件信息存储

int		INI_PORT;				//监听端口

string	INI_LOGIN_KEY,			//登陆KEY
		INI_WORLD_KEY,			//世界消息KEY
		INI_SUPER_KEY,			//超级管理KEY
		INI_CHANNEL_LIST,		//需要发送人员变动的频道
		INI_LOG_FILE;			//日志记录文件

//分割符--|一级分割，:二级分割
string s_sp_1 = "|";
string s_sp_2 = ":";

string	starttime = "1111-11-11 11:11:11";
int		onlines = 0;	
/*
--------------------------------------------------------------------------------------
	网络传输配置
	采用:epoll
--------------------------------------------------------------------------------------
*/

//线程的任务函数
void * readtask(void *args);

void * writetask(void *args);

//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
struct epoll_event ev, events[20];

int epfd;

pthread_mutex_t mutex;

pthread_cond_t cond1;

struct task *readhead = NULL, *readtail = NULL, *writehead = NULL;

#endif
