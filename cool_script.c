#include <stdio.h>
#include <unistd.h>

int main(){
    FILE* file = fopen("test.txt","w");
    for(int i = 0; i < 60; i++){
        sleep(1);
        fprintf(file, "I'm alive!\n");
    }
    return 0;
}