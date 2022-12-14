
// fcntl.h

#ifndef _FCNTL_H
#define _FCNTL_H    1

#include <sys/types.h>

/* These values are used for cmd in fcntl().  POSIX Table 6-1.  */
#define F_DUPFD            0	/* duplicate file descriptor */
#define F_GETFD            1	/* get file descriptor flags */
#define F_SETFD            2	/* set file descriptor flags */
#define F_GETFL            3	/* get file status flags */
#define F_SETFL            4	/* set file status flags */
#define F_GETLK            5	/* get record locking information */
#define F_SETLK            6	/* set record locking information */
#define F_SETLKW           7	/* set record locking info; wait if blocked */
#define F_FREESP           8	/* free a section of a regular file */

/* File descriptor flags used for fcntl().  POSIX Table 6-2. */
#define FD_CLOEXEC         1	/* close on exec flag for third arg of fcntl */

/* L_type values for record locking with fcntl().  POSIX Table 6-3. */
#define F_RDLCK            1	/* shared or read lock */
#define F_WRLCK            2	/* exclusive or write lock */
#define F_UNLCK            3	/* unlock */


//
// O_
//

// 'O_' flags.
// #atention:
// It needs to have same value found in the kernel.


/* File access modes for open() and fcntl().  POSIX Table 6-6. */
#define O_RDONLY    0  /* open(name, O_RDONLY) opens read only */
#define O_WRONLY    1  /* open(name, O_WRONLY) opens write only */
#define O_RDWR      2  /* open(name, O_RDWR) opens read/write */

/* Mask for use with file access modes.  POSIX Table 6-7. */
#define O_ACCMODE    03  /* mask for file access modes */

// #define	O_NONBLOCK	0x00000004	/* no delay */
// #define	O_APPEND	0x00000008	/* set append mode */
//#define	O_SHLOCK	0x00000010	/* open with shared file lock */
//#define	O_EXLOCK	0x00000020	/* open with exclusive file lock */
//#define  O_ASYNC  0x00000040	/* signal pgrp when data ready */
// #define	O_SYNC		0x00000080	/* synchronous writes */


/* don't follow symlinks on the last */
/* path component */
//#define	O_NOFOLLOW	0x00000100	


/* Oflag values for open().  POSIX Table 6-4. */
#define O_CREAT     00100  /* creat file if it doesn't exist */
#define O_EXCL      00200  /* exclusive use flag */
#define O_NOCTTY    00400  /* do not assign a controlling terminal */
#define O_TRUNC     01000  /* truncate flag */

/* File status flags for open() and fcntl().  POSIX Table 6-5. */
#define O_APPEND       02000	/* set append mode */
#define O_NONBLOCK     04000	/* no delay */
/* automatically re-open device after driver restart */
#define O_REOPEN      010000


//#define	O_ALT_IO	0x00040000	/* use alternate i/o semantics */
//#define	O_DIRECT	0x00080000	/* direct I/O hint *


//#todo
//#define	O_DIRECTORY	0x00200000	/* fail if not a directory */
//#define	O_CLOEXEC	0x00400000	/* set close on exec */

/*
  Credits: Serenity OS.
 
#define S_IFMT 0170000
#define S_IFDIR 0040000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFREG 0100000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IFSOCK 0140000
#define S_ISUID 04000
#define S_ISGID 02000
#define S_ISVTX 01000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#define S_IRWXG (S_IRWXU >> 3)
#define S_IRWXO (S_IRWXG >> 3) 
*/

/* Struct used for locking.  POSIX Table 6-8. */
struct flock 
{
    short l_type;    // type: F_RDLCK, F_WRLCK, or F_UNLCK 
    short l_whence;  // flag for starting offset 
    off_t l_start;   // relative offset in bytes 
    off_t l_len;     // size; if 0, then until EOF 
    pid_t l_pid;     // process id of the locks' owner 
};

// =====================================

int fallocate(int fd, int mode, off_t offset, off_t len);
int posix_fallocate(int fd, off_t offset, off_t len);

int fcntl(int fd, int cmd, ...);
int openat(int dirfd, const char *pathname, int flags);
int open(const char *pathname, int flags, mode_t mode);
int creat(const char *pathname, mode_t mode);




#endif /* _FCNTL_H */

//
// End.
//






