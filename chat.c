#include "chat.h"

/*
--------------------------------------------------------------------------------------
    函数部分
--------------------------------------------------------------------------------------
*/


/* Process daemonize */
int daemonize()
{
    int fd;

    // Fork a new process
    switch(fork())
    {
        case -1 :
            return -1;
        case 0 :
            break;
        default :
            exit(1);
    }

    if (setsid() == -1)
    {
        return -1;
    }

    // Redirect standard IO to null device
    fd = open("/dev/null", O_RDWR, 0);
    if (fd)
    {
        (void) dup2(fd, STDIN_FILENO);
        (void) dup2(fd, STDOUT_FILENO);
        (void) dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO)
        {
            (void) close(fd);
        }
    }

    return 0;
}


/* Application terminate signal handler */
static void sig_handler(const int sig)
{
    //map<int, string>::iterator it;
    //for(it = fdList.begin(); it!=fdList.end(); it++){
    //  close(it->first);
    //  cout<<it->first<<"------"<<it->second <<endl;
    //}
    //std::set<int>::iterator clientTmp;
    //for(clientTmp = clientList.begin(); clientTmp!=clientList.end(); clientTmp++){
    //  int id = *clientTmp;
    //  MYPRINTF("连接%d被关闭\n", id);
    //  shutdown(id,2);
    //  close(id);
    //}
    fprintf(stderr, "\nSIGINT handled, server terminated\n");
    exit(EXIT_SUCCESS);

    return;
}

/* Set signal handler */
void set_sig()
{
    struct sigaction *sa = (struct sigaction *) malloc(sizeof(struct sigaction));

    // SIGINT
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGKILL, sig_handler);

    // Ignore SIGPIPE & SIGCLD
    sa->sa_handler = SIG_IGN;
    sa->sa_flags = 0;
    signal(SIGPIPE, SIG_IGN);
    if (sigemptyset(&sa->sa_mask) == -1 || sigaction(SIGPIPE, sa, 0) == -1)
    {
        fprintf(stderr, "Ignore SIGPIPE failed\n");
        exit(EXIT_SIGNAL_ERROR);
    }

    return;
}


//设置为非阻塞状态函数
void setnonblocking(int sock) 
{
    int opts;
    opts = fcntl(sock, F_GETFL);
    if (opts < 0) 
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0) 
    {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}



//获取当前时间函数
string getnowtime(string type) 
{
    time_t t = time(0);
    char tmp[64];
    if (type == "a") 
    {
        strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&t));
    } 
    else 
    {
        strftime(tmp, sizeof(tmp), "%Y-%m-%d", localtime(&t));
    }
    string mytime(tmp);
    return mytime;
}

