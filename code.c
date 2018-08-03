#include<stdio.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>

// Number of trouters, keep it at max routers + 1
#define numrout 21

//actual number of routers
int routers;
int globaldone = 0;
int globalprintdone = 1;

pthread_t tid[numrout];

//Mutex to prevent parallel overwrites
pthread_mutex_t mutexes[numrout];
//Mutex to prevent parallel printing
pthread_mutex_t printer;
//Counter to print elements
int printornot;

struct row {
    int link;
    int cost;
};

struct row tables[numrout][numrout];

int inp[numrout][numrout];

int routernumber = 0;


void input(void) {
    int i, j, temp;
    FILE *fp;
    fp = fopen("inputfile", "r");

    fscanf(fp, "%d", &routers);

    printf("Reading input file now..\n");

    for(i = 1; i <= routers; i++ ) {
        for(j = 1; j <= routers; j++) {
            fscanf(fp, "%d", &inp[i][j]);
        }
    }

    printf("Finished reading input file.\n");

    fclose(fp);
}

void* ripRouters(void *arg)
{
    int i, j, status;
    int rl, rd, rc;
    int locdone = 0;


    //pthread_t myid = pthread_self();
    pthread_mutex_lock(&printer);
    int myid = ++routernumber;
    pthread_mutex_unlock(&printer);

    sleep(2);

    //self initialization
    pthread_mutex_lock(&(mutexes[myid]));

    //Read connection info from input table, then add appropriate entries in tables
    for(i = 1; i <= routers; i++) {
        if(inp[myid][i] != 0) {
            tables[myid][i].cost = 1;
            tables[myid][i].link = inp[myid][i];
        }
        else {
            tables[myid][i].cost = 999;
            tables[myid][i].link = 999;
        }
    }
    //Add self info in tables with
    tables[myid][myid].cost = 0;
    tables[myid][myid].link = -1;
    //set self link = -1 to ease future processing
    //inp[myid][myid] = -1;

    pthread_mutex_unlock(&mutexes[(int)myid]);

    //Wait for all other threads to initialize themselves
    sleep(2);

    //Begin processing RIP
    while(1==1) {
        if(locdone >= routers) {
            pthread_mutex_lock(&printer);
            globaldone++;
            pthread_mutex_unlock(&printer);
            while(globaldone < routers) {
                pthread_mutex_lock(&printer);
                //printf("Router %d waiting on others to finish. globaldone = %d.routers = %d\n", myid, globaldone, routers);
                printf("Router %d waiting on others to finish.\n", myid);
                pthread_mutex_unlock(&printer);
                sleep(5);
            }
            while(globalprintdone < myid) {
                sleep(1);
            }
            pthread_mutex_lock(&mutexes[(int)myid]);
            pthread_mutex_lock(&printer);
            printf("--------\nRouter %d\'s table:\n", (int)myid);
            printf("Dest |  Link | Cost\n");
            for(i = 1; i <= routers; i++) {
                if(tables[myid][i].cost != 999 && tables[myid][i].link != -1) {
                    printf("%4d | %5d | %4d\n", i, tables[myid][i].link, tables[myid][i].cost);
                }
                else if(tables[myid][i].cost != 999 && tables[myid][i].link == -1) {
                    printf("%4d | local | %4d\n", i, tables[myid][i].cost);
                }
            }
            printf("Finished printing router %d's table.\n--------\n", (int)myid);
            globalprintdone++;
            sleep(1);
            pthread_mutex_unlock(&printer);
            pthread_mutex_unlock(&mutexes[myid]);
            // Simplistic way to prevent multiple prints by same router
            pthread_exit(0);
        }
        else {
            //printf("Still going on. Thread %d has locdone = %d.\n", myid, locdone);
            printf("Thread %d still going on.\n", myid);
            if(printornot == 1 && locdone == 0) {
                pthread_mutex_lock(&printer);
                printf("--------\nRouter %d\'s intermediate table:\n", (int)myid);
                printf("Dest |  Link | Cost\n");
                for(i = 1; i <= routers; i++) {
                    if(tables[myid][i].cost != 999 && tables[myid][i].link != -1) {
                        printf("%4d | %5d | %4d\n", i, tables[myid][i].link, tables[myid][i].cost);
                    }
                    else if(tables[myid][i].cost != 999 && tables[myid][i].link == -1) {
                        printf("%4d | local | %4d\n", i, tables[myid][i].cost);
                    }
                }
                sleep(1);
                pthread_mutex_unlock(&printer);
            }
        }
        pthread_mutex_lock(&mutexes[(int)myid]);
        for(i = 1; i <= routers; i++) {
            if(inp[myid][i] != 0) {
                status = pthread_mutex_trylock(&mutexes[i]);
                if(status != EBUSY) {
                    for(j = 1; j <= routers; j++) {
                        //If cost != 999, it means entry exists
                        if(tables[i][j].cost != 999 && i != j) {
                            //Here we check if Rr.link == n
                            if(tables[i][j].link != inp[myid][i]) {
                                rd = j;
                                //rl = tables[i][j].link;
                                rl = inp[myid][i];
                                rc = tables[i][j].cost + 1;
                                // If Rr.dest is not in my table i.e. its cost is infinite
                                if(tables[myid][rd].cost == 999) {
                                    tables[myid][rd].cost = rc;
                                    tables[myid][rd].link = rl;
                                    locdone = 0;
                                }
                                else {
                                    //Here we perform the last component, for all Rl in Tl, except that since this is already inside outer loop, we check only one row
                                    if((tables[myid][rd].cost > rc) || ((tables[myid][rd].link == inp[myid][i])&&(rc > tables[myid][rd].cost))) {
                                        tables[myid][rd].link = rl;
                                        tables[myid][rd].cost = rc;
                                        locdone = 0;
                                    }
                                    else {
                                        locdone++;
                                    }
                                }
                            }
                        }
                        else {
                            locdone++;
                        }
                    }
                    pthread_mutex_unlock(&mutexes[i]);
                }
                else {
                    //printf("Router %d is busy, skipping for now.\n", i);
                }
            }
        }
        pthread_mutex_unlock(&mutexes[(int)myid]);
        //Since this is not a complex program, add sleep to give the other threads a chance to update themselves too
        sleep(1);
    }

    return NULL;
}
/*
void* printFunction(void *arg)
{
    int i = 1;
    while(i == 1) {
        printf("Enter 1 to have all routers print their data, or 0 to exit printing function: ");
        scanf("%d", &i);
        if(i == 0) break;
        pthread_mutex_lock(&printer);
        printcount = routers;
        pthread_mutex_unlock(&printer);
        sleep(5);
    }
    printf("Exiting print function.\n");
    return NULL;
}
*/
int main(int argc, char *argv[] )
{
    int i, err;

    input();
    //mutex initialization
    for (int i = 1; i <= routers; i++)
        pthread_mutex_init(&mutexes[i], NULL);
    //pthread_mutex_init(&printer, NULL);
    //thread creation
    if(argc == 2) {
        if(argv[1][0] == '0') {
            printornot = 0;
            printf("Will not print intermediate routing tables.\n");
        }
        else {
            printornot = 1;
            printf("Will print intermediate routing tables.\n");
        }
    }
    for(i = 1; i <= routers; i++) {
        err = pthread_create(&(tid[i]), NULL, &ripRouters, NULL);
        if (err != 0)
            printf("Can't create thread %d :[%s]\n", i, strerror(err));
        else
            printf("Thread %d created successfully.\n", i);
    }

    //Print control thread
    //pthread_create(&(tid[0]), NULL, &printFunction, NULL);


    //waiting for threads to return
    for(i = 1; i <= routers; i++) {
        pthread_join(tid[i], NULL);
    }
    //pthread_join(tid[0], NULL);

    pthread_mutex_destroy(mutexes);
    //pthread_mutex_destroy(&printer);

    return 0;
}
