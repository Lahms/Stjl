#include "../header/functions.h"

char mode_start;

void write_in_queue(RT_QUEUE *, MessageToMon);

void f_server(void *arg) {
    int err;
    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    err = run_nodejs("/usr/local/bin/node", "/home/pi/Interface_Robot/server.js");

    if (err < 0) {
        printf("Failed to start nodejs: %s\n", strerror(-err));
        exit(EXIT_FAILURE);
    } else {
#ifdef _WITH_TRACE_
        printf("%s: nodejs started\n", info.name);
#endif
        open_server();
        rt_sem_broadcast(&sem_serverOk);
    }
}

void f_sendToMon(void * arg) {
    int err;
    MessageToMon msg;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

#ifdef _WITH_TRACE_
    printf("%s : waiting for sem_serverOk\n", info.name);
#endif
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    while (1) {

#ifdef _WITH_TRACE_
        printf("%s : waiting for a message in queue\n", info.name);
#endif
        if (rt_queue_read(&q_messageToMon, &msg, sizeof (MessageToRobot), TM_INFINITE) >= 0) {
#ifdef _WITH_TRACE_
            printf("%s : message {%s,%s} in queue\n", info.name, msg.header, msg.data);
#endif

            send_message_to_monitor(msg.header, msg.data);
            free_msgToMon_data(&msg);
            rt_queue_free(&q_messageToMon, &msg);
        } else {
            printf("Error msg queue write\n");
            //perte_com_monitor
            break;
        }
    }
}

void f_receiveFromMon(void *arg) {
    MessageFromMon msg;
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

#ifdef _WITH_TRACE_
    printf("%s : waiting for sem_serverOk\n", info.name);
#endif
    rt_sem_p(&sem_serverOk, TM_INFINITE);
    do {
#ifdef _WITH_TRACE_
        printf("%s : waiting for a message from monitor\n", info.name);
#endif
        err = receive_message_from_monitor(msg.header, msg.data);
        
        if (err > 0 ) {
#ifdef _WITH_TRACE_
        printf("%s: msg {header:%s,data=%s} received from UI\n", info.name, msg.header, msg.data);
#endif
            if (strcmp(msg.header, HEADER_MTS_COM_DMB) == 0) {
                if (msg.data[0] == OPEN_COM_DMB) { // Open communication supervisor-robot
    #ifdef _WITH_TRACE_
                    printf("%s: message open Xbee communication\n", info.name);
    #endif
                    rt_sem_v(&sem_openComRobot);
                }
                else {
                    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
                    robotStarted = 1;
                    rt_mutex_release(&mutex_robotStarted);
                }
                    
                }
            else if (strcmp(msg.header, HEADER_MTS_CAMERA) == 0) {
                switch (msg.data[0]) {
                    case CAM_OPEN :
                        break;
                    case CAM_CLOSE :
                        break;
                    case CAM_ASK_ARENA :
                        break;
                    case CAM_ARENA_CONFIRM :
                        break;
                    case CAM_ARENA_INFIRM :
                        break;
                    case CAM_COMPUTE_POSITION :
                        break;
                    case CAM_STOP_COMPUTE_POSITION :
                        break;
                }
            }
            else if (strcmp(msg.header, HEADER_MTS_DMB_ORDER) == 0) {
                if (msg.data[0] == DMB_START_WITHOUT_WD) { // Start robot
    #ifdef _WITH_TRACE_
                    printf("%s: message start robot\n", info.name);
    #endif 
                    rt_sem_v(&sem_startRobot);

                } else if ((msg.data[0] == DMB_GO_BACK)
                        || (msg.data[0] == DMB_GO_FORWARD)
                        || (msg.data[0] == DMB_GO_LEFT)
                        || (msg.data[0] == DMB_GO_RIGHT)
                        || (msg.data[0] == DMB_STOP_MOVE)) {

                    rt_mutex_acquire(&mutex_move, TM_INFINITE);
                    move = msg.data[0];
                    rt_mutex_release(&mutex_move);
    #ifdef _WITH_TRACE_
                    printf("%s: message update movement with %c\n", info.name, move);
    #endif

                }
            }
        }
        } while (err > 0);
    }


void f_openComRobot(void * arg) {
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    while (1) {
#ifdef _WITH_TRACE_
        printf("%s : Wait sem_openComRobot\n", info.name);
#endif
        rt_sem_p(&sem_openComRobot, TM_INFINITE);
#ifdef _WITH_TRACE_
        printf("%s : sem_openComRobot arrived => open communication robot\n", info.name);
#endif
        err = open_communication_robot();
        if (err == 0) {
#ifdef _WITH_TRACE_
            printf("%s : the communication is opened\n", info.name);
#endif
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_ACK);
            write_in_queue(&q_messageToMon, msg);
        } else {
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_NO_ACK);
            write_in_queue(&q_messageToMon, msg);
        }
    }
}

