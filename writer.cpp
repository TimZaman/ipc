#include <iostream> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
using namespace std; 
  
int main() 
{ 
    // ftok to generate unique key 
    key_t key = ftok("shmfile", 65);   // key_t ftok(const char *pathname, int proj_id);
  
    // shmget returns an identifier in shmid 
    int shmid = shmget(key, 1024, 0666|IPC_CREAT); // int shmget(key_t key, size_t size, int shmflg);
  
    // shmat to attach to shared memory 
    char *str = (char*) shmat(shmid,(void*)0,0); 
  
    cout << "Write Data : "; 
    gets(str); 
  
    printf("Data written in memory: %s\n",str); 
      
    //detach from shared memory  
    shmdt(str); 
  
    return 0; 
} 