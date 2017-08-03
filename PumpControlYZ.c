#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <semaphore.h>

#include "SharedData.h"
#include "simulator.h"
#include "mymailbox.h"

#define MS_L1 70
#define MS_L2 100 
#define WL_High 1
#define WL_Low 0
#define MS_Normal 0
#define MS_Alarm1 1
#define MS_Alarm2 2

shareddata AlarmState;
shareddata WaterState;

mailbox mailbox_Alarm;
sem_t sem_NewData;

void  add_timespec (struct timespec *s,
                   const struct timespec *t1,
                   const struct timespec *t2)
/* s = t1+t2 */
{
 s->tv_sec  = t1->tv_sec  + t2->tv_sec;
 s->tv_nsec = t1->tv_nsec + t2->tv_nsec;
 s->tv_sec += s->tv_nsec/1000000000;
 s->tv_nsec %= 1000000000;
}


void *task1_readML(void * unused) { /* A thread is a function that will be run using pthread_create */
	/* local variables */
	int ml;
	struct timespec release; /* will contain the release date */
	struct timespec period; /* contains the period */
	struct timespec remain; /* used for clock_nanosleep */
	period.tv_nsec=50*1000*1000 ; /* le champ en nanosecondes vaut 1 seconde */
	period.tv_sec=0 ; /* le champ en secondes vaut 0 */
	/* initialization */
	clock_gettime(CLOCK_REALTIME,&release); /* release=current time */
	for (;;) { /* do forever */
	//clock_gettime(CLOCK_REALTIME,&release); /* release=current time */
		/* task body */
		//printf("T1\n");
		ml=ReadMS();
		if (ml < MS_L1){
			SD_write(AlarmState, MS_Normal);}
	    else if (ml < MS_L2){
			SD_write(AlarmState, MS_Alarm1);}
	    else{
			SD_write(AlarmState, MS_Alarm2);
		}
		sem_post(&sem_NewData);
		add_timespec(&release, &release, &period); /* computing next release time */
		clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&release,&remain); /* wait until release time */
	}
}

void *task2_readWL(void * unused) { /* A thread is a function that will be run using pthread_create */
	/* local variables */
	int wl_high,wl_low;
	struct timespec release; /* will contain the release date */
	struct timespec period; /* contains the period */
	struct timespec remain; /* used for clock_nanosleep */
	period.tv_nsec=400*1000*1000 ; /* le champ en nanosecondes vaut 1 seconde */
	period.tv_sec=0 ; /* le champ en secondes vaut 0 */
	/* initialization */
	clock_gettime(CLOCK_REALTIME,&release); /* release=current time */
	for (;;) { /* do forever */
	//clock_gettime(CLOCK_REALTIME,&release); /* release=current time */
		/* task body */
		//printf("T1\n");
		wl_high=ReadHLS();
		wl_low=ReadLLS();
		if (wl_high==1){
			SD_write(WaterState, WL_High);}
	    else if(wl_low==0){
			SD_write(WaterState, WL_Low);
		}
		sem_post(&sem_NewData);
		add_timespec(&release, &release, &period); /* computing next release time */
		clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&release,&remain); /* wait until release time */
	}
}

void *task3_CtrlAlarm(void * unused){
	int received;
	for(;;){
		mailbox_receive(mailbox_Alarm,(char*)&received);
		//printf("T3:%d\n",received);
		if (received!=MS_Normal){
			CommandAlarm(1);}
		else{
			CommandAlarm(0);
		}
	}
}

void *task4_CtrlPump(void * unused){
	int alarm, wl;
	for(;;){
		sem_wait(&sem_NewData);
		alarm=SD_read(AlarmState);
		wl=SD_read(WaterState);
		//printf("T4 executes since number is odd\n");
		if ((alarm!=MS_Alarm2) && (wl==WL_High)){
			CommandPump(1);}
		else{
			CommandPump(0);
		}
		mailbox_send(mailbox_Alarm, (char*)&alarm);
	}
}

void main() {
	pthread_t Task1, Task2, Task3, Task4;

	InitSimu();// Initializes the simulator (InitSimu),
	AlarmState = SD_init(); // Initializing the two shared data modules,
	WaterState = SD_init(); // Initializing the two shared data modules,
	sem_init(&sem_NewData,0,1); // Initializing the semaphore,
	mailbox_Alarm=mailbox_init(sizeof(int)); // Initializing the mailbox,
	
	pthread_create(&Task1, 0,task1_readML,0); /* creates the thread executing T1 */
	pthread_create(&Task2, 0,task2_readWL,0);
	pthread_create(&Task3, 0,task3_CtrlAlarm,0);
	pthread_create(&Task4, 0,task4_CtrlPump,0);
	
	pthread_join(Task1,0); /* Waits for T1 to terminate */
	mailbox_delete(mailbox_Alarm);
}