void f_startRobot(void * arg) {
    int err;

    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    while (1) {
#ifdef _WITH_TRACE_
        printf("%s : Wait sem_startRobot\n", info.name);
#endif
        rt_sem_p(&sem_startRobot, TM_INFINITE);
#ifdef _WITH_TRACE_
        printf("%s : sem_startRobot arrived => Start robot\n", info.name);
#endif
        err = send_command_to_robot(DMB_START_WITHOUT_WD);
        if (err == 0) {
#ifdef _WITH_TRACE_
            printf("%s : the robot is started\n", info.name);
#endif
            rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
            robotStarted = 1;
            rt_mutex_release(&mutex_robotStarted);
            printf("send Ack\n");
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_ACK);
            
            write_in_queue(&q_messageToMon, msg);
            rt_mutex_acquire(&mutex_compteur_com_robot, TM_INFINITE);
            compteur_com_robot = 0;
            rt_mutex_release(&mutex_compteur_com_robot);
        } else {
            rt_mutex_acquire(&mutex_compteur_com_robot, TM_INFINITE);
            compteur_com_robot = compteur_com_robot +1;
            MessageToMon msg;
            set_msgToMon_header(&msg, HEADER_STM_NO_ACK);
            write_in_queue(&q_messageToMon, msg);
            if (compteur_com_robot == 3) {
                MessageToMon msg;
                set_msgToMon_header(&msg, HEADER_STM_LOST_DMB);
                write_in_queue(&q_messageToMon, msg);
                close_communication_robot();
                rt_mutex_release(&mutex_compteur_com_robot);
                //gérer le reset
                break;
            }
            rt_mutex_release(&mutex_compteur_com_robot);   
        }
    }
}

void f_move(void *arg) {
    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);
    rt_sem_p(&sem_barrier, TM_INFINITE);

    /* PERIODIC START */
#ifdef _WITH_TRACE_
    printf("%s: start period\n", info.name);
#endif
    rt_task_set_periodic(NULL, TM_NOW, 100000000);
    while (1) {
#ifdef _WITH_TRACE_
        //printf("%s: Wait period \n", info.name);
#endif
        rt_task_wait_period(NULL);
#ifdef _WITH_TRACE_
        //printf("%s: Periodic activation\n", info.name);
        //printf("%s: move equals %c\n", info.name, move);
#endif
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        int start = robotStarted;
        rt_mutex_release(&mutex_robotStarted);
        if (start) {
            rt_mutex_acquire(&mutex_move, TM_INFINITE);
            int err = send_command_to_robot(move);
            rt_mutex_release(&mutex_move);
            
            if (err < 0) { 
                rt_mutex_acquire(&mutex_compteur_com_robot, TM_INFINITE);
                compteur_com_robot = compteur_com_robot +1;
                if (compteur_com_robot == 3) {
                    MessageToMon msg;
                    set_msgToMon_header(&msg, HEADER_STM_LOST_DMB);
                    write_in_queue(&q_messageToMon, msg);
                    close_communication_robot();
                    rt_mutex_release(&mutex_compteur_com_robot);
                    //gérer le reset
                    break;
                }
                rt_mutex_release(&mutex_compteur_com_robot);
            } 
            else {
               rt_mutex_acquire(&mutex_compteur_com_robot, TM_INFINITE);
               compteur_com_robot = 0; 
               rt_mutex_release(&mutex_compteur_com_robot);
            }            
        }
#ifdef _WITH_TRACE_
            //printf("%s: the movement %c was sent\n", info.name, move);
#endif            
        
    }
}

void write_in_queue(RT_QUEUE *queue, MessageToMon msg) {
    void *buff;
    buff = rt_queue_alloc(&q_messageToMon, sizeof (MessageToMon));
    memcpy(buff, &msg, sizeof (MessageToMon));
    rt_queue_send(&q_messageToMon, buff, sizeof (MessageToMon), Q_NORMAL);
}

void f_battery(void *arg){
    /* INIT */
    RT_TASK_INFO info;
    rt_task_inquire(NULL, &info);
    printf("Init %s\n", info.name);

    /* PERIODIC START */
    rt_task_set_periodic(NULL, TM_NOW, 500000000); 
    do {
        rt_mutex_acquire(&mutex_reset,TM_INFINITE);
        rst = reset;
        rt_mutex_release(&mutex_reset);
        if (rst) {
            break;
        }
        rt_task_wait_period(NULL);
        
        rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
        
        if (robotStarted) {
            int rep = send_command_to_robot(DMB_GET_VBAT);
            if (rep < 0) { 
                rt_mutex_acquire(&mutex_compteur_com_robot, TM_INFINITE);
                compteur_com_robot = compteur_com_robot +1;
                if (compteur_com_robot == 3) {
                     MessageToMon msg;
                    set_msgToMon_header(&msg,HEADER_STM_LOST_DMB);
                    write_in_queue(&q_messageToMon, msg);
                    close_communication_robot();
                    
                    rt_mutex_acquire(&mutex_robotStarted, TM_INFINITE);
                    robotStarted=0;
                    rt_mutex_release(&mutex_robotStarted);
                    rt_mutex_acquire(&mutex_reset,TM_INFINITE);
                    reset=1;
                    rt_mutex_release(&mutex_reset);
                    //gérer le reset
                    break;
                }
                rt_mutex_release(&mutex_compteur_com_robot);
             
            } 
            else {
               rt_mutex_acquire(&mutex_compteur_com_robot, TM_INFINITE);
               compteur_com_robot = 0; 
               rep += 48;
               rt_mutex_release(&mutex_compteur_com_robot);
                MessageToMon msg;
               set_msgToMon_header(&msg,HEADER_STM_BAT);
               set_msgToMon_data(&msg,&rep);
               write_in_queue(&q_messageToMon, msg);
            }
        }
        rt_mutex_release(&mutex_robotStarted);
    } while (rst = 0);
}

/*void f_vision(void *arg){
    
}*/