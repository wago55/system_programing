#include<stdio.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<time.h>
#include<strings.h>
#include<pthread.h>
#include<dirent.h>
#include<time.h>

#define HOSTLEN 256

// global変数でserverの状況を管理
time_t server_start_time;
int requests;

// 接続を確率する関数
int make_server_socket(int portnum){
    struct sockaddr_in serv;
    struct hostent *hp;
    char hostname[HOSTLEN];
    int sock_id;

    // 第3引数: ソケット内で使われるプロトコル (デフォルトは0)
    // 作成されたソケットのidが返される
    sock_id = socket(PF_INET, SOCK_STREAM, 0);
    // エラー処理
    if(sock_id == -1)
        return -1;

    // 第一引数に今のhostnameをHOSTLEN分入れてくれる
    gethostname(hostname, HOSTLEN);
    // hostnet構造体のポインタを返す
    hp = gethostbyname(hostname);
    // 第一引数の内容を第二引数に第三引数分コピーする
    bcopy((void *)hp -> h_addr, (void *)&serv.sin_addr, hp -> h_length);
    // ネットワークバイトオーダーに入る符号なし単整数に型変換される
    serv.sin_port = htons(portnum);
    // アドレスファミリを設定
    serv.sin_family = AF_INET;

    if(bind(sock_id, (struct sockaddr *)&serv, sizeof(serv)) != 0)
        return -1;

    if(listen(sock_id, SOMAXCONN) != 0)
        return -1;

    return sock_id;
}

// ヘッダー情報を書き加える
header(FILE *fp, char *content_type){
    fprintf(fp, "HTTP/1.0 200 OK\r\n");
    if(content_type)
    fprintf(fp, "Content-type: %s\r\n\r\n", content_type);
}

// ファイルが存在するかのチェック
int not_exist(char *file){
    struct stat info;
    return(stat(file, &info) == -1);
}

int cat_file(char *file, int fd){

    char *content = "text/plain";
    FILE *fpsock, *fpfile;
    int c;

    fpsock = fdopen(fd, "w");
    fpfile = fopen(file, "r");
    // fdopen, fopenが正常実行されているかを条件に
    if(fpsock != NULL && fpfile != NULL){
        
        // conten-typeとresponse結果を作る
        header(fpsock, content);
        // fprintf(fpsock, "\r\n");

        // fileの中身をfpsockに書き写し(fがfile nameだからopenしてreadしてファイルディスクリプタに書き写し)
        while((c = getc(fpfile)) != EOF){
            putc(c, fpsock);
        }
    
        //   コネクションを閉じる
        fclose(fpfile);
        fclose(fpsock);
    }

    return 0;
}


int process_request(char *rq, int fd){
    char cmd[BUFSIZ], arg[BUFSIZ];
    FILE *fp;

    // 第二引数を第一引数にコピー
    strcpy(arg, "./");

    // requestからformatの形式で取り出してcmd, arg + 2に代入
    if(sscanf(rq, "%s%s", cmd, arg + 2) != 2)
        return 0;

    // cmdが"GET"かを確かめる
    if(strcmp(cmd, "GET") != 0){
        fp = fdopen(fd, "w");
        fprintf(fp, "this server supported only GET");
        fflush(fp);
        fclose(fp);
    }
    // argが"status"の時の処理
    else if(strcmp(arg + 2, "status") == 0){
        fp = fdopen(fd, "w");
        fprintf(fp, "Server started: %s", ctime(&server_start_time));
        fprintf(fp, "Total requests: %d\n", requests);
        fflush(fp);
        fclose(fp);
    }
        
    // 要求されたファイルが存在するかをチェックする
    else if(not_exist(arg)){
        fp = fdopen(fd, "w");
        fprintf(fp, "404 not found\n");
        fflush(fp);
        fclose(fp);
    }

    else
        cat_file(arg, fd);

    return 0;
}

void *handler(void *fdptr){
    FILE *fpin;
    char request[BUFSIZ];
    int fd;
    // 次のthreadのために元々のfdptrを開放
    fd = *(int *)fdptr;
    free(fdptr);
    // ファイルディスクリプタをファイルのようにopenする
    // 返り値は*FILE型
    fpin = fdopen(fd, "r");
    fgets(request, BUFSIZ, fpin);
    printf("fd num: %d, request = %s", fd, request);

    process_request(request, fd);

    fclose(fpin);
}

// sever管理のglobal変数の設定
int set_global(pthread_attr_t *attrp){
    // threadの属性を初期化する
    pthread_attr_init(attrp);
    // threadをデタッチ化する
    // threadの終了を待たず、threadが終了次第開放する
    pthread_attr_setdetachstate(attrp, PTHREAD_CREATE_DETACHED);
    
    time(&server_start_time);

    requests = 0;

    return 0;
}

// main処理
int main(int argc, char **argv){
    int sock, fd;
    int *fdptr;
    // thread
    pthread_t worker;
    // threadの属性
    pthread_attr_t attr;
    // mutex lock
    pthread_mutex_t requests_lock = PTHREAD_MUTEX_INITIALIZER;

    if(argc == 1){
        // ファイルディスクリプタ -> openされたファイルは順番に配列に格納されている。その添字がファイルディスクリプタ
        // 標準ファイルディスクリプタ
        // 0 stdin
        // 1 stdout
        // 2 stderr
        fprintf(stderr, "command: ./(obj file name) portnum\n");
        exit(1);
    }

    sock = make_server_socket(atoi(argv[1]));
    if(sock == -1){
        perror("error make socket");
        exit(2);
    }

    set_global(&attr);

    while(1){
        // 第1引数: 準備した自分のソケットid
        // 第2引数: 接続してきたクライアントのアドレスが格納されている構造体のポインタ
        // 第3引数: 接続してきたクライアントのアドレスの長さを格納するポインタ
        // 読み書き両方ができるファイルディスクリプタが返ってくる
        fd = accept(sock, NULL, NULL); 
        
        // mutex lock
        pthread_mutex_lock(&requests_lock);
        // グローバル変数でリクエスト数を管理
        requests++;
        // mutex unlock
        pthread_mutex_unlock(&requests_lock);
        
        // ファイルデスクリプタのポインタ用のメモリ確保
        fdptr = malloc(sizeof(int));

        *fdptr = fd;
        // 第1引数: pthread_t型の変数を指すポインタ
        // 第2引数: pthread_attr_t型変数を指すポインタかNULL
        // 第3引数: このthreadを起動する関数
        // 第4引数: 関数に渡す引数
        // 関数名ってポインタ型らしい、、、(初知り)
        pthread_create(&worker, &attr, handler, fdptr);

    }
}