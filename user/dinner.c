#include "dinner.h"

int chopsticks[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int chopstick_lock = 1;

int getLeftChopstick(int id){
    if(id == 0) return 15;
    return id-1;
}

int getRightChopstick(int id){
    if(id == 15) return 0;
    return id+1;
}

void eatDinner (char id){
    
}
void main_dinner (){
    int id = 0;
    //init all other philosophers as child processes
    for(int i = 1; i<16; i++){
        
        //child branch
        if(fork() == 0){
            
            //give unique letter as id to each philosopher
            id = i;
            break;
            
        }
    }
    
    //solution to dining philosophers problem
    while(1){

        write( STDOUT_FILENO, 'A', 2 );

        //wait for chopstick lock
        sem_wait(&chopstick_lock);

        int l = getLeftChopstick(id);
        int r = getRightChopstick(id);

        //if right chopstick not available: release lock and keep waiting
        if(chopsticks[r] == 0){
            sem_post(&chopstick_lock);
            continue;
        }
        else{//left is available: pick up
            chopsticks[r] == 0;            
        }
        
        //if right chopstick not available: release chopsticks and lock, and keep waiting
        if(chopsticks[l] == 0){
            chopsticks[r] == 1;
            sem_post(&chopstick_lock);
            continue;
        }
        else{//right is also available: pick up
            chopsticks[r] == 0;            
        }
    
        //eat food
        
        //put both chopsticks down, release lock, and break out of loop
        chopsticks[r] == 1;
        chopsticks[l] == 1;
        sem_post(&chopstick_lock);
        break;

    }
            
    //terminate-self after eating all food
    exit( EXIT_SUCCESS );
}