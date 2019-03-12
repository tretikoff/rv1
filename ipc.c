#include "common.h"
#include "ipc.h"
#include "pa1.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>

/*
 * typedef struct {
    uint16_t     s_magic;        ///< magic signature, must be MESSAGE_MAGIC
    uint16_t     s_payload_len;  ///< length of payload
    int16_t      s_type;         ///< type of the message
    timestamp_t  s_local_time;   ///< set by sender, depends on particular PA:
                                 ///< physical time in PA2 or Lamport's scalar
                                 ///< time in PA3
} __attribute__((packed)) MessageHeader;
 */

int receive_all(void *self, Message *msgs, MessageType type);

void createMessageHeader(Message *msg, MessageType type);

typedef struct {
    int read;
    int write;
} fd;

typedef struct {
    int procCount;
    int ***fds;
} InputOutput;

typedef struct {
    InputOutput io;
    int self;
} SelfInputOutput;

int main(int argc, char **argv) {
    int proc_count;

    int opt = 0;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                proc_count = atoi(optarg);
        }
    }

    int pids[proc_count + 1];

    InputOutput io;
    io.procCount = proc_count;
    io.fds = (int ***) calloc((proc_count + 1), sizeof(int **));

    FILE *pipes_logfile = fopen(pipes_log, "a+");
    for (int i = 0; i <= proc_count + 1; ++i) {
        io.fds[i] = (int **) calloc((proc_count + 1), sizeof(int *));
        for (int j = 0; j <= proc_count + 1; ++j) {
            io.fds[i][j] = (int *) calloc(2, sizeof(int));
            pipe(io.fds[i][j]);
            fprintf(pipes_logfile, "%d %d was opened\n", i, j);
        }
    }

    int pid = 0;
    for (int i = 1; i <= proc_count; ++i) {
        pid = fork();
        if (pid == 0) {
            pids[i] = pid;
            SelfInputOutput sio = {io, i};

            FILE *logfile;
            logfile = fopen(events_log, "a+");
            fprintf(logfile, log_started_fmt, i, getpid(), getppid());
            fflush(logfile);

            Message msg;
            sprintf(msg.s_payload, log_started_fmt, i, getpid(), getppid());
            createMessageHeader(&msg, STARTED);

            send_multicast(&sio, &msg);

            Message msgs[proc_count + 1];
            receive_all(&sio, msgs, STARTED);
            fprintf(logfile, log_received_all_started_fmt, i);
            fflush(logfile);

            Message msg2;
            sprintf(msg2.s_payload, log_done_fmt, i);
            createMessageHeader(&msg2, DONE);
            send_multicast(&sio, &msg2);

            fprintf(logfile, log_done_fmt, i);

            receive_all(&sio, msgs, DONE);
            fprintf(logfile, log_received_all_done_fmt, i);

            exit(0);
        } else {
            SelfInputOutput sio = {io, 0};
            Message msgs[proc_count + 1];
            receive_all(&sio, msgs, STARTED);
            receive_all(&sio, msgs, DONE);
        }
    }
}

int send(void *self, local_id dst, const Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    if (sio->self == dst) return -1;

    write(sio->io.fds[sio->self][dst][1], msg, sizeof msg->s_header + msg->s_header.s_payload_len);
    return 0;
}

int send_multicast(void *self, const Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 0; i <= sio->io.procCount; ++i) {
        if (i != sio->self)
            send(self, i, msg);
    }
    return 0;
}

int receive(void *self, local_id from, Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    int fd = sio->io.fds[from][sio->self][0];
    char b[MAX_MESSAGE_LEN];
    while (1) {
        int sum, sum1;
        if ((sum = read(fd, &msg->s_header, sizeof(MessageHeader))) == -1) {
            usleep(100);
            continue;
        }
        if (msg->s_header.s_payload_len > 0) {
            sum1 = read(fd, msg->s_payload, msg->s_header.s_payload_len);
        }
        return 0;
    }
}

int receive_any(void *self, Message *msg) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    while (1) {
        for (int i = 0; i <= sio->io.procCount; ++i) {
            if (read(sio->io.fds[i][sio->self][0], msg, sizeof msg) > 0) {
                return 0;
            }
        }
    }
}

int receive_all(void *self, Message msgs[], MessageType type) {
    SelfInputOutput *sio = (SelfInputOutput *) self;
    for (int i = 1; i <= sio->io.procCount; ++i) {
        if (i == sio->self) continue;
//        do {
            receive(self, i, &msgs[i]);
            fflush(stdout);
//        } while (msgs[i].s_header.s_type != type);
    }

    return 0;
}

void createMessageHeader(Message *msg, MessageType messageType) {
    msg->s_header.s_magic = MESSAGE_MAGIC;
    msg->s_header.s_type = messageType;
    msg->s_header.s_local_time = time(NULL);
    msg->s_header.s_payload_len = strlen(msg->s_payload) + 1;
}
