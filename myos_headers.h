#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h> 
#include <string.h> 

/*-- myOS and myProcess: a simple process communication server--*
 * This project defines a simple but versatile process comm.    *
 * system; the process server can accept and deliver messages   *
 * between client processes for IPC purposes, and can also      *
 * provide several process synchronization services.            *
 *                                                              *
 * The system uses a single server FIFO for the server to       *
 * accept requests from and a dedicated client FIFO for each    *
 * client process. IPC mailboxes are designated by C-string     *
 * names of up to 255 characters each                           */

/* ------------------ DEFINE FIFO FILES HERE ------------------ */
#define SERVER_FIFO "/tmp/myOS_fifo"
#define CLIENT_FIFO "/tmp/myProcess_%d_fifo"
#define FIFO_MODE 0644

/* ---------------- DEFINE SOME STANDARD SIZES ----------------- */
#define STRING_SIZE 256
#define MAX_CLIENTS 100

/* ----------------- DEFINE SYSTEM CALLS HERE ------------------ */

/*  zero-order octal digit is for connection and configuration   */

/* CONNECT initiates new connection; it takes two parameters:    *
 * - int: PID of the client process                              *
 * - C-string: mailbox name.                                     */
#define SYSCALL_CONNECT 000
/* PING checks connection status by "bouncing" a one-byte        *
 * "packet" off the server; it takes one parameter:              *
 * - byte: arbitrary number that is "bounced" back to the client */
#define SYSCALL_PING 001
/* CONFIGURE sets mailbox parameters -- the user sets as many    *
 * C-string key-value pairs as they like; takes these param's:   *
 * - byte: number of parameters to set                           *
 * - (n) C-strings with format "key:value"                       */
#define SYSCALL_CONFIGURE 003
/* DISCONNECT closes client FIFO only; it takes one parameter:   *
 * - int: PID of the client process                              */
#define SYSCALL_DISCONNECT 006
/* SHUTDOWN closes all FIFO's and ends server process; no params */
#define SYSCALL_SHUTDOWN 007

/* ----- first-order octal digit is for messaging business ----- */

/* SEND sends a message to a mailbox; it takes these parameters: *
 * - C-string: destination mailbox name                          *
 * - byte: number of strings to follow                           *
 * - (n) C-strings: the message, as enumerated by previous byte  */
#define SYSCALL_SEND 020
/* CHECK queries the server to find out how many C-strings are   *
 * waiting in its mailbox; no param's                            */
#define SYSCALL_CHECK 030
/* FETCH gets the first C-string waiting in the named mailbox;   *
 * no param's                                                    */
#define SYSCALL_FETCH 040