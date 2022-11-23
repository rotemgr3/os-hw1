#include <stdio.h>

int main(){
    FILE* file = fopen("test.txt","w");
    for(int i = 0; i < 5000; i++){
        fprintf(file, "I'm alive!\n");
    }
    return 0;
}