//MD5加密函数 取32位
char *MD5String(const char *string) 
{
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


//int 转 string函数
string int2string(const int n) 
{
    std::stringstream newstr;
    newstr << n;
    return newstr.str();
}


void sendData(int tmpId, string reMsg)
{
    int         reMsgLen;
    const char *reMsgChar;
    reMsgChar = reMsg.c_str();
    reMsgLen = strlen(reMsgChar) + 1;
    //发送
    send(tmpId, reMsgChar, reMsgLen, MSG_NOSIGNAL);
}


void sendDataChannel(string channelId, string reMsg)
{
    Channel::iterator position = channelList.lower_bound(channelId);
    while (position != channelList.upper_bound(channelId)) {
        if(userList.count(position->second) != 0){
            sendData(userList[position->second].userFd, reMsg);
        }
        ++position;
    }

}


void sendDataGroup(string groupId, string reMsg, int noId)
{
    Channel::iterator position = channelList.lower_bound(groupId);
    while (position != channelList.upper_bound(groupId)) {
        if(userList.count(position->second) != 0){
            int tmpId = userList[position->second].userFd;

            if(tmpId != noId){
                sendData(tmpId, reMsg);
                cout<<"发广播给："<<position->second<<endl;
            }else{
                cout<<"没广播给："<<position->second<<endl;
            }
        }
        ++position;
    }

}

//退出某频道时候，删除multimap channelList 里的数据
void quitChannelList(string channelId, string nickName) {
    if(userList.count(nickName) != 0){
        //删除channelList
        Channel::iterator position = channelList.lower_bound(channelId);
        while (position != channelList.upper_bound(channelId)) {
            //std::cout << nickName << "和 >>" << position->second << "开始匹配在频道：" << channelId << std::endl;
            if (position->second == nickName) {
                channelList.erase(position);
                //std::cout << nickName << "被匹配并开始退出 >>" << channelId << std::endl;
                break;
            } else {
                //std::cout << nickName << "没有被匹配，继续查找在频道 >>" << channelId << std::endl;
                ++position;
            }
        }
    }
}

//退出队伍
void quitGroup(string groupId, string nickName){
    std::cout << nickName << "开始退出队伍 >>" << groupId << std::endl;
    if(userList.count(nickName) != 0){
        userList[nickName].channelId.erase(groupId);
        //userList[nickName].group =0;
        //quitChannelList(groupId,nickName);
        //std::cout << nickName << "成功退出队伍 >>" << groupId << std::endl;
    }
}

void clearInfo(int tmpfd,int type)
{
    //MYPRINTF("连接%d，因为原因%d开始退出\n", tmpfd,type);
    //先关闭连接放心点
    close(tmpfd);
    if(fdList.count(tmpfd) != 0){

        //退出后的信息清理工作。。55555，好麻烦
        string nickName = fdList[tmpfd];

        cout<<"连接:"<<tmpfd<<"用户:"<<nickName<<"原因:"<<type<<"退出"<<endl;

        //退出fdList
        fdList.erase(tmpfd);

        std::set<string>::iterator channelTmp;
        //遍历channelTmp,如果是需要通知，就通知下

        for (channelTmp = userList[nickName].channelId.begin(); channelTmp != userList[nickName].channelId.end(); channelTmp++) {
            //std::cout << nickName << "开始退出频道ID+++++++" << std::endl;
            string id = *channelTmp + "";

            int first = INI_CHANNEL_LIST.find_first_of(id.substr(0, 1));
            if(first != -1)
            {
                string reMsg = "1005" + s_sp_1 + nickName + s_sp_2 + int2string(userList[nickName].userId) + s_sp_1 + id;

                //对频道内广播
                sendDataChannel(id,reMsg);  
            }
            quitChannelList(id,nickName);
        }
        //退出userList
        if(userList.count(nickName) != 0){
            userList.erase(nickName);
        }
        if(userList.count(nickName) == 0 && fdList.count(tmpfd) == 0){
            cout<<nickName<<"退出成功"<<endl;
        }else{
            cout<<nickName<<"退出失败"<<endl;
        }
    }
}

bool checkKey(string nickName, string loginKey)
{

    string loginCheck = nickName + INI_LOGIN_KEY + getnowtime("b");
    std::cout<<"进入消息"<<loginCheck << "名字： >>" << nickName <<"KEY：" << loginKey << std::endl;
    const char *loginCheckKey = loginCheck.c_str();
    char *outKey = MD5String(loginCheckKey);
    string outLoginCheck = string(outKey);
    free(outKey);

    if(outLoginCheck != loginKey)
    {
        return false;
    }else{
        return true;
    }
}

//检查系统KEY
bool checkSystem(string superKey)
{
    std::cout << " key1: >>" << INI_SUPER_KEY <<"KEY2：" << superKey << std::endl;
    if(INI_SUPER_KEY != superKey){
        return false;
    }else{
        return true;
    }
}

bool checkLogin(int fd)
{
    if(fdList.count(fd) != 0)
    {

        string nickName = fdList[fd];

        if(userList[nickName].state == 1)
        {
            return false;
        }else{
            return true;
        }
    }
    else
    {
        string reMsg = string("2001");
        sendData(fd, reMsg);
        return false;
    }
}



static void show_help(void)
{
    char *b = "------------------------------------------------------------\n"
          "服务端版本 v" V "\n\n"
          "\n"
           "-c   加载配置文件\n"
           "-d   是否使用守护进程启动\n"
           "-h   打开帮助\n"
           "样例：./chat -c config/8888.ini -d\n"
           "------------------------------------------------------------\n";
    fprintf(stderr, b, strlen(b));
}

/*
--------------------------------------------------------------------------------------
    主体函数
--------------------------------------------------------------------------------------
*/

int main(int argc, char *argv[]) 
{
    int i, listenfd, connfd, sockfd, nfds, iRetBind, iRetListen, socktofd, c;
    pthread_t tid1;
    string  configPath;
    bool daemon = false;

    if (argc < 2){
        show_help();
        return 1;
    } 
    while ((c = getopt(argc, argv, "c:dh")) != -1) {
        switch (c) {
        case 'c':
            configPath = string(argv[2]);
            break;
        case 'd':
            daemon = true;
            break;
        case 'h':
            show_help();
            break;
        default:
            show_help();
            return 1;
        }
    }

    // Ready for daemonize ?
    if (daemon == true)
    {
        daemonize();
    }
    else
    {
        fprintf(stderr, "\n* AoXian Game Server %s *\n====== Powered by YOYANGS    ======\n", V);
    }
    
    //设置结构体为空
    struct task *new_task = NULL;
    struct userData *rdata = NULL;
    socklen_t clilen;

    // Set signations
    set_sig();



    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond1, NULL);

    //初始化用于读线程池的线程--1个
    pthread_create(&tid1, NULL, readtask, NULL);

    //生成用于处理accept的epoll专用的文件描述符   ，同时并发数
    epfd = epoll_create(OPEN_MAX);
    if (-1 == epfd) 
    {
        MYPRINTF("创建 epoll 失败");
        return EPOLL_FAILED;
    }

    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    //设置SO_REUSEADDR选项(服务器快速重起)
    int bReuseaddr = 1;
    setsockopt(listenfd,SOL_SOCKET ,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(bReuseaddr));

    //超时设置
    //struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
    //setsockopt(listenfd, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(struct timeval));

    //发送缓冲区
    int nSendBuf = MAXLINE*20;//设置为4K
    setsockopt(listenfd,SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(nSendBuf));

    //接收缓冲区
    int nRevBuf = MAXLINE*20;//设置为4K
    setsockopt(listenfd, SOL_SOCKET, SO_RCVBUF, (const char*)&nRevBuf, sizeof(nRevBuf));

    //底层不拼包
    //int tcp_nodelay = 1;
    //setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&tcp_nodelay, sizeof(tcp_nodelay));

    //linger m_sLinger;

    //m_sLinger.l_onoff = 1; // (在closesocket()调用,但是还有数据没发送完毕的时候容许逗留)

    //m_sLinger.l_linger = 3; // (容许逗留的时间为0秒)

    //setsockopt(listenfd,SOL_SOCKET,SO_LINGER,(const char*)&m_sLinger,sizeof(linger));

    //把socket设置为非阻塞方式
    setnonblocking(listenfd);

    //设置与要处理的事件相关的文件描述符
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    printf("服务启动.............\n");

    //开始读取配置文件
    IniFile ini(configPath);
    ini.setSection("config");

    INI_PORT = ini.readInt("PORT", -1);

    if(INI_PORT==-1){
      printf("读取配置文件失败%d.............\n",INI_PORT);
      return -1;
    }

    //取得登陆key
    INI_LOGIN_KEY = ini.readStr("LOGIN_KEY", "");

    //超级用户KEY
    INI_SUPER_KEY=ini.readStr("SUPER_KEY", "");

    //需要通知的列表
    INI_CHANNEL_LIST = ini.readStr("CHANNEL_LIST", "");
    
    //LOG地址
    INI_LOG_FILE=ini.readStr("LOG_FILE", "");

    printf("读取配置文件成功.............\n");


    //绑定端口，IP，进行监听
    //char *local_addr = "0.0.0.0";
    inet_aton("0.0.0.0", &(serveraddr.sin_addr));//htons(INI_PORT);
    serveraddr.sin_port = htons(INI_PORT);
    iRetBind = bind(listenfd, (sockaddr *) &serveraddr, sizeof(serveraddr));
    if (0 > iRetBind) {
        MYPRINTF("server bind failed,由于进程未完全退出，端口已被占用,请稍后重启");
        (void) close(listenfd);
        return EPOLL_FAILED;
    }

    iRetListen=listen(listenfd, LISTENQ);
    if (0 > iRetListen) {
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
/*
    const char *filename = INI_LOG_FILE.c_str();
    ofstream o_file(filename, ios_base::app);

    if (!o_file) 
    {
        printf("日志打开失败,请确认是否存在LOG子目录\n");
        std::cout << "配置目录： >>" << INI_LOG_FILE << std::endl;
        return -1;
    } 
    else 
    {
        printf("日志打开成功....开始记录....\n");
    }
*/
    //显示版本号
    printf("版本号：%s.....2009-10-27......\n", V);
  
    starttime=getnowtime("a");
    std::cout << "服务启动于 >>" << starttime << std::endl;
  
    for (;;) {
        //等待epoll事件的发生
        nfds = epoll_wait(epfd, events, LISTENQ, -1);
        //处理所发生的所有事件
        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listenfd) {
                clilen = sizeof(clientaddr);
                connfd = accept(listenfd, (sockaddr *) &clientaddr, &clilen);
                if (connfd < 0) 
                {
                    printf("connfd<0");
                    //exit(1);
                }
                else
                {
                    //设置为非阻塞
                    setnonblocking(connfd);
                    char *str = inet_ntoa(clientaddr.sin_addr);
                    std::cout << "connec_ from >>" << str << std::endl;

                    ev.data.fd = connfd;
                    //设置用于注测的读操作事件

                    ev.events = EPOLLIN | EPOLLET;
                    //注册ev

                    epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
                }
            } 
            else if (events[i].events & EPOLLIN) 
            {

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
                } 
                else 
                {
                    readtail->next = new_task;
                    readtail = new_task;
                }

                //delete new_task;
                //唤醒所有等待cond1条件的线程
                //usleep(3000);
                pthread_cond_broadcast(&cond1);
                pthread_mutex_unlock(&mutex);
     
            } 
            else if (events[i].events & EPOLLOUT) 
            {
                //如果有需要传出事件
                rdata = (struct userData *) events[i].data.ptr;
                sockfd = rdata->fd;
                socktofd = rdata->tofd;
                const char *mysend = rdata->sendData.c_str();
                //删除无用数据
                delete rdata;

                int mlen = strlen(mysend) + 1;

                MYPRINTF("发送内容--长度--SOCKETID号:%s--%d--%d..\n", mysend,mlen,socktofd);
                send(socktofd, mysend, mlen,MSG_NOSIGNAL);

                delete mysend;

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


/*
--------------------------------------------------------------------------------------
    任务读取函数
--------------------------------------------------------------------------------------
*/
char read_buf[MAXLINE];
int read_cnt = 0;

//任务读取部分
void *readtask (void *args) 
{
    /*  声明部分    */

    //基础设置
    int fd = -1;        //fd
    int n;      //长度
    short int first;            //保存是否是需要通知的频道
    char line[MAXLINE]; //信息保存
    string mysplit[15]; //设定一个分割后的数组，给后面使用

    //用于把读出来的数据传递出去
    //struct userData *data = NULL;

    //const char *filename = INI_LOG_FILE.c_str();
    //ofstream o_file(filename, ios_base::app);

    while (1) 
    {
        //锁定
        pthread_mutex_lock(&mutex);

        //循环
        while (readhead == NULL)
        pthread_cond_wait(&cond1, &mutex);

        //获得fd
        fd = readhead->fd;

        //任务
        readhead = readhead->next;

        //解锁
        pthread_mutex_unlock(&mutex);
        //data = new userData();
        //data->fd = fd;

        string reMsg,reMsgMe,channelId,channelUserList;
        //协议
        int tmpId = -1; //临时ID


        std::map<int, string>::iterator iter;

        //接受数据



        //开始判定数据，
        n = read(fd, line, MAXLINE);
        if (n >= 0 && n < 5) {
            clearInfo(fd,1);
            MYPRINTF("数据过短或用户退出...!\n");
        }else if(n < 0)
        {
            if (errno != 11)
            {
                clearInfo(fd,2);
                cout << "系统错误(" << errno << "):" << strerror(errno) << endl;
            }
        }
        else
        {
            // Flash security request
            if (0 == strncmp("<policy-file-request/>", line, 22))
            {
                reMsg = string("<cross-domain-policy><site-control permitted-cross-domain-policies=\"all\"/><allow-access-from domain=\"*\" to-ports=\"*\"/></cross-domain-policy>");
                sendData(fd, reMsg);
                MYPRINTF("发送策略文件完毕...!\n");
            }
            else
            {
                if (read_cnt > 0)
                {
//                  cout << "have buff:" << read_buf << " cout :" << read_cnt << endl;
                    char tmp[MAXLINE];
                    strncpy(tmp, read_buf, read_cnt);

                    // 连接最后的
                    for (int i = read_cnt; i < n + read_cnt; i++)
                    {
                        tmp[i] = line[i - read_cnt];
                    }
//                  strncat(tmp, line, n);
                    n += read_cnt;
//                  MYPRINTF("长度...%d\n",strlen(tmp));
//                  MYPRINTF("测试最后一个字符...%c\n",tmp[n-1]);
                    strncpy(line, tmp, n);
//                  MYPRINTF("测试最后一个字符...%c\n",line[n-1]);

                    read_cnt = 0;
//                  MYPRINTF("读取合并后数据成功...长度.%d!\n",n);
                }
                line[n] = 0;
                //cout<<"连接socket:"<<fd<<"内容:"<<line<<"读取数据成功"<<endl;

                char *p = line;
                char *p2 = line;
                int last = 0;
                //按照\0拆分
                //int scount = sizeof(line)/sizeof(char);

                // 找到line里有0的位置
                while((p2 = strchr(p, 0))){
                    int plen = p2 - line + 1;   


                    if (plen > n)
                    {
                        if (last == 0)
                        {
                            last = n;
                        }
                        read_cnt = last;
                        strncpy(read_buf, p, last);
//                      cout << "save buff:" << p << "last:" << last << endl;
                        break;
                    }
                    last = n - plen;

                    cout << "result " << p << " len " << plen << endl; 

                    string  logString = string(p);


                    //解析后入数组
                    char    *tmp = strtok(p, "|");
                    unsigned short tmpCount = 0;

                    while (tmp != NULL) 
                    {
                        mysplit[tmpCount] = string(tmp);
                        tmp = strtok(NULL, "|");
                        if(tmpCount >13){
                            clearInfo(fd,3);
                            break;
                        }
                        tmpCount++;
                    }

                    //delete tmp;
                    //如果少于1个，应该是恶意攻击，直接踢掉
                    if (tmpCount < 1 || tmpCount > 13 || mysplit[0].c_str() == NULL || mysplit[1].c_str() == NULL) 
                    { 
                        clearInfo(fd,3);
                        MYPRINTF("连接%d由于提供信息根本不符合%d规定被踢\n", fd, tmpCount);
                    } 
                    else 
                    {
                        /*解析协议头*/
                        unsigned short opCode = atoi(mysplit[0].c_str());

                        switch(opCode)
                        {
                            //根据协议头，开始处理
                            case 1:
                                //心跳包
                                //nickName = string(mysplit[1]);
                                //user = userList[nickName];
                                //sendData(fd, user.nickName);
                                //reMsg = int2string(userList[nickName].userId);
                                //std::cout << "welcome >>" << userList[nickName].userId << std::endl;
                                MYPRINTF("测试~~~~\n");
                            break;
                            case 101:

                                if(mysplit[1].c_str() == NULL || mysplit[2].c_str() == NULL || mysplit[3].c_str() == NULL || mysplit[4].c_str() == NULL || mysplit[5].c_str() == NULL || mysplit[6].c_str() == NULL || mysplit[7].c_str() == NULL || mysplit[8].c_str() == NULL || mysplit[9].c_str() == NULL || mysplit[10].c_str() == NULL || mysplit[11].c_str() == NULL){
                                    MYPRINTF("数据不合格~~~~\n");
                                    clearInfo(fd,5);
                                }else{
                                    //加入
                                    string nickName = string(mysplit[1]);
                                    //对登陆KEY做校验
                                    if(checkKey(nickName,mysplit[11]))
                                    {
                                        if(userList.count(nickName) != 0)
                                        {
                                            //MYPRINTF("我查到啦1~~~~\n");
                                            int oldFd = userList[nickName].userFd;
                                            reMsg = "2006";
                                            sendData(oldFd, reMsg);
                                            clearInfo(oldFd,4);
                                            if(userList.count(nickName) != 0){
                                                    userList.erase(nickName);
                                            }
                                        }

                                        //如果已经有加入过，通知原来的人                                
                                        if(userList.count(nickName) == 0){
                                            struct userInfo user;
                                            //MYPRINTF("我没查到啦~~~~\n");

                                            user.userFd = fd;
                                            user.nickName = nickName;

                                            user.userId   = atoi(mysplit[2].c_str());
                                            user.isVip = atoi(mysplit[3].c_str());
                                            user.gender = atoi(mysplit[4].c_str());
                                            user.faction = atoi(mysplit[5].c_str());
                                            user.weaponClass = atoi(mysplit[6].c_str());
                                            user.state = atoi(mysplit[7].c_str());
                                            user.wantedState = atoi(mysplit[8].c_str());
                                            user.pk_crime = atoi(mysplit[9].c_str());
                                            //user.group = atoi(mysplit[10].c_str());
                                            user.channelCnt = 0;

                                            pair<map<string, userInfo>::iterator, bool> Insert_Pair;

                                            //userList[nickName] = user;
                                            Insert_Pair = userList.insert(pair<string, userInfo>(nickName,user));

                                            if(atoi(mysplit[10].c_str()) != 0){
                                                //加入的时候
                                                cout<<nickName<<"在初始时候加入队伍"<<mysplit[10]<<endl;

                                                userList[nickName].channelId.insert(mysplit[10]);
                                                userList[nickName].channelCnt += 1;
                                                channelList.insert(make_pair(mysplit[10], nickName));
                                            }

                                            if(Insert_Pair.second == true)
                                           {
                                                cout<<nickName<<"加入成功"<<endl;
                                                fdList[fd] = nickName;
                                                reMsg = string("1001");
                                                sendData(fd, reMsg);
                                           }else{
                                               if(userList.count(nickName) != 0){
                                                    userList.erase(nickName);
                                                }
                                                cout<<nickName<<"加入失败"<<endl;
                                           }
                                        }else{
                                            //MYPRINTF("我查到啦2~~~~\n");
                                            int oldFd = userList[nickName].userFd;
                                            reMsg = "2006";
                                            sendData(oldFd, reMsg);
                                            clearInfo(oldFd,4);
                                            if(userList.count(nickName) != 0){
                                                userList.erase(nickName);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        clearInfo(fd,5);
                                        MYPRINTF("连接%d由于登陆KEY错误被踢\n", fd);
                                    }
                                }
                            break;
                            case 102:
                                //发送世界消息
                                if(checkLogin(fd))
                                {
                                        string nickName = fdList[fd];
                                        if(userList.count(nickName) != 0){
                                            reMsg = "1002" + s_sp_1 + nickName + s_sp_2 + int2string(userList[nickName].userId) + s_sp_1 + mysplit[1];
                                            //遍历userList的MAP
                                            for (iter = fdList.begin(); iter != fdList.end(); iter++) 
                                            {
                                                //tmpId = iter->first;
                                                sendData(iter->first, reMsg);
                                            }
                                            //sendData(fd, reMsgMe);
                                            //MYPRINTF("世界发言");
                                        }else{
                                            MYPRINTF("世界发言失败，未找到人");
                                        }
                                }
                                else
                                {   
                                    MYPRINTF("未登陆");
                                }

                            break;
                            case 103:
                                //私聊
                                if(mysplit[1].c_str() == NULL || mysplit[2].c_str() == NULL){
                                        MYPRINTF("103数据不合格~~~~\n");
                                        clearInfo(fd,5);
                                }else{
                                    if(checkLogin(fd))
                                    {
                                        string nickName = fdList[fd];
                                        if(userList.count(nickName) != 0){
                                            reMsg = "1003" + s_sp_1 + nickName + s_sp_2 + int2string(userList[nickName].userId) + s_sp_1 + mysplit[2];

                                            if(userList.count(mysplit[1]) != 0)
                                            {
                                                sendData(userList[mysplit[1]].userFd,reMsg);
                                                reMsgMe = "3003" + s_sp_1 + mysplit[1] + s_sp_2 + int2string(userList[mysplit[1]].userId) + s_sp_1 + mysplit[2];
                                            }
                                            else
                                            {
                                                reMsgMe = "2002";
                                            }
                                            sendData(fd, reMsgMe);
                                            MYPRINTF("私聊");
                                        }else{
                                            MYPRINTF("私聊失败，未找到人");
                                        }
                                    }
                                    else
                                    {   
                                        MYPRINTF("未登陆");
                                    }
                                }
                            break;
                            case 104:
                                //加入频道
                                if(checkLogin(fd))
                                {
                                    string channelId = mysplit[1];
                                    string nickName = fdList[fd];
                                    if(userList.count(nickName) != 0){
                                        if(userList[nickName].channelCnt > 15)
                                        {   
                                            //std::cout << nickName << "加入频道数为>>" << userList[nickName].channelCnt << std::endl;
                                            reMsgMe = "2004";
                                            sendData(fd, reMsgMe);
                                        }
                                        else
                                        {

                                            if(userList[nickName].channelId.count(channelId) != 0)
                                            {
                                                reMsgMe = "2005";
                                                sendData(fd, reMsgMe);
                                            }
                                            else
                                            {
                                                userList[nickName].channelId.insert(channelId);
                                                userList[nickName].channelCnt += 1;
                                                channelList.insert(make_pair(channelId, nickName));

                                                //MYPRINTF("%d加入频道成功",fd);

                                                //如果是要通知的频道，告诉频道内所有人
                                                reMsg = "1004" + s_sp_1 + nickName + s_sp_2 + int2string(userList[nickName].userId) + s_sp_2 + int2string(userList[nickName].isVip) + s_sp_2 + int2string(userList[nickName].weaponClass) + s_sp_2 + int2string(userList[nickName].gender) + s_sp_2 + int2string(userList[nickName].wantedState) + s_sp_2 + int2string(userList[nickName].pk_crime) + s_sp_1 + channelId;

                                                std::cout << "welcome 1004>>" << reMsg << std::endl;
                                                int first = INI_CHANNEL_LIST.find_first_of(channelId.substr(0, 1));
                                                if(first != -1)
                                                {
                                                    //MYPRINTF("广播XX加入");
                                                    sendDataChannel(channelId,reMsg);
                                                }
                                                else
                                                {
                                                    sendData(fd, reMsg);
                                                }
                                            }
                                        }
                                    }else{
                                        MYPRINTF("不在线104");  
                                    }   
                                }
                                else
                                {   
                                    MYPRINTF("未登陆");
                                }
                            break;
                            case 105:
                                //退出频道
                                if(checkLogin(fd))
                                {
                                    string channelId = mysplit[1];
                                    string nickName = fdList[fd];
                                    if(userList.count(nickName) != 0){
                                        userList[nickName].channelId.erase(channelId);
                                        userList[nickName].channelCnt -= 1;

                                        //退出
                                        quitChannelList(channelId,nickName);
                                        //MYPRINTF("退出频道成功");

                                        //如果是要通知的频道，告诉频道内所有人
                                        first = INI_CHANNEL_LIST.find_first_of(channelId.substr(0, 1));
                                        if(first != -1)
                                        {
                                            reMsg = "1005" + s_sp_1 + nickName + s_sp_2 + int2string(userList[nickName].userId) + s_sp_1 + channelId;
                                            sendDataChannel(channelId,reMsg);   
                                        }
                                    }else{
                                        MYPRINTF("不在线105");
                                    }
                                }
                                else
                                {   
                                    MYPRINTF("未登陆");
                                }
                            break;
                            case 106:
                                //频道内发言
                                if(mysplit[1].c_str() == NULL || mysplit[2].c_str() == NULL){
                                        MYPRINTF("106数据不合格~~~~\n");
                                        clearInfo(fd,5);
                                }else{
                                    if(checkLogin(fd))
                                    {
                                        string nickName = fdList[fd];
                                        channelId = mysplit[1];
                                        if(userList.count(nickName) != 0){
                                            reMsg = "1006" + s_sp_1 + nickName + s_sp_2 + int2string(userList[nickName].userId) + s_sp_1 + channelId +s_sp_1 + mysplit[2];

                                            sendDataChannel(channelId,reMsg);
                                            //MYPRINTF("频道发言成功");
                                        }else{
                                            MYPRINTF("频道发言失败，没找到加入人");
                                        }
                                    }
                                    else
                                    {   
                                        MYPRINTF("未登陆");
                                    }
                                }
                            break;
                            case 107:
                                //获取频道列表
                                if(checkLogin(fd))
                                {
                                    string channelId = mysplit[1];
                                    string channelUserLists ="";

                                    std::cout << "107 >>" << fd << "|" <<channelId << std::endl;

                                    //struct userInfo user;
                                    Channel::iterator position = channelList.lower_bound(channelId);
                                    while (position != channelList.upper_bound(channelId)) {
                                        if(userList.count(position->second) != 0){
                                                channelUserLists += position->second + s_sp_2 + int2string(userList[position->second].userId) + s_sp_2 + int2string(userList[position->second].isVip) + s_sp_2 + int2string(userList[position->second].weaponClass) + s_sp_2 + int2string(userList[position->second].gender) + s_sp_2 + int2string(userList[position->second].wantedState) + s_sp_2 + int2string(userList[position->second].pk_crime) + ",";
                                            }
                                        ++position;
                                    }

                                    reMsg = "1007" + s_sp_1 + channelId + s_sp_1 + channelUserLists;
                                    sendData(fd, reMsg);

                                    //MYPRINTF("发送频道列表成功");
                                }
                                else
                                {   
                                    MYPRINTF("未登陆");
                                }
                            break;
                            case 108:
                                //队伍 - TEAM
                                if(mysplit[1].c_str() == NULL || mysplit[2].c_str() == NULL){
                                    MYPRINTF("108数据不合格~~~~\n");
                                    clearInfo(fd,5);
                                }else{
                                        string nickName,channelId;
                                        //cout<<logString<<"开始108部分--"<<endl;
                                        //o_file << string(logString) << std::endl;
                                        unsigned short opCode2 = atoi(mysplit[1].c_str());
                                        switch (opCode2)
                                        {
                                            case 1:
                                                //邀请某人加入队伍
                                                nickName = mysplit[2];
                                                if(userList.count(nickName) != 0){
                                                    reMsg = "1008|1|" + nickName + s_sp_2 + int2string(userList[nickName].userId);
                                                    sendData(userList[mysplit[3]].userFd, reMsg);
                                                }else{
                                                    MYPRINTF("对方不在线");
                                                }
                                            break;
                                            case 2:
                                                //把某人加入某队伍
                                                nickName = mysplit[2];
                                                channelId = mysplit[3];
                                                cout<<nickName<<"--加入队伍--"<<channelId<<endl;
                                                if(userList.count(nickName) != 0){
                                                    //userList[nickName].group = atoi(channelId.c_str());
                                                    userList[nickName].channelId.insert(channelId);
                                                    userList[nickName].channelCnt += 1;

                                                    channelList.insert(make_pair(channelId, nickName));
                                                    //如果是要通知的频道，告诉频道内所有人
                                                    reMsg = "1008|2|" + nickName + s_sp_2 + int2string(userList[nickName].userId) + s_sp_1 + channelId;

                                                    if(userList.count(mysplit[4]) != 0){
                                                        sendDataGroup(channelId,reMsg,userList[mysplit[4]].userFd);
                                                    }else{
                                                        sendDataGroup(channelId,reMsg,1);
                                                    }
                                                }else{
                                                    MYPRINTF("不在线");
                                                }
                                            break;
                                            case 3:
                                                //修改某人权限
                                                nickName = mysplit[2];
                                                channelId = mysplit[3];
                                                if(userList.count(nickName) != 0){
                                                    //userList[nickName].groupLevel = groupLevel;
                                                    cout<<nickName<<"被修改权限"<<endl;
                                                    reMsg = "1008|3|" + nickName +  s_sp_2 + int2string(userList[nickName].userId) + s_sp_1 + channelId;

                                                    if(userList.count(mysplit[4]) != 0){
                                                        sendDataGroup(channelId,reMsg,userList[mysplit[4]].userFd);
                                                    }else{
                                                        sendDataGroup(channelId,reMsg,1);
                                                    }
                                                }else{
                                                    MYPRINTF("不在线");
                                                }
                                            break;
                                            case 4:
                                                //退出队伍
                                                nickName = mysplit[2];
                                                channelId = mysplit[3];

                                                if(userList.count(nickName) != 0){
                                                    quitGroup(channelId,nickName);
                                                    quitChannelList(channelId,nickName);
                                                    reMsg = "1008|4|" + nickName + s_sp_2 + int2string(userList[nickName].userId);
                                                    sendDataChannel(channelId,reMsg);
                                                }else{
                                                    MYPRINTF("不在线");
                                                }
                                            break;
                                            case 5:
                                                //队伍列表
                                                if(checkLogin(fd))
                                                {
                                                    channelId = mysplit[2];
                                                    string channelUserLists ="";

                                                    Channel::iterator position = channelList.lower_bound(channelId);
                                                    while (position != channelList.upper_bound(channelId)) {
                                                        if(userList.count(position->second) !=0 ){
                                                            channelUserLists += position->second + s_sp_2 + int2string(userList[position->second].userId) + ",";
                                                        }
                                                        ++position;
                                                    }

                                                    reMsg = "1008|5" + s_sp_1 + channelId + s_sp_1 + channelUserLists;
                                                    sendData(fd, reMsg);

                                                    MYPRINTF("发送频道列表成功");
                                                }
                                                else
                                                {   
                                                    MYPRINTF("未登陆");
                                                }
                                            break;
                                            case 6:
                                                //解散队伍
                                                if(mysplit[2].c_str() == NULL){
                                                    MYPRINTF("108数据不合格~~~~\n");
                                                    clearInfo(fd,5);
                                                }else{
                                                    channelId = mysplit[2];
                                                    //给队伍里所有人发一条队伍解散信息
                                                    reMsg = "1008|6|" + mysplit[3] + "|" + channelId;
                                                    if(userList.count(mysplit[4]) != 0){
                                                        sendDataGroup(channelId,reMsg,userList[mysplit[4]].userFd);
                                                    }else{
                                                        sendDataGroup(channelId,reMsg,1);
                                                    }
                                                    //遍历队伍里所有人
                                                    Channel::iterator position = channelList.lower_bound(channelId);
                                                    while (position != channelList.upper_bound(channelId)) {
                                                        //替每个人退出下组队状态
                                                        std::cout << position->second << "开始解散出队伍"<<channelId<< std::endl;
                                                        if(userList.count(position->second) != 0){
                                                            //找到的人
                                                            quitGroup(channelId,position->second);
                                                        }
                                                        ++position;
                                                    }
                                                    channelList.erase(channelId);
                                                }
                                            break;
                                            case 7:
                                                //踢出队伍
                                                nickName = mysplit[2];
                                                channelId = mysplit[3];
                                                std::cout << nickName << "开始被踢出队伍"<<channelId<< std::endl;
                                                if(userList.count(nickName) != 0){
                                                    reMsg = "1008|7|" + nickName + s_sp_2 + int2string(userList[nickName].userId) + "|" + mysplit[4];

                                                    if(userList.count(mysplit[5]) != 0){
                                                        sendDataGroup(channelId,reMsg,userList[mysplit[5]].userFd);
                                                    }else{
                                                        sendDataGroup(channelId,reMsg,1);
                                                    }
                                                    quitGroup(channelId,nickName);
                                                    quitChannelList(channelId,nickName);
                                                    std::cout << nickName << "踢出队伍成功"<<channelId<< std::endl;
                                                }else{
                                                    MYPRINTF("不在线");
                                                }
                                            break;
                                            case 8:
                                                //邀请某人加入队伍
                                                nickName = mysplit[2];
                                                if(userList.count(nickName) != 0 && userList.count(mysplit[3]) != 0){
                                                    reMsg = "1008|8|" + nickName + s_sp_2 + int2string(userList[nickName].userId);
                                                    sendData(userList[mysplit[3]].userFd, reMsg);
                                                }else{
                                                    MYPRINTF("不在线");
                                                }
                                            break;
                                            case 9:
                                                //队长广播消息到全队
                                                channelId = mysplit[2];
                                                nickName = mysplit[5];
                                                reMsg = "1008|9|" + channelId + s_sp_1 + mysplit[3] + s_sp_1 + mysplit[4];
                                                if(userList.count(nickName) != 0){
                                                    sendDataGroup(channelId,reMsg,userList[nickName].userFd);
                                                }else{
                                                    sendDataGroup(channelId,reMsg,1);
                                                }

                                                MYPRINTF("队伍广播");
                                            break;
                                            default:{
                                                MYPRINTF("108无");
                                            }
                                            break;
                                        }
                                }
                            break;
                            case 10000:
                                if(checkSystem(mysplit[3])){
                                    //系统消息-群发
                                    reMsg = "10000" + s_sp_1 +  mysplit[1] + s_sp_1 + mysplit[2];
                                    //遍历userList的MAP
                                    for (iter = fdList.begin(); iter != fdList.end(); iter++) 
                                    {
                                        tmpId = iter->first;
                                        sendData(tmpId, reMsg);
                                    }
                                    MYPRINTF("系统消息");
                                }else{
                                    MYPRINTF("key错误");
                                }
                            break;
                            case 10001:
                                if(checkSystem(mysplit[4])){
                                    if(userList.count(mysplit[2]) != 0){
                                        //系统消息-只发给某人
                                        reMsg = "10001" + s_sp_1 +  mysplit[1] + s_sp_1 + mysplit[3];
                                        //遍历userList的MAP
                                        //std::cout << "10001发送出： >>" << reMsg << std::endl;
                                        sendData(userList[mysplit[2]].userFd, reMsg);
                                        MYPRINTF("系统消息发送给某人成功");
                                    }else{
//                                      std::cout << mysplit[2] << "消息debug"<< std::endl;
                                        MYPRINTF("此人不在线");
                                    }
                                }else{

                                    MYPRINTF("key错误");
                                }
                            break;
                            case 10002:
                                if(checkSystem(mysplit[4])){
                                    //修改用户状态
                                    //unsigned short userState = atoi(mysplit[2].c_str());
                                    if(userList.count(mysplit[1]) != 0){
                                        switch (atoi(mysplit[2].c_str()))
                                        {
                                            case 1:
                                                userList[mysplit[1]].state = atoi(mysplit[3].c_str());
                                                MYPRINTF("修改用户状态成功");
                                            break;
                                            case 2:
                                                userList[mysplit[1]].isVip = atoi(mysplit[3].c_str());
                                                MYPRINTF("修改用户VIP成功");
                                            break;
                                            case 3:
                                                userList[mysplit[1]].wantedState = atoi(mysplit[3].c_str());
                                                MYPRINTF("通缉状态");
                                            break;
                                            case 4:
                                                userList[mysplit[1]].pk_crime = atoi(mysplit[3].c_str());
                                                MYPRINTF("罪恶值");
                                            break;
                                            default:
                                            MYPRINTF("默认的");
                                        }
                                    }else{
                                        MYPRINTF("查无此人");
                                    }
                                }else{
                                    MYPRINTF("key错误");
                                }
                            break;
                            case 10003:
                                if(checkSystem(mysplit[1])){
                                    //获取在线人数，启动时间
                                    onlines = fdList.size();
                                    reMsg = "10003" + s_sp_1 + int2string(onlines) + s_sp_1 + starttime;
                                    sendData(fd, reMsg);
                                    MYPRINTF("发送系统状态完毕");
                                }else{
                                    MYPRINTF("key错误");
                                }
                            break;
                            case 10004:
                                if(checkSystem(mysplit[4])){
                                    //系统消息-广播到频道
                                    reMsg = "10004" + s_sp_1 +  mysplit[1] + s_sp_1 +  mysplit[2] + s_sp_1 + mysplit[3];
                                    //遍历userList的MAP
                                    std::cout << "10004发送出： >>" << reMsg << std::endl;
                                    sendDataChannel(mysplit[2],reMsg);
                                    MYPRINTF("系统消息发送给某人成功");
                                }else{
                                    MYPRINTF("key错误");
                                }
                            break;
                            case 10005:
                                if(checkSystem(mysplit[2])){
                                    if(userList.count(mysplit[1])!=0 ){
                                        //获取某人信息
                                        reMsg = "10005" + s_sp_1 + mysplit[1] + s_sp_1 + userList[mysplit[1]].nickName +s_sp_1 +int2string(userList[mysplit[1]].userFd) + s_sp_1 + int2string(userList[mysplit[1]].userId);
                                    }else{
                                        //获取某人信息
                                        reMsg = "查无此人或不在线";
                                    }
                                    sendData(fd, reMsg);
                                    MYPRINTF("发送系统状态完毕");
                                }else{
                                    MYPRINTF("key错误");
                                }
                            break;
                            default:
//                              clearInfo(fd,6);
                                MYPRINTF("瞎发啥");
                        }
                    }
                    if (plen >= n) break;
                    p = &line[plen]; 
                }
            }
        }
    }
}
