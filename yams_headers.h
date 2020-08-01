#ifndef IPC_H_INCLUDED
#define IPC_H_INCLUDED

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 

/* -------------------------------------------------------------------- *
 * ----------       YAMS: Yet Another Messaging System        --------- *
 * ---------- a simple IPC and process synchronization system --------- *
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
#define SERVER_FIFO_1 "YAMSD_syscall_fifo"
#define SERVER_FIFO_2 "YAMSD_comm_channel_fifo"
/* Each client gets its own "return address" in the form of a FIFO that *
 * is initialized with the client process' host OS PID                  */
#define CLIENT_FIFO "YAMS_%d_fifo"
/* the FIFO's all get the file permissions of the devil: 666            */
/* seriously, this ensures that the server and all client processes can */
/* read from and write to the necessary FIFO files                      */
#define FIFO_MODE 0666

/* -------------------- DEFINE SOME STANDARD SIZES -------------------- */
/* For the purposes of this demonstration program, a small array size   *
 * for the connected client manager and the mailbox hash table is large *
 * enough to show proof-of-concept. For a full-scale deployment that    *
 * might need to handle hundreds of processes, we might choose a much   *
 * larger number for LIST_SIZE, we might implement a hash table with    *
 * linked lists like we do for mailboxes here, or we might use a        *
 * dynamically-allocated array that we can copy into a larger array     *
 * when space gets tight, a la the implementation of vector in C++      */
#define LIST_SIZE 64

/* -------------------- DEFINE SYSTEM CALLS HERE ---------------------- */

/* unless otherwise noted, IPC server responds to all sys calls with a  *
 * single C-string status message                                       */

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

/* GETPID simply requests the client process's PID;                     *
 * (no parameters)                                                      *
 * response:                                                            *
 * - int: PID                                                           */
#define SYSCALL_GETPID 010

/* GETAGE requests the number of seconds the client has been "running"  *
 * (i.e., connected to the process server);                             *
 * (no parameters)                                                      *
 * response:                                                            *
 * - int: age of process in seconds                                     */
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

/* SEND sends a message of a set priority and type to a named mailbox   *
 *                                                                      *
 * SEND takes these parameters:                                         *
 * - C-string: destination mailbox name                                 *
 * - int: priority                                                      *
 * - int: message type                                                  *
 * - (n) C-strings: the message                                         *
 * - one empty C-string as a message terminator                         */
#define SYSCALL_SEND 020

/* CHECK queries the server to find out how many C-strings of a given   *
 * priority level and message type and from a given sender (send an     *
 * empty string to check for messages from all senders) are waiting in  *
 * its mailbox;                                                         *
 *                                                                      *
 * CHECK takes these parameters:                                        *
 * - int: priority to check for                                         *
 * - int: message type to check for                                     *
 * - C-string: sender mailbox name to check for                         */
#define SYSCALL_CHECK 021

/* RECV gets the first message of the given priority, message type, and *
 * sender mailbox waiting in the client's mailbox and removes it from   *
 * the message queue; if no qualifying message is waiting, nothing is   *
 * returned and the client blocks.                                      *
 *                                                                      *
 * RECV takes these parameters:                                         *
 * - int: priority                                                      *
 * - int: message type                                                  *
 * - C-string: sender mailbox name                                      *
 * response takes the following form                                    *
 * - int: priority                                                      *
 * - int: message type                                                  *
 * - C-string: sender mailbox name                                      *
 * - int: number of lines                                               *
 * - (n) C-strings: the message                                         */
#define SYSCALL_RECV 022

/* CONFIGURE sets mailbox parameters -- the user sets as many           *
 * C-string key-value pairs as they like; takes these param's:          *
 * - int: number of parameters to set                                   *
 * - (n) C-strings with format "key:value"                              */
#define SYSCALL_CONFIGURE 023

#endif