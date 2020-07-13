#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h> 
#include <string.h> 

/* -------------------------------------------------------------------- *
 * -------- myOS and myProcess: a simple process communication -------- *
 * --------           and synchronization system               -------- *
 * -------------------------------------------------------------------- *
 * This project defines a simple but versatile process commmunication   *
 * and synchronization system; the process server can accept and        *
 * deliver messages between client processes for IPC purposes, and      *
 * can also provide several process synchronization services.           *
 *                                                                      *
 * The system uses a pair of server FIFOs thru which the server accepts *
 * requests and system calls and a dedicated client FIFO for each       *
 * client process, using the client's host-OS PID as a naming key.      *
 * Each client process is assigned a unique PID for purposes of         *
 * identifying it within the set of client processes, and also as an    *
 * index into the array of current client records. Each client also     *
 * provides an IPC mailbox name as a C-string of up to 255 characters.  */

/* ---------------------- DEFINE FIFO FILES HERE ---------------------- */
/* The first (syscall) FIFO is used to receive system calls from client *
 * processes and then grant sole use of the second (comm_channel) FIFO  *
 * to that processs for sending IPC messages; this way 'stray' system   *
 * calls from other processes will not get mixed in with the IPC        *
 * messages being sent by the client who made the syscall we are        *
 * currently handling                                                   */ 
#define SERVER_FIFO_1 "myOS_syscall_fifo"
#define SERVER_FIFO_2 "myOS_comm_channel_fifo"
/* Each client gets its own "return address" in the form of a FIFO that *
 * is initialized with the client process' host OS PID                  */
#define CLIENT_FIFO "myProcess_%d_fifo"
/* the FIFO's all get the file permissions of the devil: 666            */
/* seriously, this ensures that the server and all client processes can */
/* read from and write to the necessary FIFO files                      */
#define FIFO_MODE 0666

/* -------------------- DEFINE SOME STANDARD SIZES -------------------- */
/* I have yet to run into a string in this program that needed to be    *
 * longer than 255 characters, and it seems like a nice round number    */
#define STRING_SIZE 256
/* For the purposes of this demonstration program, an array of 10       *
 * connected clients is large enough to show proof-of-concept. For a    *
 * full-scale deployment, we might choose a much larger number for      *
 * MAX_CLIENTS, or we might use a dynamically-allocated array that we   *
 * can copy into a larger array each time space gets tight.             */
#define MAX_CLIENTS 10

/* -------------------- DEFINE SYSTEM CALLS HERE ---------------------- */

/* - octal codes starting with 0 are for connection and disconnection - */

/* CONNECT initiates new connection; it takes two parameters:           *
 * - int: host-OS PID of the client process                             *
 * - C-string: mailbox name.                                            */
#define SYSCALL_CONNECT 000

/* PING checks connection status by "bouncing" a one-byte               *
 * "packet" off the server; it takes one parameter:                     *
 * - int: arbitrary number that is "bounced" back to the client         */
#define SYSCALL_PING 001

/* DISCONNECT closes client FIFO only; it takes one parameter:          *
 * - int: PID of the client process                                     */
#define SYSCALL_EXIT 006

/* SHUTDOWN closes all FIFO's and ends server process; no params        */
#define SYSCALL_SHUTDOWN 007


/* ------- octal codes starting with 1 are for interprocess API ------- */

/* GETPID simply requests the client process's PID; no parameters       */
#define SYSCALL_GETPID 010

/* GETAGE requests the number of seconds the client has been "running"  *
 * (i.e., connected to the process server); no parameters               */
#define SYSCALL_GETAGE 011

/* JOINPID puts a process on hold (by blocking as it waits to read a    *
 * response on its client FIFO) until the specified process EXIT's      *
 * - int: PID of process to JOIN                                        */
#define SYSCALL_JOINPID 012

/* WAIT puts a process on hold (by blocking as it waits to read a       *
 * response on its client FIFO) until the specified process responds    *
 * with a SIGNAL call                                                   *
 * - int: PID of process to WAIT for                                    */
#define SYSCALL_WAIT 013

/* SIGNAL sends a message to a WAITing process                          *
 * - int: PID of process to SIGNAL to                                   */
#define SYSCALL_SIGNAL 014


/* ---- octal codes starting with 2 are for interprocess messaging ---- */

/* SEND sends a message to a mailbox; it takes these parameters:        *
 * - C-string: destination mailbox name                                 *
 * - int: number of strings to follow                                   *
 * - (n) C-strings: the message, as enumerated by previous byte         */
#define SYSCALL_SEND 020

/* CHECK queries the server to find out how many C-strings are          *
 * waiting in its mailbox; no param's                                   */
#define SYSCALL_CHECK 021

/* FETCH gets the first C-string waiting in the named mailbox;          *
 * no param's                                                           */
#define SYSCALL_FETCH 022

/* CONFIGURE sets mailbox parameters -- the user sets as many           *
 * C-string key-value pairs as they like; takes these param's:          *
 * - int: number of parameters to set                                   *
 * - (n) C-strings with format "key:value"                              */
#define SYSCALL_CONFIGURE 023