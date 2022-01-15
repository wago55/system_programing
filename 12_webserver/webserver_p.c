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

#define HOSTLEN 256

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
        fprintf(fp, "Content-type: %s\r\n", content_type);
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
        fprintf(fpsock, "\r\n");
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


// main処理
int main(int argc, char *argv[]){
    int sock, fd;
    FILE *fpin;
    char request[BUFSIZ];

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

    if(sock == -1)
        exit(2);

    while(1){
        // 第1引数: 準備した自分のソケットid
        // 第2引数: 接続してきたクライアントのアドレスが格納されている構造体のポインタ
        // 第3引数: 接続してきたクライアントのアドレスの長さを格納するポインタ
        // 読み書き両方ができるファイルディスクリプタが返ってくる
        fd = accept(sock, NULL, NULL);
        // ファイルディスクリプタをreadでopen
        fpin = fdopen(fd, "r");
        // clientからのrequestを取り出して、serverに出力
        fgets(request, BUFSIZ, fpin);
        printf("fd num: %d, request = %s", fd, request);

        // requestを処理
        process_request(request, fd);
        // threadと違って他の各processで閉じないといけない
        fclose(fpin);
    }
}
