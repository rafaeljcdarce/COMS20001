#include "dinner.h"

int chopsticks[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int chopstick_lock = 1;

int getLeftChopstick(int id){
    return id;
}

int getRightChopstick(int id){
    if(id == 15) return 0;
    return id+1;
}


void main_dinner (){
    write( STDOUT_FILENO, "Loading the Dining Philosophers...\n", 34);

    //create child processes (the philosophers)
    for(int i = 0; i<16; i++){
        
        if(fork() == 0){

            //give unique id to each philosopher (first init all even IDs then odds)
            int id = (i<8) ? i*2 : 2*(i-8) + 1;
            char p[2];
            itoa(p, id);
            write( STDOUT_FILENO, "Philosopher ", 12);
            write( STDOUT_FILENO, p, 2);
            write( STDOUT_FILENO, " has sat down for dinner\n", 26 );
            
            //solution to dining philosophers problem
            while(1){



                sleep(id);

                write( STDOUT_FILENO, "Philosopher ", 12);
                write( STDOUT_FILENO, p, 2);
                write( STDOUT_FILENO, " is waiting\n", 12 );



                //wait for chopstick lock
                sem_wait(&chopstick_lock);

                int l = getLeftChopstick(id);
                int r = getRightChopstick(id);

                //only pick up chopsticks if both are available together
                if(chopsticks[l] == 1 && chopsticks[r] == 1){
                    sem_wait(&chopsticks[r]);
                    sem_wait(&chopsticks[l]);
                    sem_post(&chopstick_lock);
                    write( STDOUT_FILENO, "Philosopher ",12);
                    write( STDOUT_FILENO, p, 2);
                    write( STDOUT_FILENO, " picked up both chopsticks\n", 27 );
                }
                else{//keep waiting else
                    sem_post(&chopstick_lock);
                    write( STDOUT_FILENO, "Philosopher ", 12);
                    write( STDOUT_FILENO, p, 2);
                    write( STDOUT_FILENO, " cannot pick up both chopsticks\n", 32 );
                    continue;
                }



                //eat food
                write( STDOUT_FILENO, "Philosopher ", 12);
                write( STDOUT_FILENO, p, 2);
                write( STDOUT_FILENO, " is eating\n", 11 );
                sleep(id);

                write( STDOUT_FILENO, "Philosopher ", 12);
                write( STDOUT_FILENO, p, 2);
                write( STDOUT_FILENO, " finished eating\n", 17 );

                //put both chopsticks down, release lock, and break out of loop
                sem_wait(&chopstick_lock);
                sem_post(&chopsticks[r]);
                sem_post(&chopsticks[l]);
                sem_post(&chopstick_lock);

                write( STDOUT_FILENO, "Philosopher ", 12);
                write( STDOUT_FILENO, p, 2);
                write( STDOUT_FILENO, " put down both chopsticks\n", 26 );
            }

        }
    }
    exit( EXIT_SUCCESS );
}
