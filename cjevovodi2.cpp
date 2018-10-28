#include <sys/types.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <string>
#include <ctime>
#include <cmath>
#include <fstream>

using namespace std;

#define MAXREAD 100

int pid;
int mojaPipa1[2]; //roditelj prema djetetu
int mojaPipa2[2]; //dijete prema roditelju

void childWork();
void parentWork();
bool check_string(const char* string);

int main() {
    pipe(mojaPipa1);
    pipe(mojaPipa2);

    pid = fork();
    if (pid == 0) {
        childWork();
    }
    else {
        parentWork();
    }

    return 0;
}

void childWork() {
    dup2(mojaPipa1[0], 0);
    close(mojaPipa1[0]);
    close(mojaPipa1[1]);

    dup2(mojaPipa2[1], 1);
    dup2(mojaPipa2[1], 2);
    close(mojaPipa2[1]);
    close(mojaPipa2[0]);

    execl("/usr/bin/bc", "bc", NULL);
    exit(1);
}

void parentWork() {
    int n;
    int resultTarget;
    string unos;
    int bytes_read;
    string receive_output = "";
    char readbuffer[MAXREAD];
    cout << "Unesi broj zadataka: ";
    cin >> n;

    close(mojaPipa1[0]);
    close(mojaPipa2[1]);
    for (int i = 0; i < n; i++) {
            resultTarget = rand() % (20);
            cout << resultTarget << " = ";
            cin >> unos;
            write(mojaPipa1[1],unos.c_str(), unos.length());
            write(mojaPipa1[1],"\n", 1);
            sleep(1);
            bytes_read = read(mojaPipa2[0], readbuffer, sizeof(readbuffer)-1);
            readbuffer[bytes_read] = '\0';
            if(check_string(readbuffer)) {
                if (resultTarget == atoi(readbuffer)) {
                    printf("%s","ISPRAVNO\n");
                }
                else {
                    printf("%s, tocan odgovor je %s\n","NESIPRAVNO", readbuffer);
                }
            }
            else {
                printf("%s","NEISPRAVAN IZRAZ\n");
            }
    }
}

bool check_string(const char* string) {
  const int string_len = strlen(string);
  for(int i = 0; i < string_len-1; ++i) {
    if(!isdigit(string[i]) && string[0] != '-') 
      return false;
  }
  return true;
}