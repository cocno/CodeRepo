#ifdef RCSID
static char *RCSid =
   "$Header: skgfqsbt.c 31-jan-2002.10:12:24 swerthei Exp $";
#endif /* RCSID */

/* Copyright (c) 1995, 2002, Oracle Corporation.  All rights reserved.  
All Rights Reserved 


NAME
skgfqsbt.c - SBT Disk implementation

DESCRIPTION
This file implements the SBT API version 2.0 on top of a disk filesystem.

It is used for internal testing, and is also provided to BSP vendors to
use as a reference implementation of SBT version 2.0.

This program can be compiled standalone on Solaris like this:

cc -c -o skgfqsbt.o -I. -DORA_BSP skgfqsbt.c

Or, to produce a libobk.so from this program:

cc -o libobk.so -G -I. -DORA_BSP skgfqsbt.c

PUBLIC FUNCTION(S)
see skgfqsbt.h

NOTES
The SBT API provides a service that takes a stream of data from the
API client and stores it in the specified name, and can later retrieve
it given that name.

The local filesystem provides the same type of service, so it is easy
to map the SBT functions to filesystem functions.  The intent of
this program is to show how the SBT interface should be implemented and
to provide a test implementation of SBT for internal testing.

This implementation also supports the Proxy Copy optional extensions to
the API.

We also maintain a simple media management catalog.

This implementation is intended as an example of a correct SBT API
implementation.  It is not intended to be a robust implementation that
supports high concurrency and fail-safe operation.

The I/O in this program is done using the unix functions (open, close,
etc.)  rather than the more portable C functions (fopen, fclose, etc.)
because on our base development platform, Solaris, you cannot open as many
files in a single process with the C functions as you can with the unix
functions.  Since all of the other Oracle code in the process uses the
unix functions, it is possible that this process may already have more
open files than the C functions can handle.

todo: honor trace levels according to spec ###

MODIFIED      MM/DD/YY   REASON
swerthei   01/31/02 - batch files for proxy copy READY state
sdizdar    01/25/02 - improve simulation of errors
banand     12/14/01 - bug 2149318 - use ANSI function prototypes
sdizdar    11/01/01 - add seal for SBT 1, sbtinfo2() checks for seal
swerthei   03/28/01 - add unique identifier to our files
banand     12/27/00 - Fix 1560422
banand     01/24/01 - allow writting to null device
banand     01/05/01 - disk SBT implementation always linked into Oracle.
kpatel     11/17/99 - fix 1076724: flock_t struct initialization
swerthei   06/04/99 - allow sbtinfo2 to work when file not in catalog     
swerthei   05/27/99 - sbtpccancel tracing
mluong     12/01/98 - fix conflicts in comments
rlu        11/26/98 - add a return stmt in sbtpvt_pm_add_try_again
rlu        09/02/98 - rmansdk
dbeusee    09/12/98 - misc_814
swerthei   06/04/98 -
swerthei   05/26/98 - ORASBT_PROXY_MAXFILES
swerthei   01/14/98 - SBT 2.0
swerthei   01/13/98 - fix error in sbtinfo
swerthei   01/09/98 - fix lint error
swerthei   01/02/98 - prepare for external consumption by BSP vendors
swerthei   08/18/97 - continue fix for bug 494146
dakramer   05/02/97 - bug 484146
dlomax     04/17/97 - fix compile warnings
swerthei   04/21/97 - fix lint error
swerthei   04/20/97 - fix possible strcpy overrun in sbtremove
swerthei   03/24/97 - fix BACKUP_DIR test (= -> ==) (per tpystyne)
tpystyne   02/24/97 - do not prepend full pathnames with BACKUP_DIR
tarora     04/08/96 - got rid of stdio calls (fopen doesn't work with
                   fd's over 256)
tarora     11/28/95 - replaced fprintf(stderr..) with fprintf(trcout),
                   fixed bug in sbtwrite (!maxblocks-->maxblocks)
tarora     10/20/95 - added support to return SBTWTEET for EOV testing


This program is used internally and externally, by BSP program vendors.
The ORA_BSP macro must be defined to compile this program outside of the
Oracle development environment.

defines sbt* functions as xsbt* functions. These pointers are set in 
skgfqslc() if user requested DISK implementation of SBT.
*/


#ifndef SKGFQSBT_ORACLE
#include <skgfqsbt.h>
#endif

#ifndef ORA_BSP
# include <s.h>

#define sbtopen xsbtopen
#define sbtclose xsbtclose
#define sbtread xsbtread
#define sbtwrite xsbtwrite
#define sbtremove xsbtremove
#define sbtinit xsbtinit
#define sbtinfo xsbtinfo

#else
#define STATICF static
#define CONST const
#endif



#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
# include <sys/types.h>
# include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "ncOracleConfig.h"
#include "ncDataProcessor.h"

#include <string>
#include <vector>
#include <fstream>
using namespace std;
#include <dlfcn.h>

void
addPipeLog (const char* fmt, ...)
{
	if (access ("/anyorascript/on_off.txt", 0) == 0){
		char buf[2048];
		va_list ap;
		va_start (ap, fmt);

		::vsprintf (buf, fmt, ap);

		va_end (ap);
		fstream fs ("/anyorascript/server_sbt_log.log", fstream::app | fstream::out | fstream::ate);
		fs << buf << endl;
		fs.close ();
	}
}


typedef struct flock flock_t;

#define STR_ORACLE_SYNC_FILE ("/sync_oracle_sbt")

/*****************************************************************************/
/*                       Global variables and definitions                    */
/*****************************************************************************/
/* Maximum file name length, including null terminator. */
#define SBT_FILENAME_MAX 513

/* Maximum Oracle database name length, including null terminator. */
#define SBT_DBNAME_MAX 9

/* Directory separator character.  Used by sbtpvt_tfn. */
#define SBT_DIR_STR "/"

/* Backup catalog file name.  This is translated according to the same
 * rules we use for regular backup files before opening it. */
#define SBT_CAT_NAME "Eisoo_Oracle_Disk_SBT_Catalog"

/* Catalog ID.  This string is at the beginning of the first record of the
 * catalog.  It is used as both an eyecatcher and to validate that this is
 * really a catalog file. */
#define SBT_CAT_ID "ORA_SBT_CATALOG"

/* Size of each logical record in the media management catalog. */
#define SBT_CAT_LRECL (sizeof (sbtcat))

/* Maximum error message size */
#define SBT_ERROR_MAX 1024

/* Shorthand for null pointer */
#define NP ((void*)0)

/* Null device */
#define SBT_NULLDEVICE "/dev/null"

/* Seal value that identifies a file created by this interface. */
static char sbt_seal_value[] = "ORA_SBT_FILE";

#define SBT_SEAL_LENGTH (sizeof sbt_seal_value - 1)

struct sbtglobs
{
	ncDataStreamServer*		_dataStream;

	sbtglobs() : _dataStream(0) {}
};

// static pthread_key_t g_key;

/*****************************************************************************/
/*                           Backup Catalog                                  */
/*****************************************************************************/
/* The catalog is manipulated as follows:
*   When a file is created (sbtbackup/sbtpcbackup):
*
*     Before opening the file with, look for the record in the
*     catalog.  If it is found, issue error SBT_ERROR_EXISTS.
*
*     When sbtclose2 is called, we search the catalog for a record with the
*     same name.  If found, it is overwritten.  If not found, a new record
*     is appended to the end of the file.
*
*   When a file is retrieved (sbtrestore):
*
*     Before we open the file in sbtrestore, Search for the record in the
*     catalog.  If not found, return an error.  The catalog is not updated.
*
*   When a file is deleted (sbtremove2):
*     Search for the record in the catalog.  If not found, return an error.
*     The record in the catalog is updated with the delete time.
*
* Note that the names stored in the catalog are the names passed by the
* caller, not the translated names we actually use to store the files.
* Certain functions, such as sbtinfo2, return backup file names to the
* caller, and they will expect to see the same names that they gave us.
*/

/* The first record in the backup catalog contains the following structure.  It
* is written with length SBT_CATALOG_LRECL, so that all the records in the
* catalog have the same size.  */

struct sbtcath
{
	char          sbtcath_id[20];                        /* identifier string */
};

typedef struct sbtcath sbtcath;

/* Each detail record in the backup catalog contains the following structure:
 */
struct sbtcat
{
	char          sbtcat_file_name[SBT_FILENAME_MAX];
	char          sbtcat_dbname[SBT_DBNAME_MAX];
	unsigned long sbtcat_dbid;
	time_t        sbtcat_create_time;                /* date and time created */
	time_t        sbtcat_expire_time;       /* date and time file will expire */
	time_t        sbtcat_delete_time;                /* date and time deleted */
	unsigned long sbtcat_method;            /* file creation method - has the
											 same values as SBTBFINFO_METHOD */
	unsigned long sbtcat_share;              /* file sharing method - has the
											  same values as SBTBFINFO_SHARE */
	unsigned long sbtcat_order;             /* file ordering method - has the
											  same values as SBTBFINFO_ORDER */
	char          sbtcat_label[20];                         /* 'volume' label */
	unsigned long sbtcat_block_size;                        /* only for proxy */
	unsigned long sbtcat_file_size;                         /* only for proxy */
	unsigned long sbtcat_platform_reserved_size;            /* only for proxy */
};

typedef struct sbtcat sbtcat;

/*****************************************************************************/
/* Version 1 static data.  These definitions are used only by the version    */
/* 1 functions - sbtopen, stclose, etc.  These are mutable.                  */
/*****************************************************************************/

/* File handle.  This is set to -1 when no file is open and is set to the
 * filesystem handle of the current file when a file is open.  The SBT
 * specification stipulates that the API client will not open more than one
 * file at a time.  We store this field locally so that we can:
 *  - verify that the handle passed to sbtread/sbtwrite/sbtclose is the same
 *    handle that was returned from sbtopen.
 *  - return an error if sbtopen is called to open a second file while there
 *    is already an open file.
 */

static int    sbtpvt_fh = -1;

/* Backup file block size.  This is the size of data blocks read/written to
 * the current file. */

static size_t sbtpvt_bsz;

/* Trace file name.  This holds the trace file name that was passed to
 * sbtinit. */

static char sbtpvt_trace_file[SBT_FILENAME_MAX];

/* 'tape' information returned from sbtinfo */

static char  sbtpvt_devnbr[64];
static char *sbtpvt_devret[] = {sbtpvt_devnbr, (char*)NP};

/* Pending error information. */

static unsigned long sbtpvt_error_code;
static char          sbtpvt_error_native[SBT_ERROR_MAX];
static char          sbtpvt_error_utf8[SBT_ERROR_MAX];

/* END version 1 static data */

/*****************************************************************************/
/*                             Structures                                    */
/*****************************************************************************/

/* When the API layer needs persistent (lasting more than one API call) storage
 * whose size is not known at compile-time, for many small items, or for many
 * items of variable length, we use a simple algorithm which packs the items
 * into a larger chunk of memory, and chains such chunks of memory together as
 * needed. */

/* This structure holds information about one chunk of storage allocated for
 * persistent memory required by the API layer. */
 
/*1T的数据占用的所有的SBTPM_CHUNK_SIZE为30000byte，SBTPM_ARRAY_SIZE为3000,修改以最大支持10T*/
#define SBTPM_CHUNK_SIZE /*16384*/(300000)              /* size, in bytes, of each chunk */
#define SBTPM_ARRAY_SIZE /*1000*/(30000)               /* number of char* in list array */

struct sbtpmc
{
	struct sbtpmc *sbtpmc_next;           /* pointer to next sbtpmc structure */
	char          *sbtpmc_first;          /* first available storage location */
	char          *sbtpmc_last;        /* last available storage location + 1 */
	char          *sbtpmc_avail;                   /* next available position */
};

typedef struct sbtpmc sbtpmc;

/* This structure manages one persistent memory pool. */
struct sbtpm
{
	sbtpmc  *sbtpm_sbtpmc_first;                     /* first allocated chunk */
	sbtpmc  *sbtpm_sbtpmc_current;                 /* current allocated chunk */
	void   **sbtpm_list;                         /* array of element pointers */
	void   **sbtpm_list_avail;                       /* first available entry */
	size_t   sbtpm_list_size;            /* number of void* available in list */
};

typedef struct sbtpm sbtpm;

/*****************************************************************************/
/*                             API Context                                   */
/*****************************************************************************/
struct sbtctx
{
	unsigned long sbtctx_flags;
#define SBTCTX_INIT     0x1                    /* the context is initialized */
#define SBTCTX_CREATING 0x2             /* the current file is being created */

	/* We need a permanent place to store the strings that we pass to the
	* environment. */
	sbtpm          sbtctx_env;

	/* The trace level that was specified in sbtinit2 is saved here. */
	unsigned long  sbtctx_trace_level;

	/* The process ID which was running this session.  This is used in sbtend to
	* detect if we are being called from a different process, which is possible
	* when the SBTEND_ABEND flag is set. */
#ifdef __LINUX__
	__pid_t          sbtctx_pid;
#else
	pid_t          sbtctx_pid;
#endif

	/* The next two fields store the information about some error that happened
	* in an SBT function, for subsequent retrieval by sbterror. */
	unsigned long   sbtctx_error_code;
	char            sbtctx_error_native[SBT_ERROR_MAX];
	char            sbtctx_error_utf8[SBT_ERROR_MAX];
	int             sbtctx_simerr_count;

	/* Information about the current file.  The handle is -1 when no file is
	* open. */
	char           sbtctx_file_name[SBT_FILENAME_MAX];
	int            sbtctx_file_handle;
	size_t         sbtctx_block_size;
	size_t         sbtctx_copy_number;          /* copy number from sbtbackup */
	sbtcat         sbtctx_sbtcat;          /* catalog record for current file */

	/* Catalog information */
	sbtcath       *sbtctx_sbtcath;                          /* catalog header */
	char           sbtctx_sbtcath_buf[SBT_CAT_LRECL];
	int            sbtctx_catalog_handle;              /* catalog file handle */

	/* Backup file information returned by sbtinfo2 */
	sbtpm          sbtctx_bfinfo;        /* bfinfo structs returned to caller */
	sbtpm          sbtctx_bfcat;    /* persistent storage for catalog records */

	/* Proxy copy session information. */
	int            sbtctx_proxy_batch; /* number of files to copy at one time */
	int            sbtctx_proxy_ready; /* # of files that haven't been copied */
	int            sbtctx_proxy_state;
#define SBTCTX_PROXY_NO_SESSION 0
#define SBTCTX_PROXY_NAMING     1
#define SBTCTX_PROXY_INPROGRESS 2
	int            sbtctx_proxy_type;
#define SBTCTX_PROXY_BACKUP  1
#define SBTCTX_PROXY_RESTORE 2
	sbtpm          sbtctx_proxy_files;              /* proxy file information */
	sbtpm          sbtctx_proxy_names;              /* proxy name information */
};

typedef struct sbtctx sbtctx;

/* One file in a proxy copy session. */
struct sbtpf
{
	unsigned long  sbtpf_flags;
#define SBTPF_BACKUP 0x1           /* direction is BACKUP, otherwise RESTORE */
#define SBTPF_CANCEL 0x2          /* client called sbtpccancel for this file */
	char          *sbtpf_os_file_name;                     /* filesystem name */
	char          *sbtpf_backup_file_name;       /* caller's backup file name */
	char           sbtpf_dbname[SBT_DBNAME_MAX];
	unsigned long  sbtpf_dbid;
	unsigned long  sbtpf_os_reserved_size;
	unsigned long  sbtpf_platform_reserved_size;
	unsigned long  sbtpf_block_size;
	unsigned long  sbtpf_file_size;    /* we don't know this until sbtpcstart */
	unsigned int   sbtpf_state;     /* same values as sbtpcstatus.file_status */
	unsigned int   sbtpf_laststate;
						  /* last state the client knows about for this file */
	unsigned long  sbtpf_errcode;
	char           sbtpf_errmsg[SBT_ERROR_MAX];
};

typedef struct sbtpf sbtpf;

/*****************************************************************************/
/* Version 2 function prototypes.                                            */
/*****************************************************************************/
STATICF int sbtbackup( void *ctx, unsigned long flags,
                       char *backup_file_name,
                       sbtobject *file_info,
                       size_t block_size,
                       size_t max_size,
                       unsigned int copy_number,
                       unsigned int media_pool );

STATICF int sbtclose2( void *ctx, unsigned long flags );

STATICF int sbtcommand( void *ctx, unsigned long flags, char *command );

STATICF int sbtend( void *ctx, unsigned long flags );

STATICF int sbterror( void *ctx, unsigned long flags,
                      unsigned long *error_code,
                      char **error_text_native,
                      char **error_text_utf8 );

STATICF int sbtinfo2( void *ctx, unsigned long flags,
                      char **backup_file_name_list,
                      sbtbfinfo **backup_file_info );

STATICF int sbtinit2( void *ctx, unsigned long flags,
                      sbtinit2_input *initin,
                      sbtinit2_output **initout );

STATICF int sbtread2( void *ctx, unsigned long flags, void *buf );

STATICF int sbtremove2( void *ctx, unsigned long flags,
                        char **backup_file_name_list );

STATICF int sbtrestore( void *ctx, unsigned long flags,
                        char *backup_file_name,
                        size_t block_size );

STATICF int sbtwrite2( void *ctx, unsigned long flags, void *buf );

STATICF int sbtpcbackup( void *ctx, unsigned long flags,
                         unsigned long *handle_pointer,
                         char *backup_file_name,
                         char *os_file_name,
                         sbtobject *file_info,
                         unsigned long os_reserved_size,
                         unsigned long platform_reserved_size,
                         unsigned long block_size,
                         unsigned int media_pool );

STATICF int sbtpccancel( void *ctx, unsigned long flags,
                         unsigned long handle );

STATICF int sbtpccommit( void *ctx, unsigned long flags,
                         unsigned long handle );

STATICF int sbtpcend( void *ctx, unsigned long flags );

STATICF int sbtpcquerybackup( void *ctx, unsigned long flags,
                              char *os_file_name );

STATICF int sbtpcqueryrestore( void *ctx, unsigned long flags,
                               char *backup_file_name,
                               char *os_file_name );

STATICF int sbtpcrestore( void *ctx, unsigned long flags,
                          unsigned long *handle_pointer,
                          char *backup_file_name,
                          char *os_file_name,
                          unsigned long os_reserved_size );
STATICF int sbtpcstart( void *ctx, unsigned long flags,
                        unsigned long handle,
                        unsigned long file_size );

STATICF int sbtpcstatus( void *ctx, unsigned long flags,
                         unsigned long handle,
                         unsigned long *handle_pointer,
                         unsigned int *file_status );

STATICF int sbtpcvalidate( void *ctx, unsigned long flags );

/*****************************************************************************/
/*                     Information returned by sbtinit.                      */
/*****************************************************************************/
static unsigned long sbtpvt_apivsn1   = SBT_APIVSN_SET(1,1);
static unsigned long sbtpvt_apivsn2   = SBT_APIVSN_SET(2,0);
static unsigned long sbtpvt_mmsvsn    = SBT_MMSVSN_SET(8,1,3,0);
static char          sbtpvt_mmsdesc[] = "ESO Oracle Disk API for Linux/Unix version";
static sbt_mms_fptr sbtpvt_mms_fptr   = {sbtbackup, sbtclose2, sbtcommand,
                                         sbtend, sbterror, sbtinfo2, sbtinit2,
                                         sbtread2, sbtremove2, sbtrestore,
                                         sbtwrite2, sbtpcbackup, sbtpccancel,
                                         sbtpccommit, sbtpcend,
                                         sbtpcquerybackup, sbtpcqueryrestore,
                                         sbtpcrestore, sbtpcstart,
                                         sbtpcstatus, sbtpcvalidate};

static unsigned long sbtpvt_ctxsize   = sizeof (sbtctx);
static unsigned long sbtpvt_proxy_maxfiles = 5;

static sbtinit_output sbtpvt_init[]   =
{
	{ SBTINIT_MMS_APIVSN, (void*) &sbtpvt_apivsn2 },
	{ SBTINIT_MMS_DESC,   (void*) sbtpvt_mmsdesc },
	{ SBTINIT_MMS_FPTR,   (void*) &sbtpvt_mms_fptr },
	{ SBTINIT_CTXSIZE,    (void*) &sbtpvt_ctxsize },
	{ SBTINIT_MMSVSN,     (void*) &sbtpvt_mmsvsn },
	{ SBTINIT_PROXY,      NP },
	{ SBTINIT_OUTEND,     NP }
};

static sbtinit_output sbtpvt_init_maxfiles[] =
{
	{ SBTINIT_MMS_APIVSN, (void*) &sbtpvt_apivsn2 },
	{ SBTINIT_MMS_DESC,   (void*) sbtpvt_mmsdesc },
	{ SBTINIT_MMS_FPTR,   (void*) &sbtpvt_mms_fptr },
	{ SBTINIT_CTXSIZE,    (void*) &sbtpvt_ctxsize },
	{ SBTINIT_MMSVSN,     (void*) &sbtpvt_mmsvsn },
	{ SBTINIT_PROXY,      NP },
	{ SBTINIT_PROXY_MAXFILES, (void*) &sbtpvt_proxy_maxfiles },
	{ SBTINIT_OUTEND,     NP }
};

static sbtinit_output sbtpvt_init_noproxy[] =
{
	{ SBTINIT_MMS_APIVSN, (void*) &sbtpvt_apivsn2 },
	{ SBTINIT_MMS_DESC,   (void*) sbtpvt_mmsdesc },
	{ SBTINIT_MMS_FPTR,   (void*) &sbtpvt_mms_fptr },
	{ SBTINIT_CTXSIZE,    (void*) &sbtpvt_ctxsize },
	{ SBTINIT_MMSVSN,     (void*) &sbtpvt_mmsvsn },
	{ SBTINIT_OUTEND,     NP }
};

static sbtinit_output sbtpvt_init_v1[] =
{
	{ SBTINIT_MMS_APIVSN, (void*) &sbtpvt_apivsn1 },
	{ SBTINIT_MMS_DESC,   (void*) sbtpvt_mmsdesc },
	{ SBTINIT_MMSVSN,     (void*) &sbtpvt_mmsvsn },
	{ SBTINIT_OUTEND,     NP }
};

/*****************************************************************************/
/*                          Private functions                                */
/*****************************************************************************/

/* Port-specific I/O functions. */
STATICF int   sbtpvt_open( char *file, int opflag, mode_t mode );
STATICF int   sbtpvt_open_input( sbtctx *lctx, char *name, int bkfile );
STATICF int   sbtpvt_open_output( sbtctx *lctx,char *name,
                                     int bkfile, int replace );
STATICF int   sbtpvt_check_seal( sbtctx *lctx, char *name, int fd );
STATICF int   sbtpvt_make_seal( sbtctx *lctx, char *name, int fd );
STATICF int   sbtpvt_close( int fd );
STATICF int   sbtpvt_read( int fd, void *buf, size_t block_size );
STATICF int   sbtpvt_write( int fd, void *buf, size_t block_size );
STATICF void  sbtpvt_copy( sbtctx *lctx, sbtpf *pf );
STATICF void  sbtpvt_ready( sbtctx *lctx );

/* Backup catalog manipulation functions. */
STATICF int sbtpvt_catalog_open( sbtctx *lctx );
STATICF int sbtpvt_catalog_close( sbtctx *lctx );
STATICF int sbtpvt_catalog_find( sbtctx *lctx, char *name, sbtcat *rec,
                                    unsigned long *recnum );
STATICF int sbtpvt_catalog_update( sbtctx *lctx,
                                      sbtcat *rec, unsigned long recnum );

/* Persistent memory management functions. */
STATICF void *sbtpvt_pm_add( sbtctx *lctx, sbtpm *sbtpm,
                                CONST void *item, size_t length,
                                unsigned long *index );
STATICF void sbtpvt_pm_clear( sbtctx *lctx, sbtpm *sbtpm );
STATICF void sbtpvt_pm_free( sbtctx *lctx, sbtpm *sbtpm );
STATICF sbtpmc *sbtpvt_sbtpmc_new( sbtctx *lctx );
#define sbtpvt_pm_get(pm, index) ((pm)->sbtpm_list && \
                                  (pm)->sbtpm_list + index \
                                  < (pm)->sbtpm_list_avail ? \
                                  (pm)->sbtpm_list[index] : NP)
#define sbtpvt_pm_count(pm) ((pm)->sbtpm_list ? \
                             (pm)->sbtpm_list_avail - (pm)->sbtpm_list : 0)

/* Miscellaneous functions. */
STATICF void  sbtpvt_trace(sbtctx *lctx, int type, CONST char *format, ...);
#define SBT_TRACE 0
#define SBT_ERROR 1
STATICF int   sbtpvt_tfn( sbtctx *ctx,
                             CONST char *input_name, char *output_name,
                             size_t output_name_size );
STATICF int   sbtpvt_strscat( char *dst,
                                 CONST char *src, size_t dstsize );
STATICF void  sbtpvt_sbtobject( sbtobject *obj, char *dbnamep,
                                   unsigned long *dbidp );
STATICF void  sbtpvt_error(sbtctx *lctx, unsigned long error_code,
                           CONST char *format, ...);
STATICF void  sbtpvt_pcerror( sbtctx *lctx, sbtpf *pf );
STATICF int   sbtpvt_simerr( sbtctx *lctx, CONST char *function );
STATICF void sbtpvt_lock_init( flock_t *lock, short ltype );
STATICF sbtglobs* ssgetsbtglobs ();
/* This is used to print a state in a textual form in an error message */
#define sbtpvt_textstate(s) \
(((s) >= SBTPCSTATUS_NOTREADY && (s) <= SBTPCSTATUS_ERROR) ? \
 (sbtpvt_state_names[(s)-1]) : "UNKNOWN")

/* This table is used to simulate an error status at the time that the
 * specified status would ordinarily be returned from sbtpcstatus. */
static CONST char *sbtpvt_state_names[] =
{
	"SBTPCSTATUS_NOTREADY",
	"SBTPCSTATUS_READY",
	"SBTPCSTATUS_INPROGRESS",
	"SBTPCSTATUS_END",
	"SBTPCSTATUS_DONE",
	"SBTPCSTATUS_ERROR"
};

static void
setSbtSendErrMsg (sbtctx* ctx, const char* op)
{
	char msg [256];
	sprintf (msg, "Failed to send %s message (errcode=%s)", op, dlerror ());

	//ctx->sbtctx_error_code = 0;
	sbtpvt_error (ctx, SBT_ERROR_MM, msg);
}
/*****************************************************************************/
/* Sbtinit is used by both V1 and V2 clients.                                */
/*****************************************************************************/
int         sbtinit(bserc           *se,
                    sbtinit_input   *initin,
                    sbtinit_output **initout)
{
	::addPipeLog("[%s %s] sbtinit begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int   i;
	char *version = getenv("ORASBT_MMS_VERSION");
	char *maxfiles = getenv("ORASBT_PROXY_MAXFILES");

	::addPipeLog("[%s %s] sbtinit ORACLE_SID: %s, %s, %d", __DATE__, __TIME__, getenv ("ORACLE_SID"), __FILE__, __LINE__);
	::addPipeLog("[%s %s] sbtinit process: %d, %s, %d", __DATE__, __TIME__, getpid (), __FILE__, __LINE__);

	se->bsercerrno = 0;
	sbtpvt_trace_file[0] = 0;

	if (sbtpvt_simerr((sbtctx *)NP, "sbtinit"))
	{
		se->bsercoer = SBTINIT_ERROR_SYS;
		return -1;
	}

	/* Process the input parameters. */
	for (i = 0; initin[i].i_flag != SBTINIT_INEND; i++)
	{
		switch (initin[i].i_flag)
		{
			case SBTINIT_TRACE_NAME:
			/* Copy the trace file name into our local storage. */
			strncpy(sbtpvt_trace_file, (const char *)(initin[i].i_thing),
				 sizeof sbtpvt_trace_file);

			/* It is an error if the trace file name didn't fit. */
			if (sbtpvt_trace_file[sizeof sbtpvt_trace_file - 1])
			{
				se->bsercoer = SBTINIT_ERROR_ARG;
				return -1;
			}

			default: break;                                  /* unknown init value */
		}
	}

	/* Return our information to the API client. */
	if (version && *version == '1')
		*initout = sbtpvt_init_v1;
	else if (getenv("ORASBT_PROXY_DISABLE"))
		*initout = sbtpvt_init_noproxy;
	else
	{
		if (maxfiles)
			*initout = sbtpvt_init_maxfiles;
		else
			*initout = sbtpvt_init;
	}
	::addPipeLog("[%s %s] sbtinit end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*****************************************************************************/
/* Version 2 API functions.                                                  */
/*****************************************************************************/
STATICF int sbtbackup(void           *ctx,
                      unsigned long   flags,
                      char           *backup_file_name,
                      sbtobject      *file_info,
                      size_t          block_size,
           /* This implementation does nothing with the remaining parameters */
                      size_t          max_size,
                      unsigned int    copy_number,
                      unsigned int    media_pool)
{
	::addPipeLog("[%s %s] sbtbackup begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	sbtglobs* sg = ssgetsbtglobs ();

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */


	/* If ORASBT_SIM_ERROR is set to sbtbackup_copy_2, then it means that
	* the sbtbackup for second copy fails. */
	if (sbtpvt_simerr(lctx, "sbtbackup_copy_2"))
	{
		if (copy_number == 2)
			return -1;
	}
	else if (sbtpvt_simerr(lctx, "sbtbackup"))
		return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtbackup: sbtinit2 never called");
		return -1;
	}

	/* It's an error if there is already a file open. */
	if (lctx->sbtctx_file_handle != -1)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtbackup: a file is already open");
		return -1;
	}

	/* Save the file name in the global context */
	if (strlen(backup_file_name) >= SBT_FILENAME_MAX)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtbackup: file name too long");
		return -1;
	}

	strcpy(lctx->sbtctx_file_name, backup_file_name);

	// sg->_dataStream->initHandle ();
	// sg->_dataStream->postMessage (MSG_BLOCK_SIZE, block_size);
	// sg->_dataStream->postMessage (MSG_PIECE_NAME, backup_file_name);

	/* Save the copy_number. */
	lctx->sbtctx_copy_number = copy_number;

	lctx->sbtctx_flags |= SBTCTX_CREATING;     /* turn the creating bit on */
	/* Construct a new catalog entry.  When we close
	* the file, this will also be written to the catalog. */
	memset((void*)&lctx->sbtctx_sbtcat, 0, sizeof lctx->sbtctx_sbtcat);
	strcpy(lctx->sbtctx_sbtcat.sbtcat_file_name, lctx->sbtctx_file_name);
	sbtpvt_sbtobject(file_info, lctx->sbtctx_sbtcat.sbtcat_dbname,
					&lctx->sbtctx_sbtcat.sbtcat_dbid);
	lctx->sbtctx_sbtcat.sbtcat_create_time = time((time_t *)0);
	lctx->sbtctx_sbtcat.sbtcat_method = SBTBFINFO_METHOD_STREAM;
	lctx->sbtctx_sbtcat.sbtcat_share = SBTBFINFO_SHARE_MULTIPLE;
	lctx->sbtctx_sbtcat.sbtcat_order = SBTBFINFO_ORDER_RANDOM;
	strcpy(lctx->sbtctx_sbtcat.sbtcat_label, "VOL533");

   /* Save the block size for use by sbtwrite2, and sbtclose2. */
   lctx->sbtctx_block_size = block_size;

   ::addPipeLog("[%s %s] sbtbackup end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
   return 0;
}

STATICF int sbtclose2( void *ctx, unsigned long flags )
{
	::addPipeLog("[%s %s] sbtclose2 begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	int     rc = 0;
	sbtglobs* sg = ssgetsbtglobs ();

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtclose2"))
		return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error (lctx, SBT_ERROR_MM, "sbtclose2: sbtinit2 never called");
		return -1;
	}

   /* Reset our internal state regardless of whether the close worked or not,
    * because there's not much we can do to recover from a failure to close a
    * filesystem file. */
   lctx->sbtctx_file_handle = -1;
   if (rc < 0)
   {
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtclose: error closing backup file; errno = %d", errno);
		return -1;
   }

   sg->_dataStream->postMessage (MSG_DATA_END, "");

	/* If this file was opened for output, and this is a successful close,
	* create its record in the media management catalog.  Note that this SBT
	* implementation does not check for two processes writing a backup file
	* with the same name at the same time. */
	::addPipeLog("[%s %s] sbtclose2 end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtcommand( void *ctx,
                        unsigned long flags,
                        char *command)
{
	::addPipeLog("[%s %s] sbtcommand begin command=%s, %s, %d",__DATE__,__TIME__, command, __FILE__, __LINE__);
	/* We use the command to set environment variables.  The format of the
	* command is:
	*
	*   ENV=(variable1=value1,variable2=value2,variable3=value3,...) */
	char       *pos;
	sbtctx *lctx = (sbtctx*) ctx;
	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtcommand"))
		return -1;

	sbtpvt_trace(lctx, SBT_TRACE, "sbtcommand: \"%s\"", command);

	char *env = (char *)sbtpvt_pm_add(lctx, &lctx->sbtctx_env, (void*) command,
					   strlen(command)+1, (unsigned long *)NP);

	if ((pos = strstr(env, "ENV=")))
	{
		char *lpar = strchr(pos, '('); /* pointer to the left parenthesis */
		char *rpar = strchr(pos, ')'); /* pointer to the right parenthesis */
		if (lpar && rpar && lpar < rpar)
		{
			char       *var;
			lpar++;
			*rpar = '\0';
			while ((var = strtok(lpar, ", \t")))
				{
				lpar = (char*)NP;
				if (!strchr(var, '='))
					continue;
				pos = strchr(var, '=');
				pos++;
				::addPipeLog("[%s %s] sbtcommand begin var=%s, %d, %s, %d",__DATE__,__TIME__, pos, strlen(pos),__FILE__, __LINE__);
			}
		}
	}

	::addPipeLog("[%s %s] sbtcommand end  %s, %d",__DATE__, __TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtend( void *ctx,
                    unsigned long flags)
{
	::addPipeLog("[%s %s] sbtend begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	sbtglobs* sg = ssgetsbt();

	if (sg->_dataStream != 0) {
		sg->_dataStream->closeHandle ();
		delete sg->_dataStream;
		sg->_dataStream = 0;
	}

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtend"))
		return -1;

	/* If the API was never initialized, then we have nothing to do. */
	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
		return 0;

	/* If the SBTEND_ABEND flag is set, then we may be running in some other
	* os-dispatchable unit of work than the one which originally opened the
	* file.  In this case, we call a routine to check if we are in the same
	* unit of work or not.  If not, then we can't do much to clean up after
	* ourselves.  This code using getpid() will probably not work in
	* windows-NT.  The getpid() call would have to be changed to
	* currentProcess(), and the type of sbtctx_pid would have to be chaned from
	* pid_t to int.  The resource types we're freeing here, files and memory,
	* are owned by processes, not threads, in NT, so the basic concept is still
	* valid. */
	if (flags & SBTEND_ABEND && getpid() != lctx->sbtctx_pid)
		return 0;

	/* If there is a file open, then close it.  If it was open for output, we do
	* not add a record to the catalog, because the file creation is aborted. */
	if (lctx->sbtctx_file_handle != -1)
		sbtpvt_close(lctx->sbtctx_file_handle);

	/* Free any additional storage we allocated. */
	sbtpvt_pm_free(lctx, &lctx->sbtctx_bfinfo);
	sbtpvt_pm_free(lctx, &lctx->sbtctx_bfcat);
	sbtpvt_pm_free(lctx, &lctx->sbtctx_proxy_files);
	sbtpvt_pm_free(lctx, &lctx->sbtctx_proxy_names);

	::addPipeLog("[%s %s] sbtend end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbterror( void *ctx,
                      unsigned long flags,
                      unsigned long *error_code,
                      char **error_text_native,
                      char **error_text_utf8)
{
	::addPipeLog("[%s %s] sbterror begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	//char   *utf8 = getenv("ORASBT_UTF8");
	sbtglobs* sg = ssgetsbtglobs ();

	/* For testing of the API client, the client can control whether or not
	* a utf8 error message is returned along with the native error message. */

	bool isError = false;
	if (sbtpvt_error_code)
	{
		isError = true;
		*error_code = sbtpvt_error_code;
		*error_text_native = sbtpvt_error_native;
		//if (utf8)
		*error_text_utf8 = sbtpvt_error_utf8;
		//else
		//*error_text_utf8 = NP;
	}
	else if (lctx->sbtctx_error_code)
	{
		isError = true;
		*error_code = lctx->sbtctx_error_code;
		*error_text_native = lctx->sbtctx_error_native;
		//if (utf8)
		*error_text_utf8 = lctx->sbtctx_error_utf8;
		//else
		//*error_text_utf8 = NP;
	}

	else {
		return -1;
	}

	::addPipeLog("[%s %s] sbterror end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}


STATICF int sbtinfo2( void *ctx,
                      unsigned long   flags,
                      char          **backup_file_name_list,
                      sbtbfinfo     **backup_file_info)
{
	::addPipeLog("[%s %s] sbtinfo2 begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	sbtcat *cat;
	sbtcat   catalog;
	struct  stat statbuf;
	sbtglobs* sg = ssgetsbtglobs ();

/* Shorthand macro to add one type-value pair to the array of sbtbfinfo
 * elements stored in the persistent memory object sbtctx_bfinfo */
#define addbf(lctx, type, value) \
	do \
	{ \
		sbtbfinfo *bf = \
		 (sbtbfinfo *)sbtpvt_pm_add((lctx), &(lctx)->sbtctx_bfinfo, NP, sizeof *bf, (unsigned long *)NP); \
		if (!bf) \
		 return -1; \
		bf->sbtbfinfo_type = (type); \
		bf->sbtbfinfo_value = (void*) (value); \
	} while (0)

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtinfo2"))
		return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtinfo2: sbtinit2 never called");
		return -1;
	}

	sbtpvt_pm_clear(lctx, &lctx->sbtctx_bfinfo);
	sbtpvt_pm_clear(lctx, &lctx->sbtctx_bfcat);

	for (;*backup_file_name_list; backup_file_name_list++)
	{
		::addPipeLog("[%s %s] sbtinfo2, backup_file_name = %s, %s, %d",__DATE__,__TIME__, *backup_file_name_list, __FILE__, __LINE__);
		if (!(cat = (sbtcat*)sbtpvt_pm_add(lctx, &lctx->sbtctx_bfcat, NP,
							   sizeof *cat, (unsigned long *)NP)))
			return -1;

		strcpy(catalog.sbtcat_file_name, *backup_file_name_list);
		strcpy(catalog.sbtcat_dbname, "");
		catalog.sbtcat_dbid = 0;
		catalog.sbtcat_create_time = 0;
		catalog.sbtcat_expire_time = 0;
		catalog.sbtcat_delete_time = 0;
		catalog.sbtcat_method = 1;
		catalog.sbtcat_share = 2;
		catalog.sbtcat_order = 1;
		strcpy(catalog.sbtcat_label, "");
		catalog.sbtcat_block_size = 0;
		catalog.sbtcat_file_size = 0;
		catalog.sbtcat_platform_reserved_size = 0;

		//cat非空的话就给相关的信息赋值
		if (cat)
			memcpy((void*) cat, (void*) &catalog, sizeof catalog);

		addbf(lctx, SBTBFINFO_NAME, cat->sbtcat_file_name);

		/* file is found */
		addbf(lctx, SBTBFINFO_METHOD, &cat->sbtcat_method);
		addbf(lctx, SBTBFINFO_CRETIME, &cat->sbtcat_create_time);

		if (cat->sbtcat_expire_time)
			addbf(lctx, SBTBFINFO_EXPTIME, &cat->sbtcat_expire_time);

		addbf(lctx, SBTBFINFO_LABEL, cat->sbtcat_label);

		if (cat->sbtcat_method == SBTBFINFO_METHOD_STREAM)
		{
			addbf(lctx, SBTBFINFO_SHARE, &cat->sbtcat_share);
			addbf(lctx, SBTBFINFO_ORDER, &cat->sbtcat_order);
		}

		addbf(lctx, SBTBFINFO_COMMENT, "Oracle disk API");
   }

   addbf(lctx, SBTBFINFO_END, 0);

   *backup_file_info = (sbtbfinfo*)sbtpvt_pm_get(&lctx->sbtctx_bfinfo, 0);
   ::addPipeLog("[%s %s] sbtinfo2 end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
   return 0;
#undef addbf
}

STATICF int sbtinit2( void *ctx,
                      unsigned long flags,
                      sbtinit2_input *initin,
                      sbtinit2_output **initout)
{
	::addPipeLog("[%s %s] sbtinit2 begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	int     i;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtinit2")) {
		return -1;
	}

	/* Process the input parameters. */
	for (i = 0; initin[i].sbtinit2_input_type != SBTINIT2_END; i++)
	{
		switch (initin[i].sbtinit2_input_type)
		{
		case SBTINIT2_TRACE_LEVEL:
		 /* Store the trace level. */
		 lctx->sbtctx_trace_level = *(unsigned long*) initin[i].sbtinit2_input_value;

		case SBTINIT2_CLIENT_APIVSN:
		 /* The version number of this API that is supported by the API client.
		  * There is only one 2.0 flavor of this API right now, so there is
		  * nothing we can do with this parameter.
		  */
		 break;

		case SBTINIT2_OPERATOR_NOTIFY:
		 /* This API doesn't require operator intervention.  If we did, then
		  * when this flag is seen, we should return to the client with
		  * error code SBT_ERROR_OPERATOR when we need the operator to do
		  * something like mount a new tape.
		  */
		 break;

		default: break;                                  /* unknown init value */
		}
	}

	sbtpvt_trace(lctx, SBT_TRACE,
                "sbtinit2: continuing initialization of SBT API");

	/* If this is the first time we're called, initialize any fields in the
	* handle which we need to be non-zero.  If we're called more than once in a
	* session, then we just extract whatever we need from the sbtinit_input and
	* return. */
	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		lctx->sbtctx_flags |= SBTCTX_INIT;
		lctx->sbtctx_file_handle = -1;                      /* no file is open */
		lctx->sbtctx_catalog_handle = -1;           /* the catalog is not open */
		lctx->sbtctx_sbtcath = (sbtcath*) lctx->sbtctx_sbtcath_buf;
		lctx->sbtctx_pid = getpid();
	}

	// 创建线程存储键
	pthread_key_create(&g_key, 0);
	sbtglobs* sg = ssgetsbt();

	//sg->_dataStream = new ncDataStreamServer (string (getenv ("ORACLE_SID")), to_string ((unsigned)getpid ()), to_string ((unsigned)gettid ()));
	//sg->_dataStream->sendInitId ();

	::addPipeLog("[%s %s] sbtinit2 oraclepid= %d, %s, %d",__DATE__,__TIME__,lctx->sbtctx_pid, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtread2( void *ctx,
                      unsigned long flags,
                      void *buf)
{
	//::addPipeLog("[%s %s] sbtread2 begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtread2"))
		return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtread2: sbtinit2 never called");
		return -1;
	}
   	


	/* rc > 0. We got some data from the file, so it is not end-of-file.
    * The API does not specify the behavior if the file size is not an exact
    * multiple of the buffer size (i.e. the last buffer is short).  So, we will
    * simply leave the remainder of the buffer untouched - it is up to the
    * client to deal with that case.  That case can only occur when the file is
    * read with a block size different than it was created with. */
	//::addPipeLog("[%s %s] sbtread2 end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtremove2( void *ctx,
                        unsigned long flags,
                        char **backup_file_name_list )
{
	::addPipeLog("[%s %s] sbtremove2 begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	int      ret = -1;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtremove2"))
		return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtremove2: sbtinit2 never called");
		return -1;
	}
	::addPipeLog("[%s %s] sbtremove2 end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return ret = 0;
}

STATICF int sbtrestore( void          *ctx,
                        unsigned long  flags,
                        char          *backup_file_name,
                        size_t         block_size )
{
	::addPipeLog("[%s %s] sbtrestore begin backup_file_name:%s, %s, %d",__DATE__,__TIME__, backup_file_name, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	sbtglobs* sg = ssgetsbtglobs ();

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtrestore"))
	  return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtrestore: sbtinit2 never called");
		return -1;
	}

	/* It's an error if there is already a file open. */
	if (lctx->sbtctx_file_handle != -1)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtrestore: a file is already open");
		return -1;
	}

   /* Save the file name in the global context */
   if (strlen(backup_file_name) >= SBT_FILENAME_MAX)
   {
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtrestore: file name too long");
		return -1;
	}

	strcpy(lctx->sbtctx_file_name, backup_file_name);


	// find the position of the backupset file from the anybackup catalogset.
	// it will return the catalog item.

	size_t length = strlen(backup_file_name);

	// 在备份集有多个备份片的时候，中间读取其他的备份片的时候
	// 如果同步文件没有删除，会导致无法读取下一个备份片
	string filename(STR_ORACLE_SYNC_FILE);
	remove (filename.c_str());
	if (result != 0) {
		sbtpvt_error (lctx, SBT_ERROR_MM,
			 "sbtrestore: sendMessage ORA_NEWSET failed!,result:%d",result);
		return -1;
	}
	//lctx->sbtctx_file_handle =
	  //sbtpvt_open_input(lctx, lctx->sbtctx_file_name, 1);
	lctx->sbtctx_flags &= ~SBTCTX_CREATING;   /* turn the creating bit off */

	/* Save the block size for use by sbtread2. */
	lctx->sbtctx_block_size = block_size;

	// it should wait a minute to avoid to read message sended by itself in sbtread2 function
	::addPipeLog("[%s %s] sbtrestore end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtwrite2( void *ctx,
                       unsigned long flags,
                       void *buf )
{
	//::addPipeLog("[%s %s] sbtwrite2 begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtwrite2"))
	  return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtwrite2: sbtinit2 never called");
		return -1;
	}

	// sg->_dataStream->postMessage (MSG_BUFFER_SIZE, (long) lctx->sbtctx_block_size, buf);

	//::addPipeLog("[%s %s] sbtwrite2 end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtpcbackup( void *ctx,
                         unsigned long flags,
                         unsigned long *handle_pointer,
                         char *backup_file_name,
                         char *os_file_name,
                         sbtobject *file_info,
                         unsigned long os_reserved_size,
                         unsigned long platform_reserved_size,
                         unsigned long block_size,
                         unsigned int media_pool )
{
	::addPipeLog("[%s %s] sbtpcbackup begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	sbtpf   *pf;
	unsigned long i;
	struct   stat statbuf;
	int      statrc;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpcbackup"))
		return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtpcbackup: sbtinit2 never called");
		return -1;
	}

	/* It is an error if a proxy session is already in progress. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_INPROGRESS)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcbackup: proxy copy session already active");
		return -1;
	}

	/* The client needs to know if the file to be backed up does not
	* exist. */
	while ((statrc = stat(os_file_name, &statbuf)) == -1 && errno == EINTR);
	if (statrc == -1)
	{
		if (errno == EACCES  || errno == ENOENT || errno == ENOLINK || errno == ENOTDIR)
		{
			/* file not found or permission denied. */
			sbtpvt_error(lctx, SBT_ERROR_NOTFOUND,
					  "sbtpcbackup: file %s not found, errno = %d",
					  os_file_name, errno);
		}
		else
		{
			/* some other error */
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpcbackup: I/O accessing file %s, errno = %d",
					  os_file_name, errno);
		}
		return -1;
	}

	/* The first call to sbtpcbackup/sbtpcrestore sets the direction (backup or
	* restore) for all the files being proxy copied.  Each subsequent call must
	* match the direction specified in the first call. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
	{
		lctx->sbtctx_proxy_ready = 0;
		lctx->sbtctx_proxy_state = SBTCTX_PROXY_NAMING;
		lctx->sbtctx_proxy_type = SBTCTX_PROXY_BACKUP;
		sbtpvt_pm_clear(lctx, &lctx->sbtctx_proxy_files);
		sbtpvt_pm_clear(lctx, &lctx->sbtctx_proxy_names);
	}
	else
	{
		/* session in in NAMING state */
		if (lctx->sbtctx_proxy_type != SBTCTX_PROXY_BACKUP)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpcbackup: restore proxy session already started");
			return -1;
		}
	}

	/* Allocate and initialize the file handle.  The platform_reserved_size,
	* block_size and file_size are obtained from the client if this is a
	* backup, and from our catalog is this is a restore. */
	pf = (sbtpf *)sbtpvt_pm_add(lctx, &lctx->sbtctx_proxy_files, (sbtpf*)NP, sizeof *pf, &i);
	if (!pf)
		return -1;

	memset((void*) pf, 0, sizeof *pf);
	pf->sbtpf_flags |= SBTPF_BACKUP;
	pf->sbtpf_platform_reserved_size = platform_reserved_size;
	pf->sbtpf_block_size = block_size;
	if (!(pf->sbtpf_os_file_name =
		 (char *)sbtpvt_pm_add(lctx, &lctx->sbtctx_proxy_names,
					   (void*) os_file_name,
					   strlen(os_file_name)+1, (unsigned long*)NP)))
	  return -1;
	if (!(pf->sbtpf_backup_file_name =
		 (char *)sbtpvt_pm_add(lctx, &lctx->sbtctx_proxy_names,
					   (void*) backup_file_name,
					   strlen(backup_file_name)+1, (unsigned long*)NP)))
	  return -1;
	sbtpvt_sbtobject(file_info, pf->sbtpf_dbname, &pf->sbtpf_dbid);
	pf->sbtpf_os_reserved_size = os_reserved_size;
	pf->sbtpf_state = SBTPCSTATUS_NOTREADY;

	/* Invert the handle before returning it to the user, so that valid handles
	* won't have low digits. */
	*handle_pointer = ~i;
	::addPipeLog("[%s %s] sbtpcbackup end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtpccancel( void *ctx,
                         unsigned long flags,
                         unsigned long handle )
{
	::addPipeLog("[%s %s] sbtpccancel begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	sbtpf   *pf;

	sbtpvt_trace(lctx, SBT_TRACE, "sbtpccancel: handle=%lu", handle);

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpccancel"))
		return -1;

	/* It is an error if no proxy copy session is active. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpccancel: no proxy session is active");
		return -1;
	}

	/* Get the proxy file information. */
	if (!(pf = (sbtpf*) sbtpvt_pm_get(&lctx->sbtctx_proxy_files, ~handle)))
	{
		/* Invalid handle. */
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcommit: invalid handle: %lu", handle);
		return -1;
	}

	sbtpvt_trace(lctx, SBT_TRACE, "sbtpccancel: file=%s",pf->sbtpf_os_file_name);

	pf->sbtpf_flags |= SBTPF_CANCEL;
	::addPipeLog("[%s %s] sbtpccancel end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtpccommit( void *ctx,
                         unsigned long flags,
                         unsigned long handle )
{
	::addPipeLog("[%s %s] sbtpccommit begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	sbtpf   *pf;
	sbtcat   cat;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpccommit"))
		return -1;

	/* It is an error if no proxy copy session is active. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpccommit: no proxy session is active");
		return -1;
	}

	/* Get the proxy file information. */
	if (!(pf = (sbtpf*) sbtpvt_pm_get(&lctx->sbtctx_proxy_files, ~handle)))
	{
		/* Invalid handle. */
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcommit: invalid handle: %lu", handle);
		return -1;
	}

	if (pf->sbtpf_state != SBTPCSTATUS_END)
	{
		/* Invalid status.  Status must be END before the client calls
		* sbtpccommit. */
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpccommit: file %s state is %s, not END",
				   pf->sbtpf_os_file_name, sbtpvt_textstate(pf->sbtpf_state));
		return -1;
	}

	/* The API spec says we'll only be called for files that are being backed
	* up, not for files that are being restored. */
	if (!(pf->sbtpf_flags & SBTPF_BACKUP))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpccommit: file %s is not being backed up",
				   pf->sbtpf_os_file_name);
		return -1;
	}

	/* It is an error if sbtpccancel has already been called for this file,
	* because the API spec says that no further calls will be made for a file
	* after sbtpccancel is called. */
	if (pf->sbtpf_flags & SBTPF_CANCEL)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpccommit: file %s has already been canceled",
				   pf->sbtpf_os_file_name);
		return -1;
	}

   /* Create the catalog entry for this file. */
   memset((void*) &cat, 0, sizeof cat);
   strcpy(cat.sbtcat_file_name, pf->sbtpf_backup_file_name);
   strcpy(cat.sbtcat_dbname, pf->sbtpf_dbname);
   cat.sbtcat_dbid = pf->sbtpf_dbid;
   cat.sbtcat_create_time = time((time_t *)NP);
   cat.sbtcat_method = SBTBFINFO_METHOD_PROXY;
   cat.sbtcat_share = SBTBFINFO_SHARE_MULTIPLE;
   cat.sbtcat_order = SBTBFINFO_ORDER_RANDOM;
   strcpy(cat.sbtcat_label, "3380-3");
   cat.sbtcat_block_size = pf->sbtpf_block_size;
   cat.sbtcat_file_size = pf->sbtpf_file_size;
   cat.sbtcat_platform_reserved_size = pf->sbtpf_platform_reserved_size;

   pf->sbtpf_state = SBTPCSTATUS_DONE;
   ::addPipeLog("[%s %s] sbtpccommit end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
   return 0;
}

STATICF int sbtpcend( void *ctx,
                      unsigned long flags )
{
	::addPipeLog("[%s %s] sbtpcend begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpcend"))
		return -1;

	/* The API spec says that this function must be a NOP if no proxy session is
	* active. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
		return 0;

	/* Because this implementation never has any work outstanding, the only
	* thing we must do is reset the conversation state. */
	lctx->sbtctx_proxy_state = SBTCTX_PROXY_NO_SESSION;
	::addPipeLog("[%s %s] sbtpcend end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtpcquerybackup( void *ctx,
                              unsigned long flags,
                              char *os_file_name )
{
	::addPipeLog("[%s %s] sbtpcquerybackup begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	struct  stat statbuf;
	int     statrc;
	char   *mask = getenv("ORASBT_PROXY_MASK");   /* prefix for files to fail */

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpcquerybackup"))
	  return -1;

	/* For Oracle testing, simulate an error if the file's prefix matches an
	* environment variable. */
	if (mask && !memcmp(mask, os_file_name, strlen(mask)))
	{
		sbtpvt_error(lctx, SBT_ERROR_NOPROXY,
				   "could not proxy backup file %s", os_file_name);
		return -1;
	}

   /* The only reason we wouldn't be able to open the specified file is if
    * it were not found. */
   while ((statrc = stat(os_file_name, &statbuf)) == -1 && errno == EINTR);
   if (statrc == -1)
   {
		if (errno == EACCES  || errno == ENOENT ||
		  errno == ENOLINK || errno == ENOTDIR)
		{
			 /* file not found or permission denied. */
			 sbtpvt_error(lctx, SBT_ERROR_NOTFOUND,
						  "sbtpcquerybackup: file %s not found, errno = %d",
						  os_file_name, errno);
		}
		else
		{
			 /* some other error */
			 sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpcquerybackup: I/O error accessing file %s, errno = %d",
						  os_file_name, errno);
		}
		return -1;
   }
   ::addPipeLog("[%s %s] sbtpcquerybackup end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
   return 0;
}

STATICF int sbtpcqueryrestore( void *ctx,
                               unsigned long flags,
                               char *backup_file_name,
                               char *os_file_name )
{
	::addPipeLog("[%s %s] sbtpcqueryrestore begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	char   *mask = getenv("ORASBT_PROXY_MASK");   /* prefix for files to fail */

	if (sbtpvt_simerr(lctx, "sbtpcqueryrestore"))
		return -1;

	/* For Oracle testing, simulate an error if the file's prefix matches an
	* environment variable. */
	if (mask && !memcmp(mask, os_file_name, strlen(mask)))
	{
		sbtpvt_error(lctx, SBT_ERROR_NOPROXY,
				   "could not proxy restore file %s", os_file_name);
		return -1;
	}
	::addPipeLog("[%s %s] sbtpcqueryrestore end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
}

STATICF int sbtpcrestore( void             *ctx,
                          unsigned long     flags,
                          unsigned long    *handle_pointer,
                          char             *backup_file_name,
                          char             *os_file_name,
                          unsigned long     os_reserved_size )
{
	::addPipeLog("[%s %s] sbtpcrestore begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	sbtpf   *pf;
	unsigned long i;
	sbtcat   cat;
	cat.sbtcat_block_size = 0;
	cat .sbtcat_file_size = 0;
	cat.sbtcat_platform_reserved_size = 0;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpcrestore"))
		return -1;

	if (!(lctx->sbtctx_flags & SBTCTX_INIT))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM, "sbtpcrestore: sbtinit2 never called");
		return -1;
	}

	/* It is an error if a proxy session is already in progress. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_INPROGRESS)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpcrestore: proxy copy session already active");
		return -1;
	}

	/* The first call to sbtpcbackup/sbtpcrestore sets the direction (backup or
	* restore) for all the files being proxy copied.  Each subsequent call must
	* match the direction specified in the first call. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
	{
		lctx->sbtctx_proxy_ready = 0;
		lctx->sbtctx_proxy_state = SBTCTX_PROXY_NAMING;
		lctx->sbtctx_proxy_type = SBTCTX_PROXY_RESTORE;
		sbtpvt_pm_clear(lctx, &lctx->sbtctx_proxy_files);
		sbtpvt_pm_clear(lctx, &lctx->sbtctx_proxy_names);
	}
	else
	{
		/* session in in NAMING state */
		if (lctx->sbtctx_proxy_type != SBTCTX_PROXY_RESTORE)
		{
			 sbtpvt_error(lctx, SBT_ERROR_MM,
						  "sbtpcrestore: proxy backup session already started");
			 return -1;
		}
	}

	/* Allocate and initialize the file handle.  The platform_reserved_size,
	* block_size and file_size are obtained from the client if this is a
	* backup, and from our catalog is this is a restore. */
	pf = (sbtpf *)sbtpvt_pm_add(lctx, &lctx->sbtctx_proxy_files, NP, sizeof *pf, &i);

	if (!pf)
		return -1;

	memset((void*) pf, 0, sizeof *pf);
	pf->sbtpf_platform_reserved_size = cat.sbtcat_platform_reserved_size;
	pf->sbtpf_block_size = cat.sbtcat_block_size;
	pf->sbtpf_file_size = cat.sbtcat_file_size;
	if (!(pf->sbtpf_os_file_name =
		 (char*)sbtpvt_pm_add(lctx, &lctx->sbtctx_proxy_names,
					   (void*) os_file_name,
					   strlen(os_file_name)+1, (unsigned long*)NP)))
		return -1;
	if (!(pf->sbtpf_backup_file_name =
		 (char*)sbtpvt_pm_add(lctx, &lctx->sbtctx_proxy_names,
					   (void*) backup_file_name,
					   strlen(backup_file_name)+1, (unsigned long*)NP)))
		return -1;
	pf->sbtpf_os_reserved_size = os_reserved_size;
	pf->sbtpf_state = SBTPCSTATUS_NOTREADY;

	/* Invert the handle before returning it to the user, so that valid handles
	* won't have low digits. */
	*handle_pointer = ~i;
	::addPipeLog("[%s %s] sbtpcrestore end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtpcstart( void *ctx,
                        unsigned long flags,
                        unsigned long handle,
                        unsigned long file_size )
{
	::addPipeLog("[%s %s] sbtpcstart begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx *lctx = (sbtctx*) ctx;
	sbtpf  *pf;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpcstart"))
		return -1;

	/* It is an error if no proxy copy session is active. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcstart: no proxy session is active");
		return -1;
	}

	/* It is an error if the handle is not valid.  Note that the handle we
	* return to the caller is the index, bitwise inverted. */
	if (!(pf = (sbtpf*) sbtpvt_pm_get(&lctx->sbtctx_proxy_files, ~handle)))
	{
		/* Invalid handle. */
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcstart: invalid handle: %lu", handle);
		return -1;
	}

	/* If this is a backup, use the file size provided by the client, otherwise
	* use the file size that we saved in our catalog. */
	if (pf->sbtpf_flags & SBTPF_BACKUP)
		pf->sbtpf_file_size = file_size;

	if (pf->sbtpf_state != SBTPCSTATUS_READY)
	{
		/* Invalid status.  Status must be READY before the client calls
		* sbtpcstart. */
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcstart: file %s state is %s, not READY",pf->sbtpf_os_file_name, sbtpvt_textstate(pf->sbtpf_state));
		return -1;
	}

	pf->sbtpf_state = SBTPCSTATUS_INPROGRESS;
	lctx->sbtctx_proxy_ready++;
	::addPipeLog("[%s %s] sbtpcstart end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtpcstatus( void           *ctx,
                         unsigned long   flags,
                         unsigned long   handle,
                         unsigned long  *handle_pointer,
                         unsigned int   *file_status )
{
	::addPipeLog("[%s %s] sbtpcstatus begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	unsigned long i;
	sbtpf   *pf;
	unsigned long inprogress = 0;

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpcstatus"))
		return -1;

	/* It is an error if no proxy copy session is active. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcstatus: no proxy session is active");
		return -1;
	}

	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NAMING)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcstatus: sbtpcvalidate has not been called");
		return -1;
	}

	/* Transition some files to READY state. */
	sbtpvt_ready(lctx);

	/* The client wants the status of this file immediately, with no waiting. */
	if (!(flags & SBTPCSTATUS_WAIT))
	{
		if (!(pf = (sbtpf*) sbtpvt_pm_get(&lctx->sbtctx_proxy_files, ~handle)))
		{
			 /* Invalid handle. */
			 sbtpvt_error(lctx, SBT_ERROR_MM,"sbtpcstatus: invalid handle: %lu", handle);
			 return -1;
		}
		goto sbtpcstatus_found;
	}

	/* If there are enough files to copy now, then copy some files.  When the
	* threshold is reached, we copy all files that are ready, because this
	* algorithm ensures that there will never be MORE than that many files
	* ready to copy. */
sbtpcstatus_copy:
	if (lctx->sbtctx_proxy_ready >= lctx->sbtctx_proxy_batch)
	{
		for (i = 0; i < (unsigned long)sbtpvt_pm_count(&lctx->sbtctx_proxy_files); i++)
		{
			pf = (sbtpf*) sbtpvt_pm_get(&lctx->sbtctx_proxy_files, i);

			if (pf->sbtpf_state == SBTPCSTATUS_INPROGRESS)
			{
				sbtpvt_copy(lctx, pf);
				lctx->sbtctx_proxy_ready--;
			}
		}
	}

	/* We must choose the file. */
	for (i = 0; i < (unsigned long)sbtpvt_pm_count(&lctx->sbtctx_proxy_files); i++)
	{
		pf = (sbtpf*) sbtpvt_pm_get(&lctx->sbtctx_proxy_files, i);

		if (pf->sbtpf_state == SBTPCSTATUS_INPROGRESS)
		{
			inprogress++;
			continue;
		}

		if (pf->sbtpf_state == SBTPCSTATUS_NOTREADY)
			continue;

		if (pf->sbtpf_state != pf->sbtpf_laststate &&
		  !(pf->sbtpf_flags & SBTPF_CANCEL))
		{
			 *handle_pointer = ~i;
			 goto sbtpcstatus_found;
		}
	}

	/* No files were found to return.  This is either because the client has not
	* called us to change the state of some files, or because we have some
	* files left over to copy, because the total number of files is not evenly
	* divisible by the batch size.  We'll reset the batch size now and pick up
	* the remainder of the files. */
	if (inprogress)
	{
		lctx->sbtctx_proxy_batch = 0;
		goto sbtpcstatus_copy;
	}

	/* The client already knows the state of all the files. */
	sbtpvt_error(lctx, SBT_ERROR_NOWORK,"sbtpcstatus: no new file status");
	return -1;

sbtpcstatus_found:
	if (sbtpvt_simerr(lctx, sbtpvt_textstate(pf->sbtpf_state)))
		sbtpvt_pcerror(lctx, pf);

	*file_status = pf->sbtpf_laststate = pf->sbtpf_state;

	/* If we're returning the status of a file that has had an error, then
	* call sbtpvt_error, which sets up the error codes */
	if (pf->sbtpf_state == SBTPCSTATUS_ERROR)
		sbtpvt_error(lctx, pf->sbtpf_errcode, "%s", pf->sbtpf_errmsg);
	::addPipeLog("[%s %s] sbtpcstatus end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

STATICF int sbtpcvalidate( void *ctx,
                           unsigned long flags )
{
	/* Here is where we would, if this were a real media manager, check to see
	* if any special hardware required to do the proxy is ready for use. */
	::addPipeLog("[%s %s] sbtpcvalidate begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtctx  *lctx = (sbtctx*) ctx;
	char    *batch = getenv("ORASBT_PROXY_BATCH");

	lctx->sbtctx_error_code = 0;                   /* clear any pending error */

	if (sbtpvt_simerr(lctx, "sbtpcvalidate"))
		return -1;

	/* It is an error if no proxy copy session is active. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_NO_SESSION)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpcvalidate: no proxy session is active");
		return -1;
	}

	/* It is also an error if we're already copying files. */
	if (lctx->sbtctx_proxy_state == SBTCTX_PROXY_INPROGRESS)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpcvalidate: sbtpcvalidate has already been called");
		return -1;
	}

	if (!sbtpvt_pm_count(&lctx->sbtctx_proxy_files))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				"sbtpcvalidate: no files have been specified for proxy copy");
		return -1;
	}

	/* Change the session state to 'in progress'. */
	lctx->sbtctx_proxy_state = SBTCTX_PROXY_INPROGRESS;
	lctx->sbtctx_proxy_batch = (batch && atoi(batch)) ? atoi(batch) : 1;
	::addPipeLog("[%s %s] sbtpcvalidate end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*****************************************************************************/
/* Version 1 API functions must still be supplied in version 2, so that this */
/* program can be linked with an older oracle client that uses version 1.    */
/*****************************************************************************/

/*-------------------------------- sbtopen ----------------------------------*/

int             sbtopen( bserc          *se,
                         char           *bkfilnam,
                         unsigned long   mode,
                         size_t          block_size,
                         sbtobject       bkobject[] )
{
	::addPipeLog("[%s %s] sbtopen begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int      oflag;
	int      rc;
	mode_t   lastmask;
	int      hold_errno;
	char     local_file_name[SBT_FILENAME_MAX];

	se->bsercerrno = 0;

	if (sbtpvt_simerr((sbtctx*)NP, "sbtopen"))
		return -1;

	/* It's an error if there is already a file open. */
	if (sbtpvt_fh != -1)
	{
		sbtpvt_error((sbtctx*)NP, SBTOPEIA, "sbtopen: a file is already open");
		se->bsercoer = SBTOPEIA;
		return -1;
	}

   /* Translate the file name. */
   if (sbtpvt_tfn((sbtctx*)NP, bkfilnam,
                  local_file_name, sizeof local_file_name))
   {
		se->bsercoer = SBTOPEIA;
		return -1;
   }

   if (mode & SBTOPMRD)
   {
		/* We're opening the file for input.  The file must exist. */
		oflag = O_RDONLY;
   }
   else
   {
		/* We're opening the file for output.  The O_EXCL flag is used so that
		* the open will fail if the file already exists.  The SBT specification
		* says that only one copy of a backup file with a given name can exist
		* in the media management software's catalog. */
		oflag   = O_WRONLY | O_CREAT | O_TRUNC | O_EXCL;
   }

	/* This is unix-specific file open code.  It saves the current file mask,
	* sets the mask to zero, opens the file with the correct mode and
	* permissions, and the restores the original file mask. */
	lastmask = umask((mode_t)0000);
	rc = sbtpvt_open(local_file_name, oflag, 0664);
	hold_errno = errno;
	umask(lastmask);
	if (rc == -1)
	{
		if (hold_errno == EEXIST)
		{
			/* File already exists */
			sbtpvt_error((sbtctx*)NP, SBTOPEEX,
				   "sbtopen: backup file %s already exists", local_file_name);
			se->bsercoer = SBTOPEEX;
		}
		else if ((hold_errno == ENOENT || hold_errno == EACCES)
			   && mode & SBTOPMRD)
		{
			 /* File does not exist, and we're opening the file for input. */
			 sbtpvt_error((sbtctx*)NP, SBTOPENF,
					   "sbtopen: backup file %s not found", local_file_name);
			 se->bsercoer = SBTOPENF;
		}
		else
		{
			 /* All other error conditions just return OS error, and errno. */
			 sbtpvt_error((sbtctx*)NP, SBTOPEOS,
						  "sbtopen: error opening backup file %s; errno = %d.",
						  local_file_name, errno);
			 se->bsercoer   = SBTOPEOS;
			 se->bsercerrno = hold_errno;
		}
		return -1;
	}

	if (!(mode & SBTOPMRD))
	{
		/* If this is backup, then make seal, so that we know that this is a
		* backup piece. */
		if (sbtpvt_make_seal((sbtctx*)NP, bkfilnam, rc) == -1)
		{
			se->bsercoer = SBTOPEOS;
			sbtpvt_close(rc);
			return -1;
		}
	}
	else
	{
		/* If this is restore, then check seal to verify if this is a
		* backup piece. */
		if (sbtpvt_check_seal((sbtctx*)NP, bkfilnam, rc) == -1)
		{
			se->bsercoer = SBTOPENF;
			sbtpvt_close(rc);
			return -1;
		}
	}

	/* Save the file handle and block size for use by sbtopen, sbtwrite, and
	* sbtclose. */
	sbtpvt_fh  = rc;
	sbtpvt_bsz = block_size;
	::addPipeLog("[%s %s] sbtopen end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return rc;
}

/*-------------------------------- sbtclose ---------------------------------*/
int             sbtclose( bserc          *se,
                          int             th,
                          unsigned long   flags )
{
	::addPipeLog("[%s %s] sbtclose begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int   rc;

	se->bsercerrno = 0;

	if (sbtpvt_simerr((sbtctx*)NP, "sbtclose"))
		return -1;

	/* It is an error if no file is open, or if the caller's handle does not
	* match the current file. */
	if (sbtpvt_fh == -1)
	{
		sbtpvt_error((sbtctx*)NP, SBTCLENF, "sbtclose: no file is open");
		se->bsercoer = SBTCLENF;
		return -1;
	}

	if (sbtpvt_fh != th)
	{
		sbtpvt_error((sbtctx*)NP, SBTCLENF,
				   "sbtclose: client handle does not match API handle");
		se->bsercoer = SBTCLENF;
		return -1;
	}

	rc = sbtpvt_close(sbtpvt_fh);

	/* Reset the file regardless of whether the close worked or not, because
	* there's not much we can do to recover from a failure to close a
	* filesystem file. */
	sbtpvt_fh = -1;
	if (rc < 0)
	{
		sbtpvt_error((sbtctx*)NP, SBTCLEOS,
				   "sbtclose: error closing backup file; errno = %d.", errno);
		se->bsercoer = SBTCLEOS;
		se->bsercerrno = errno;
		return -1;
	}
	::addPipeLog("[%s %s] sbtclose end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*-------------------------------- sbtread ----------------------------------*/
int     sbtread( bserc  *se,
                 int     th,
                 char   *buf )
{
	::addPipeLog("[%s %s] sbtread begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int rc;

	se->bsercerrno = 0;

	if (sbtpvt_simerr((sbtctx*)NP, "sbtread"))
		return -1;

	/* It is an error if no file is open, or if the caller's handle does not
	* match the current file. */
	if (sbtpvt_fh == -1)
	{
		sbtpvt_error((sbtctx*)NP, SBTRDENF, "sbtread: no file is open");
		se->bsercoer = SBTRDENF;
		return -1;
	}

	if (sbtpvt_fh != th)
	{
		sbtpvt_error((sbtctx*)NP, SBTRDENF,
				   "sbtread: client handle does not match API handle");
		se->bsercoer = SBTRDENF;
		return -1;
	}

	rc = sbtpvt_read(sbtpvt_fh, (void*) buf, sbtpvt_bsz);

	if (rc == 0)
	{
		/* End of file. */
		se->bsercoer = SBTRDEEF;
		return -1;
	}
	else if (rc < 0)
	{
		/* I/O error. */
		sbtpvt_error((sbtctx*)NP, SBTRDEER, "sbtread: I/O error, errno = %d", errno);
		se->bsercoer = SBTRDEER;
		se->bsercerrno = errno;
		return -1;
	}

	/* rc > 0. We got some data from the file, so it is not end-of-file.
	* The API does not specify the behavior if the file size is not an exact
	* multiple of the buffer size (i.e. the last buffer is short).  So, we will
	* simply leave the remainder of the buffer untouched - it is up to the
	* client to deal with that case.  That case can only occur when the file is
	* read with a block size different than it was created with. */
	::addPipeLog("[%s %s] sbtread end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*-------------------------------- sbtwrite ---------------------------------*/
int     sbtwrite( bserc  *se,
                  int     th,
                  char   *buf )
{
	::addPipeLog("[%s %s] sbtwrite begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	se->bsercerrno = 0;

	if (sbtpvt_simerr((sbtctx*)NP, "sbtwrite"))
		return -1;

	/* It is an error if no file is open, or if the caller's handle does not
	* match the current file. */
	if (sbtpvt_fh == -1)
	{
		sbtpvt_error((sbtctx*)NP, SBTWTENF, "sbtwrite: no file is open");
		se->bsercoer = SBTWTENF;
		return -1;
	}

	if (sbtpvt_fh != th)
	{
		sbtpvt_error((sbtctx*)NP, SBTWTENF,
				   "sbtwrite: client handle does not match API handle");
		se->bsercoer = SBTWTENF;
		return -1;
	}

	if ((unsigned int)sbtpvt_write(th, (void*) buf, sbtpvt_bsz) != sbtpvt_bsz)
	{
		/* If we could not write exactly as many bytes as requested, then there
		* was some sort of I/O error. */
		sbtpvt_error((sbtctx*)NP, SBTWTEER,
				   "sbtwrite: I/O error, errno = %d", errno);
		se->bsercoer = SBTWTEER;
		return -1;
	}
	::addPipeLog("[%s %s] sbtwrite end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*------------------------------- sbtremove ---------------------------------*/

int         sbtremove( bserc      *se,
                       char       *bkfilnam,
                       sbtobject   bkobject[] )
{
	::addPipeLog("[%s %s] sbtremove begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	char local_file_name[SBT_FILENAME_MAX];
	int  fd;

	se->bsercerrno = 0;

	if (sbtpvt_simerr((sbtctx*)NP, "sbtremove"))
		return -1;

	/* Translate the file name. */
	if (sbtpvt_tfn((sbtctx*)NP, bkfilnam,
				  local_file_name, sizeof local_file_name))
	{
		se->bsercoer = SBTRMEIA;
		return -1;
	}

	/* Don't delete files that weren't created by us.  It's not an error if
	* the file can't be opened. */
	if ((fd = sbtpvt_open_input((sbtctx*)NP, local_file_name, 1)) != -1)
	{
		if (sbtpvt_check_seal((sbtctx*)NP, local_file_name, fd) == -1)
			return -1;
		sbtpvt_close(fd);
	}

	/* Call the unix unlink function to delete the file. */
	if (strcmp(local_file_name, SBT_NULLDEVICE) && unlink(local_file_name))
	{
		if (errno == ENOENT)
		{
			 /* file not found. */
			 sbtpvt_error((sbtctx*)NP, SBTRMENF,
					   "sbtremove: backup file %s not found", local_file_name);
			 se->bsercoer = SBTRMENF;
		}
		else if (errno == ETXTBSY)
		{
			 /* file is in use. */
			 sbtpvt_error((sbtctx*)NP, SBTRMEUS,
					   "sbtremove: backup file %s in use", local_file_name);
			 se->bsercoer = SBTRMEUS;
		}
		else
		{
			 /* some other I/O error. */
			 sbtpvt_error((sbtctx*)NP, SBTRMEOS,
						  "sbtremove: error removing file %s, errno = %d",
						  local_file_name, errno);
			 se->bsercoer = SBTRMEOS;
			 se->bsercerrno = errno;
		}
		return -1;
	}
	::addPipeLog("[%s %s] sbtremove end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/* ------------------------------- sbtinfo --------------------------------- */
char      **sbtinfo( bserc      *se,
                     char       *bkfilnam,
                     sbtobject   bkobject[] )
{
	/* This function is supposed to return the name of the 'tape' that contains
	* the specified backup file.  We have no such concept, because this file
	* lives in the filesystem, so we'll return the device number instead. */
	::addPipeLog("[%s %s] sbtinfo begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int  rc;
	struct stat statbuf;
	char local_file_name[SBT_FILENAME_MAX];

	se->bsercerrno = 0;

	if (sbtpvt_simerr((sbtctx*)NP, "sbtinfo"))
		return (char **)NP;

	/* Translate the file name. */
	if (sbtpvt_tfn((sbtctx*)NP, bkfilnam,
				  local_file_name, sizeof local_file_name))
	{
		se->bsercoer = SBTIFEIA;
		return (char **)NP;
	}

	while ((rc = stat(local_file_name, &statbuf)) == -1 && errno == EINTR);
	if (rc == -1)
	{
		if (errno == EACCES)
		{
			 /* permission denied */
			 sbtpvt_error((sbtctx*)NP, SBTIFEAC,
						  "sbtinfo: backup file %s permission denied",
						  local_file_name);
			 se->bsercoer = SBTIFEAC;
		}
		if (errno == ENOENT || errno == ENOLINK || errno == ENOTDIR)
		{
			 /* file not found */
			 sbtpvt_error((sbtctx*)NP, SBTIFENF,
						  "sbtinfo: backup file %s not found", local_file_name);
			 se->bsercoer = SBTIFENF;
		}
		else
		{
			 /* some other error */
			 sbtpvt_error((sbtctx*)NP, SBTIFEOS,
						  "sbtinfo: I/O error accessing file %s, errno = %d",
						  local_file_name, errno);
			 se->bsercoer = SBTIFEOS;
			 se->bsercerrno = errno;
		}
		return (char **)NP;
	}
	else
	{
		/* Verify if the file is a really a backup piece. If file is a backup
		* piece then it has to have "seal". */
		int  fd;
		if ((fd = sbtpvt_open_input((sbtctx*)NP, local_file_name, 1)) != -1)
		{
			if (sbtpvt_check_seal((sbtctx*)NP, local_file_name, fd) == -1)
			{
				se->bsercoer = SBTIFENF;
				se->bsercerrno = errno;
				sbtpvt_close(fd);
				return (char **)NP;
			}
			sbtpvt_close(fd);
		}
		else
		{
			se->bsercoer = SBTIFENF;
			se->bsercerrno = errno;
			return (char **)NP;
		}
	}
   
	/* Return the device number. */
	sprintf(sbtpvt_devnbr, "%d", (int)statbuf.st_dev);
	::addPipeLog("[%s %s] sbtinfo end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return sbtpvt_devret;
}

/*****************************************************************************/
/* Private functions.  Some of These are used by both the version 1 and      */
/* version 2 public functions.                                               */
/*****************************************************************************/

/*------------------------------- sbtpvt_open_input -------------------------*/
/*
  NAME
    sbtpvt_open_input - sbt private open for input

  DESCRIPTION
    Opens a file for input
  RETURNS
    fd on successful; -1 when failure
  NOTES
*/
STATICF   int sbtpvt_open_input( sbtctx   *lctx,
                                 char     *name,
                                 int       bkfile )
                   /* 1 = this is a backup file, otherwise it is a user file */
{
	::addPipeLog("[%s %s] sbtpvt_open_input begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int fd;
	char tname[SBT_FILENAME_MAX];

	if (bkfile)
	{
		if (sbtpvt_tfn(lctx, name, tname, sizeof tname))
			return -1;
		name = tname;
	}

	fd = sbtpvt_open(name, O_RDONLY, 0);

	if (fd == -1)
	{
		if (errno == ENOENT || errno == EACCES)
		{
			 sbtpvt_error(lctx, SBT_ERROR_NOTFOUND,
			"sbtpvt_open_input: file %s does not exist or cannot be accessed, errno = %d",
						  name, errno);
		}
		else
		{
			 sbtpvt_error(lctx, SBT_ERROR_MM,
					"sbtpvt_open_input: file %s, error calling open(), errno = %d",
						  name, errno);
		}
	}
	::addPipeLog("[%s %s] sbtpvt_open_input end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return fd;
}

/*------------------------------- sbtpvt_open_output ------------------------*/
/*
  NAME
    sbtpvt_open_output - sbt private open for output

  DESCRIPTION
    Opens a file for output
  RETURNS
    fd on successful; -1 when failure
  NOTES
*/
STATICF int sbtpvt_open_output( sbtctx *lctx,
                                char   *name,
                                int     bkfile,
                 /* 1 = this is a backup file, otherwise this is a user file */
                                int     replace )
{
	::addPipeLog("[%s %s] sbtpvt_open_output begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	mode_t lastmask = umask((mode_t)0);
	int    fd;
	char   tname[SBT_FILENAME_MAX];

	if (bkfile)
	{
		if (sbtpvt_tfn(lctx, name, tname, sizeof tname))
			return -1;
		name = tname;
	}

	/* We're opening the file for output.  The O_EXCL flag is used so that
	* the open will fail if the file already exists.  The SBT specification
	* says that only one copy of a backup file with a given name can exist
	* in the media management software's catalog.  We have already checked
	* to make sure that the file does not exist in our catalog, using this
	* flag makes sure that we don't overwrite the file. */
	if (strcmp(name, SBT_NULLDEVICE))
		fd = sbtpvt_open(name,
					   O_WRONLY | O_CREAT | O_TRUNC | (replace ? 0 : O_EXCL),
					   0666);
	else
		fd = sbtpvt_open(name,
					   O_WRONLY | O_CREAT | O_TRUNC,
					   0666);
	  
	umask(lastmask);

	if (fd == -1)
	{
		if (errno == EEXIST)
		{
			 sbtpvt_error(lctx, SBT_ERROR_EXISTS,
						  "sbtpvt_open_output: file %s already exists", name);
		}
		else
		{
			 sbtpvt_error(lctx, SBT_ERROR_MM,
				  "sbtpvt_open_output: file %s, error calling open(), errno = %d",
						  name, errno);
		}
	}
	::addPipeLog("[%s %s] sbtpvt_open_output end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
   return fd;
}

/*------------------------------- sbtpvt_check_seal -------------------------*/
/*
  NAME
    sbtpvt_check_seal - sbt check seal on file

  DESCRIPTION
    Check the file's seal to make sure it was created by this program.
    The file must have been immediately opened, and not read from yet.
    It will be left positioned after the seal.
  RETURNS
    0 on success; -1 when failure
  NOTES
*/
STATICF   int sbtpvt_check_seal( sbtctx *lctx,
                                 char   *name,
                                 int     fd )
{
	::addPipeLog("[%s %s] sbtpvt_check_seal begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	char buf[SBT_SEAL_LENGTH];

	if (sbtpvt_read(fd, (void*)buf, (size_t) SBT_SEAL_LENGTH) !=
	   SBT_SEAL_LENGTH)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_check_seal: file %s, error reading seal, errno = %d",
				   name, errno);
		return -1;
	}
	  
	if (memcmp((void*)buf, (void*)sbt_seal_value, SBT_SEAL_LENGTH))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_check_seal: file %s, seal does not match", name);
		return -1;
	}
	::addPipeLog("[%s %s] sbtpvt_check_seal end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*------------------------------- sbtpvt_make_seal --------------------------*/
/*
  NAME
    sbtpvt_make_seal - sbt make seal on file

  DESCRIPTION
    Create a seal on the file to indicate that it was created by this program.
    The file must have been immediately opened, and not written to yet.
    It will be left positioned after the seal.
  RETURNS
    0 on success; -1 when failure
  NOTES
*/
STATICF   int sbtpvt_make_seal( sbtctx *lctx,
                                char   *name,
                                int     fd )
{
	::addPipeLog("[%s %s] sbtpvt_make_seal begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	if (sbtpvt_write(fd, (void *)sbt_seal_value, (size_t)SBT_SEAL_LENGTH) !=
	   SBT_SEAL_LENGTH)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_make_seal: file %s, error writing seal, errno = %d",
				   name, errno);
		return -1;
	}
	::addPipeLog("[%s %s] sbtpvt_make_seal end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*------------------------------- sbtpvt_open -------------------------------*/
/*
  NAME
    sbtpvt_open - sbt private open

  DESCRIPTION

  RETURNS
    fd on successful; -1 when failure
  NOTES
*/
STATICF   int sbtpvt_open( char     *file,
                           int       opflag,
                           mode_t    mode )
{
	::addPipeLog("[%s %s] sbtpvt_open begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int fd;

	while ((fd = open(file, opflag, mode)) == -1 && errno == EINTR);
	::addPipeLog("[%s %s] sbtpvt_open end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return fd;
}

/*------------------------------ sbtpvt_close -------------------------------*/
/*
  NAME
    sbtpvt_close - sbt private close function

  DESCRIPTION

  RETURNS
    0 on successful; -1 when failure
  NOTES
*/
STATICF int   sbtpvt_close( int fd )
{
	::addPipeLog("[%s %s] sbtpvt_close begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int   retval;

	while ((retval = close(fd)) < 0 && errno == EINTR);
	::addPipeLog("[%s %s] sbtpvt_close end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return retval;
}

/*-------------------------------- sbtpvt_read ------------------------------*/
/*
  NAME
    sbtpvt_read - sbt private read function

  DESCRIPTION

  RETURNS
    number of bytes read on successful, negative when failure.

  NOTES
*/
STATICF int sbtpvt_read( int             fd,
                         void           *buf,
                         size_t          block_size )
{
	int retval;

	while ((retval = read(fd, buf, block_size)) == -1 && errno == EINTR);
	return retval;
}

/*------------------------------- sbtpvt_write ------------------------------*/
/*
  NAME
    sbtpvt_write - sbt private write function

  DESCRIPTION

  RETURNS
    number of bytes written on successful, negative when failure.

  NOTES
*/
STATICF int   sbtpvt_write( int           fd,
                            void         *buf,
                            size_t        block_size )
{
	int   retval;

	while ((retval = write(fd, buf, block_size)) < 0 && errno == EINTR);
	return retval;
}

/*------------------------------- sbtpvt_copy -------------------------------*/
/*
  NAME
    sbtpvt_copy - sbt private copy function - do proxy copy of one file

  DESCRIPTION

  RETURNS

  NOTES
*/
STATICF void  sbtpvt_copy( sbtctx *lctx,
                           sbtpf  *pf )
{
	int   of;                                                     /* o/s file */
	int   in;                                                   /* input file */
	int   out;                                                 /* output file */
	char *in_name;
	char *out_name;
	int   rc;
	size_t prefix = (size_t)
	  (pf->sbtpf_platform_reserved_size + pf->sbtpf_block_size);
	int bufsize = (int) (pf->sbtpf_os_reserved_size +          /* buffer size */
						pf->sbtpf_block_size * 1000);
	void *buf = malloc((size_t)bufsize);                        /* I/O buffer */
	void *prefsave = malloc((size_t)prefix);
	unsigned long remaining = pf->sbtpf_file_size;          /* blocks to xfer */

	if (!buf)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_copy: could not malloc %d bytes", (int) bufsize);
		if (prefsave != 0){
			free (prefsave);
			prefsave = 0;
		}
		return;
	}

	if (!prefsave)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_copy: could not malloc %d bytes", (int) prefix);
		free(buf);
		buf = 0;
		return;
	}

	if (pf->sbtpf_flags & SBTPF_BACKUP)
	{
		in = of = sbtpvt_open_input(lctx, pf->sbtpf_os_file_name, 0);
		out = /*bf =*/ sbtpvt_open_output(lctx, pf->sbtpf_backup_file_name, 1, 0);
		in_name = pf->sbtpf_os_file_name;
		out_name = pf->sbtpf_backup_file_name; 
		if (sbtpvt_make_seal(lctx, out_name, out) == -1)
			out = -1;
	}
	else
	{
		out = of = sbtpvt_open_output(lctx, pf->sbtpf_os_file_name, 0, 1);
		in = /*bf = */sbtpvt_open_input(lctx, pf->sbtpf_backup_file_name, 1);
		in_name = pf->sbtpf_backup_file_name;
		out_name = pf->sbtpf_os_file_name; 
		if (sbtpvt_check_seal(lctx, in_name, in) == -1)
			in = -1;
	}

	if (in == -1 || out == -1)
	{
		sbtpvt_pcerror(lctx, pf);
		free (prefsave);
		prefsave = 0;
		free(buf);
		buf = 0;
		return;
	}

	/* Regardless of the direction (backup or restore), we don't touch the
	* os_reserved_size area of the os file. */
	if (lseek(of, (off_t) pf->sbtpf_os_reserved_size, SEEK_SET) !=
	   (off_t) pf->sbtpf_os_reserved_size)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_copy: lseek error, errno = %d", errno);
		sbtpvt_pcerror(lctx, pf);
		free (prefsave);
		prefsave = 0;
		free(buf);
		buf = 0;
		return;
	}

	rc = sbtpvt_read(in, buf, prefix);
	if ((size_t)rc != prefix)
	{
		if (rc >= 0)
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_copy: unexpected end of file for file %s",
					  in_name);
		else
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_copy: I/O error reading file %s, errno = %d",
					  in_name, errno);
		sbtpvt_pcerror(lctx, pf);
		return;
	}

	/* If this is a restore, zero out the file prefix and block 1.  They get
	* restored last, when the restore is complete.  If this is a backup,
	* copy the file prefix and block 1 before entering the main loop, so
	* that the main copy loop can handle both backups and restores. */
	if (!(pf->sbtpf_flags & SBTPF_BACKUP))
	{
		memcpy(prefsave, buf, (size_t)prefix);
		memset(buf, 0, (size_t)prefix);
	}

	if ((size_t)sbtpvt_write(out, buf, prefix) != prefix)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_copy: I/O error writing file %s, errno = %d",
				   out_name, errno);
		sbtpvt_pcerror(lctx, pf);
		return;
	}
	remaining--;

	/* Now copy the bulk of the file. */
	while (1)
	{
		unsigned long blocks = remaining > 1000 ? 1000 : remaining;
		size_t bytes = (size_t) (blocks * pf->sbtpf_block_size);

		rc = sbtpvt_read(in, buf, bytes);
		if ((size_t)rc != bytes)
		{
			if (rc >= 0)
				sbtpvt_error(lctx, SBT_ERROR_MM,
							 "sbtpvt_copy: unexpected end of file for file %s",
							 in_name);
			else
				sbtpvt_error(lctx, SBT_ERROR_MM,
							 "sbtpvt_copy: I/O error reading file %s, errno = %d",
							 in_name, errno);
			sbtpvt_pcerror(lctx, pf);
			return;
		}

		if ((size_t)sbtpvt_write(out, buf, bytes) != bytes)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_copy: I/O error writing file %s, errno = %d",
					  out_name, errno);
			sbtpvt_pcerror(lctx, pf);
			return;
		}

		if (!(remaining -= blocks))
			break;
	}

	/* We're done.  Now, if this is a restore, we restore the first block
	* of the file, as dictated by the API spec. */
	if (!(pf->sbtpf_flags & SBTPF_BACKUP))
	{
		if (lseek(out, (off_t) pf->sbtpf_os_reserved_size, SEEK_SET) !=
		  (off_t) pf->sbtpf_os_reserved_size)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_copy: lseek error, errno = %d", errno);
			sbtpvt_pcerror(lctx, pf);
			return;
		}

		if ((size_t)sbtpvt_write(out, prefsave, prefix) != prefix)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_copy: I/O error writing file %s, errno = %d",
					  out_name, errno);
			sbtpvt_pcerror(lctx, pf);
			return;
		}
	}

	sbtpvt_close(in);
	if (sbtpvt_close(out) == -1)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_copy: I/O error closing file %s, errno = %d",
				   out_name, errno);
		sbtpvt_pcerror(lctx, pf);
		return;
	}

	pf->sbtpf_state = SBTPCSTATUS_END;

	free(buf);
	free(prefsave);
}

/*------------------------------- sbtpvt_ready ------------------------------*/
/*
  NAME
    sbtpvt_ready - put a group of files into ready mode

  DESCRIPTION

  RETURNS
    number of bytes written on successful, negative when failure.

  NOTES
*/
STATICF void  sbtpvt_ready( sbtctx *lctx )
{
	/* If any files are still in READY mode, then do nothing.  If no files
	* are in READY mode, but some files are still in NOTREADY mode, then
	* change some of them to the READY state.  The number of files that
	* will be changed to the READY state defaults to 10, and can be changed
	* by the ORASBT_PROXY_READYBATCH variable. */

	char    *readybatch_c = getenv("ORASBT_PROXY_READYBATCH");
	int      readybatch = 0;
	int      i;
	sbtpf   *pf;

	if (readybatch_c)
		readybatch = atoi(readybatch_c);

	if (!readybatch)
		readybatch = 10;

	for (i = 0; i < sbtpvt_pm_count(&lctx->sbtctx_proxy_files); ++i)
	{
		pf = (sbtpf*)sbtpvt_pm_get(&lctx->sbtctx_proxy_files, i);

		if (pf->sbtpf_state == SBTPCSTATUS_READY)
			return;
	}

	for (i = 0; i < sbtpvt_pm_count(&lctx->sbtctx_proxy_files); ++i)
	{
		pf = (sbtpf*)sbtpvt_pm_get(&lctx->sbtctx_proxy_files, i);

		if (pf->sbtpf_state == SBTPCSTATUS_NOTREADY)
		{
			pf->sbtpf_state = SBTPCSTATUS_READY;

			if (!--readybatch)
				return;
		}
	}
}


/*------------------------------ sbtpvt_trace -------------------------------*/
/*
  NAME
    sbtpvt_trace - sbt private trace function

  DESCRIPTION

  RETURNS
    nothing

  NOTES
*/
STATICF void sbtpvt_trace(sbtctx *lctx, int type, CONST char *format, ...)
{
	va_list v;
	mode_t  lastmask;
	int     fd;
	char    trcmsg[1024];
	char    work[1024];
	time_t  t;
	char    timestr[64];
	flock_t lock;

	va_start(v, format);

	/* If there is no trace file, we can't trace. */
	if (!sbtpvt_trace_file[0])
		return;

	/* If the client is v2 then we must honor the trace level, otherwise the
	* fact that a trace file was specified tells us to do tracing. */
	if (lctx && !lctx->sbtctx_trace_level && type == SBT_TRACE)
		return;

	/* initialize lock structure */
	sbtpvt_lock_init (&lock, F_WRLCK);

	t = time((time_t*)NP);
	strftime(timestr, sizeof timestr, "%m/%d/%y %H:%M:%S",
			localtime(&t));

	/* Format the trace message according to the API spec. */
	vsprintf(work, format, v);
	sprintf(trcmsg, "SBT-%lu %s %s\n", (unsigned long) getpid(), timestr, work);

	/* open the trace file in append mode, print message and close file.
	* We must also lock the file before writing (see the SBT API doc).
	* */
	lastmask = umask((mode_t)0000);
	fd = sbtpvt_open(sbtpvt_trace_file, O_WRONLY | O_APPEND | O_CREAT, \
					(mode_t)0666);
	if (fd >= 0)
	{
		fcntl(fd, F_SETLKW, &lock);
		sbtpvt_write(fd, (void*)trcmsg, strlen(trcmsg));
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLKW, &lock);
		sbtpvt_close(fd);
	}
	(void) umask(lastmask);
}

/*------------------------------- sbtpvt_tfn --------------------------------*/
/*
  NAME
    sbtpvt_tfn - SBT Private Translate File Name

  DESCRIPTION
    This function receives a file name that was supplied by the API clinet and
    converts it into a format usable by this SBT API.

  RETURNS
     0: the name was translated successfully
    -1: the name could not be translated

  NOTES
*/
STATICF int sbtpvt_tfn( sbtctx     *lctx,
                        CONST char *input_name,
                        char       *output_name,
                        size_t     output_name_size )
{
	output_name[0] = '\0';                 /* initialize the output filename */

	if (memcmp((void*)input_name, (void*)SBT_DIR_STR, strlen(SBT_DIR_STR)))
	{
		char *backupdir;
		if ((backupdir = /*getenv("BACKUP_DIR")*/getenv("ORACLE_HOME")))
		{
			sbtpvt_strscat(output_name, backupdir,   output_name_size);
			sbtpvt_strscat(output_name, SBT_DIR_STR, output_name_size);
		}
		else
		{
			sbtpvt_error(lctx, 4110,
					  "sbtpvt_tfn: BACKUP_DIR environment variable not set");
			return -1;
		}
	}

	/* The prefix is constructed.  Now append the user-specified portion. */
	if (sbtpvt_strscat(output_name, input_name, output_name_size))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_tfn: backup file name exceeds limit of %d",
				   (int) output_name_size);
		return -1;
	}

	return 0;
}

/*------------------------------- sbtpvt_strscat ----------------------------*/
/*
  NAME
    sbtpvt_strscat - SBT Private safe string concatenation

  DESCRIPTION
    Safely concatenate two strings.  The copy stops when byte dst[dstlen-2] is
    filled, or when the last character of src is copied, whichever comes first,
    and then a terminating null is appended to the result.

  RETURNS
     0: no overflow
    -1: overflow

  NOTES
*/
STATICF int sbtpvt_strscat( char        *dst,
                            CONST char  *src,
                            size_t       dstsize )
{
	char   hold;
	size_t dl = strlen(dst);

	/* The destination is already overflowed. */
	if (dl >= dstsize)
		return -1;

	strncpy(dst + dl, src, dstsize - dl);

	hold = dst[dstsize - 1];
	dst[dstsize - 1] = '\0';

	return hold ? -1 : 0;
}

/*------------------------------- sbtpvt_sbtobject --------------------------*/
/*
  NAME
    sbtpvt_sbtobject - SBT Private Parse sbtobject

  DESCRIPTION
    Retrieve information from an sbtobject structure.

  RETURNS

  NOTES
*/
STATICF void sbtpvt_sbtobject( sbtobject *obj,
                               char *dbnamep,
                               unsigned long *dbidp )
{
	int i;

	for (i = 0; obj[i].o_flag != SBTOBJECT_END; i++)
	{
		if (obj[i].o_flag == SBTOBJECT_DBNAME && dbnamep)
		{
			strncpy(dbnamep, (char*) obj[i].o_thing, SBT_DBNAME_MAX);
		}

		else if (obj[i].o_flag == SBTOBJECT_DBID && dbidp)
		{
			*dbidp = *(unsigned long*) obj[i].o_thing;
		}
	}
}

/*------------------------------- sbtpvt_catalog_open -----------------------*/
/*
  NAME
    sbtpvt_catalog_open - open SBT catalog

  DESCRIPTION
    After opening, the catalog is positioned to read the first record following
    the catalog header.

  RETURNS
    0 - all OK
   -1 - error opening catalog

  NOTES
*/
STATICF int sbtpvt_catalog_open( sbtctx *lctx )
{
	::addPipeLog("[%s %s] sbtpvt_catalog_open begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	char local_file_name[SBT_FILENAME_MAX];
	flock_t lock;
	int  rc;

	/* If the catalog is already open, then most likely some other error
	* happened and was returned while the catalog was open.  Just close and
	* re-open the catalog. */
	if (lctx->sbtctx_catalog_handle != -1)
		sbtpvt_close(lctx->sbtctx_catalog_handle);

	/* Translate the file name. */
	if (sbtpvt_tfn(lctx, SBT_CAT_NAME, local_file_name,
				  sizeof local_file_name))
		return -1;

	/* Open the catalog file. */
	if ((lctx->sbtctx_catalog_handle = sbtpvt_open(local_file_name,
												  O_RDWR | O_CREAT,
												  (mode_t) 0664)) == -1)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_open: file %s, open error, errno = %d",
				   local_file_name, errno);
		return -1;
	}

	/* initialize the lock */
	sbtpvt_lock_init(&lock, F_WRLCK);

	/* Lock the file. */
	if (fcntl(lctx->sbtctx_catalog_handle, F_SETLKW, &lock) == -1)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_open: file %s, cntl error, errno = %d",
				   local_file_name, errno);
		return -1;
	}

   /* Read the header record and validate the identifier string. */
   rc = sbtpvt_read(lctx->sbtctx_catalog_handle,
                    (void*) lctx->sbtctx_sbtcath_buf, SBT_CAT_LRECL);

   if (rc == -1)
   {
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_open: read error, errno = %d", errno);
		return -1;
   }

	/* if no bytes are read, then the file is empty, so we'll create the
	* header record. */
	if (!rc)
	{
		memset((void *)(lctx->sbtctx_sbtcath_buf), 0, SBT_CAT_LRECL);
		strcpy(lctx->sbtctx_sbtcath->sbtcath_id, SBT_CAT_ID);
		if (sbtpvt_write(lctx->sbtctx_catalog_handle,
					   (void*) lctx->sbtctx_sbtcath_buf, SBT_CAT_LRECL) !=
		  SBT_CAT_LRECL)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_catalog_open: write error, errno = %d", errno);
			return -1;
		}
	}
	else
	{
		if (rc < (int)SBT_CAT_LRECL ||
		  strcmp(lctx->sbtctx_sbtcath->sbtcath_id, SBT_CAT_ID))
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_catalog_open: file %s is not an SBT catalog",
					  local_file_name);
			return -1;
		}
	}
	::addPipeLog("[%s %s] sbtpvt_catalog_open end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*------------------------------- sbtpvt_catalog_close ----------------------*/
/*
  NAME
    sbtpvt_catalog_close - close SBT catalog

  DESCRIPTION

  RETURNS
    0 - all OK
   -1 - error closing catalog

  NOTES
*/
STATICF int sbtpvt_catalog_close( sbtctx *lctx )
{
	::addPipeLog("[%s %s] sbtpvt_catalog_close begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int fd = lctx->sbtctx_catalog_handle;
	flock_t lock;

	lctx->sbtctx_catalog_handle = -1;

	if (fd == -1)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_close: catalog is not open");
		return -1;
	}

	if (lseek(fd, (off_t) 0, SEEK_SET))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_close: lseek error, errno = %d", errno);
		return -1;
	}

	/* The file is positioned to record 0.  Update the catalog header. */
	if (sbtpvt_write(fd, (void*) lctx->sbtctx_sbtcath_buf, SBT_CAT_LRECL) !=
	   SBT_CAT_LRECL)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_close: write error, errno = %d", errno);
		return -1;
	}

	/* initialize the lock */
	sbtpvt_lock_init(&lock, F_UNLCK);

	/* Unlock the file. */
	if (fcntl(fd, F_SETLKW, &lock) == -1)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_close: fcntl error, errno = %d", errno);
		return -1;
	}

	/* Close the file. */
	if (sbtpvt_close(fd))
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_close: close error, errno = %d", errno);
		return -1;
	}
	::addPipeLog("[%s %s] sbtpvt_catalog_close end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return 0;
}

/*------------------------------- sbtpvt_catalog_find -----------------------*/
/*
  NAME
    sbtpvt_catalog_find - find record in SBT catalog

  DESCRIPTION
    Look up the specified name in the SBT catalog.  If found, the position
    of the record is returned.  If not found, the position of an available
    record is returned.

  PARAMETERS
    name    (in)  - The name to look up
    rec     (in)  - The place to put the catalog record, if found
    rencum  (out) - The address of the field where the 0-based record number
                    will be returned.  This will be used on a subsequent call
                    to sbtpvt_catalog_update.  If this is zero, then the
                    record number will not be returned.

  RETURNS
    1 - record not found
    0 - record found
   -1 - error reading catalog

  NOTES
*/
STATICF int sbtpvt_catalog_find( sbtctx *lctx,
                                 char   *name,
                                 sbtcat *rec,
                                 unsigned long *recnum )
{
	::addPipeLog("[%s %s] sbtpvt_catalog_find begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	int rc;
	unsigned long currec = 0;                               /* record counter */
	unsigned long availrec = 0;                   /* available record counter */
	sbtcat   cat;
	int      mustclose = 0;
	int      ret = -1;

	if (lctx->sbtctx_catalog_handle == -1)
	{
		/* If the catalog is not already open, then open it now. */
		if (sbtpvt_catalog_open(lctx))
			return -1;
		mustclose = 1;
	}

	if (lseek(lctx->sbtctx_catalog_handle, (off_t) SBT_CAT_LRECL, SEEK_SET) !=
	   (off_t) SBT_CAT_LRECL)
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_catalog_find: lseek error, errno = %d",
				   errno);
		goto sbtpvt_catalog_find_ret;
	}

	/* The catalog is open and positioned at the first detail record. */
	while (1)
	{
		rc = sbtpvt_read(lctx->sbtctx_catalog_handle,
					   (void*) &cat, SBT_CAT_LRECL);

		if (rc == -1)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_catalog_find: read error, errno = %d",
					  errno);
			goto sbtpvt_catalog_find_ret;
		}

		/* I've reached end-of-file, and the record is not found.  Set *recnum
		* to the 0-based record number where a new record can be placed. */
		if (rc < (int)SBT_CAT_LRECL)
		{
			if (recnum)
				*recnum = availrec ? availrec : currec+1;
			ret = 1;
			goto sbtpvt_catalog_find_ret;
		}

		currec++;

		/* See if this is the one the caller is looking for. */
		if (!strncmp(cat.sbtcat_file_name, name, sizeof cat.sbtcat_file_name)
		  && !cat.sbtcat_delete_time)
		{
			/* It is the right record. */
			if (recnum)
				*recnum = currec;
			if (rec)
				memcpy((void*) rec, (void*) &cat, sizeof *rec);
			ret = 0;
			goto sbtpvt_catalog_find_ret;
		}
		else
		{
			/* It is not the right record.  If it is a record for a file that has
			* been deleted, then save its position so that we can return this
			* position if the file the user is looking for is not found.  Records
			* that represent files that were deleted more than 30 days ago are
			* eligable for reuse. */
			if (cat.sbtcat_delete_time)
				availrec = currec;
		}
	}

sbtpvt_catalog_find_ret:
	if (mustclose && sbtpvt_catalog_close(lctx))
		ret = -1;
	::addPipeLog("[%s %s] sbtpvt_catalog_find end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return ret;
}

/*------------------------------- sbtpvt_catalog_update ---------------------*/
/*
  NAME
    sbtpvt_catalog_update - update record in SBT catalog

  DESCRIPTION

  RETURNS
    0 - all OK
   -1 - error reading catalog

  NOTES
*/
STATICF int sbtpvt_catalog_update( sbtctx *lctx,
                                   sbtcat *rec,
                                   unsigned long recnum )
{
	return 0;
}

/*---------------------------------- sbtpvt_pm_add --------------------------*/
/*
  NAME
    sbtpvt_pm_add - add entry to persistent storage array

  DESCRIPTION
    There could be many thousands of files that must be returned by sbtinfo2,
    or that we must store information about during a proxy copy session,
    and we must allocate persistent storage for all of them.
    It is not efficient to 'malloc' many very small memory areas for the names,
    so we allocate memory in large chunks and fill each chunk with items
    before allocating another chunk.

    These persistent arrays also maintain a contiguous array of pointers to
    their elements, which can be returned by functions such as sbtinfo2.

    The sbtpmc structure resides at the beginning of each of the large chunks
    of memory used to hold the strings.  It holds the beginning and ending
    usable string addresses in the memory chunk and a pointer to the next
    chunk.

  RETURNS
    A pointer to the new persistent storage, or 0 for error.

  NOTES
*/
STATICF void *sbtpvt_pm_add( sbtctx     *lctx,
                             sbtpm      *sbtpm,
                             CONST void *item,
                             size_t      itemlen,
                             unsigned long *index )
{
	//::addPipeLog("[%s %s] sbtpvt_pm_add begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
   void *ret;

   if (!sbtpm->sbtpm_sbtpmc_first)
   {
      if (!(sbtpm->sbtpm_sbtpmc_first = sbtpvt_sbtpmc_new(lctx)))
         return NP;

      sbtpm->sbtpm_sbtpmc_current = sbtpm->sbtpm_sbtpmc_first;
   }

sbtpvt_pm_add_try_again:
	/* If this name is too long to fit in this chunk, then move into the next
	* chunk or allocate a new one if this is the last one. */
	if (sbtpm->sbtpm_sbtpmc_current->sbtpmc_avail + itemlen >=
	   sbtpm->sbtpm_sbtpmc_current->sbtpmc_last)
	{
		/* If the chunk hasn't been used yet, then the item is too large to
		* fit in a whole chunk.  This is an internal error. */
		if (sbtpm->sbtpm_sbtpmc_current->sbtpmc_avail ==
		  sbtpm->sbtpm_sbtpmc_current->sbtpmc_first)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_pm_add: internal error-item too long (%lu)",
					  (unsigned long)itemlen);
			return NP;
		}

		/* If this is the last allocated chunk, then allocate a new one,
		* otherwise just start using the next chunk. */
		if (!sbtpm->sbtpm_sbtpmc_current->sbtpmc_next)
		{
			if (!(sbtpm->sbtpm_sbtpmc_current->sbtpmc_next =
			   sbtpvt_sbtpmc_new(lctx)))
			return NP;
		}

		sbtpm->sbtpm_sbtpmc_current = sbtpm->sbtpm_sbtpmc_current->sbtpmc_next;
		sbtpm->sbtpm_sbtpmc_current->sbtpmc_avail = sbtpm->sbtpm_sbtpmc_current->sbtpmc_first;
		goto sbtpvt_pm_add_try_again;
	}

	/* Add this pointer to the array of pointers that is maintained in the
	* persistent memory structure.  If the pointer list hasn't been allocated
	* yet, or if there is not enough room for one more entry plus a terminating
	* null, then allocate another entry.  realloc() is not a great way to
	* manage memory, but these areas are not going to be large. */
	if (!sbtpm->sbtpm_list_size ||
	   sbtpm->sbtpm_list_avail + 1 >=
	   sbtpm->sbtpm_list + sbtpm->sbtpm_list_size)
	{
		sbtpm->sbtpm_list_size += SBTPM_ARRAY_SIZE;
		sbtpm->sbtpm_list = (void **)realloc((void*) sbtpm->sbtpm_list,
								 sbtpm->sbtpm_list_size *
													/* allocate so many ptrs */
								 sizeof *sbtpm->sbtpm_list);
		sbtpm->sbtpm_list_avail = sbtpm->sbtpm_list;

		if (!sbtpm->sbtpm_list)
		{
			sbtpvt_error(lctx, SBT_ERROR_MM,
					  "sbtpvt_pm_add: could not realloc %ld bytes",
					  sbtpm->sbtpm_list_size *
					  sizeof *sbtpm->sbtpm_list);
			return NP;
		}
	}

	ret = *sbtpm->sbtpm_list_avail = sbtpm->sbtpm_sbtpmc_current->sbtpmc_avail;
	*++sbtpm->sbtpm_list_avail = NP;

	/* Set the next available item address. */
	sbtpm->sbtpm_sbtpmc_current->sbtpmc_avail += itemlen;

	if (item)
		memcpy(ret, item, itemlen);

	if (index)
		*index = (unsigned long) (sbtpm->sbtpm_list_avail - sbtpm->sbtpm_list - 1);
	//::addPipeLog("[%s %s] sbtpvt_pm_add end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	return ret;
}

/*---------------------------------- sbtpvt_pm_clear ------------------------*/
/*
  NAME
    sbtpvt_pm_clear - clear the name list for re-use

  DESCRIPTION

  RETURNS
    0 - all OK
   -1 - some error occurred

  NOTES
*/
STATICF void sbtpvt_pm_clear( sbtctx *lctx,
                              sbtpm  *sbtpm )
{
	::addPipeLog("[%s %s] sbtpvt_pm_clear begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	sbtpm->sbtpm_sbtpmc_current = sbtpm->sbtpm_sbtpmc_first;
	if (sbtpm->sbtpm_sbtpmc_current)
		sbtpm->sbtpm_sbtpmc_current->sbtpmc_avail = sbtpm->sbtpm_sbtpmc_current->sbtpmc_first;

	if (sbtpm->sbtpm_list)
		*sbtpm->sbtpm_list = NP;
	sbtpm->sbtpm_list_avail = sbtpm->sbtpm_list;
	::addPipeLog("[%s %s] sbtpvt_pm_clear end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
}

/*---------------------------------- sbtpvt_pm_free -------------------------*/
/*
  NAME
    sbtpvt_pm_free - free the name list

  DESCRIPTION

  RETURNS
    0 - all OK
   -1 - some error occurred

  NOTES
*/
STATICF void sbtpvt_pm_free( sbtctx *lctx,
                             sbtpm  *sbtpm )
{
	sbtpmc *curr;
	sbtpmc *next;

	for (curr = sbtpm->sbtpm_sbtpmc_first;
		curr;
		curr = next)
	{
		/* Save address of the next entry now, because we're freeing the current
		* one. */
		next = curr->sbtpmc_next;
		free((void*)curr);
	}

	if (sbtpm->sbtpm_list)
		free((void*)sbtpm->sbtpm_list);
}

/*------------------------------ sbtpvt_sbtpmc_new --------------------------*/
/*
  NAME
    sbtpvt_sbtpmc_new - allocate and initialize a new chunk of storage for the
                        name list

  DESCRIPTION

  RETURNS
    0 - all OK
   -1 - some error occurred

  NOTES
*/
STATICF sbtpmc *sbtpvt_sbtpmc_new( sbtctx *lctx )
{
	sbtpmc *ret = (sbtpmc *)malloc(SBTPM_CHUNK_SIZE);

	if (ret)
	{
		memset((void*)ret, 0, sizeof *ret);
		ret->sbtpmc_avail = ret->sbtpmc_first = (char*)(ret + 1);
		ret->sbtpmc_last  = ((char*)ret) + SBTPM_CHUNK_SIZE;
	}
	else
	{
		sbtpvt_error(lctx, SBT_ERROR_MM,
				   "sbtpvt_sbtpmc_new: could not malloc %ld bytes",
				   SBTPM_CHUNK_SIZE);
	}
	return ret;
}

/*--------------------------------- sbtpvt_error ----------------------------*/
/*
  NAME
    sbtpvt_error - SBT record error information

  DESCRIPTION
    Save the error message text and error message code in the API context.
    Also log the error in the trace file.

  RETURNS

  NOTES
*/
STATICF void  sbtpvt_error(sbtctx *lctx, unsigned long error_code,
                           CONST char *format, ...)
{
	::addPipeLog("[%s %s] sbtpvt_error begin, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
	va_list v;
	char    *dest_native;
	char    *dest_utf8;

	va_start(v, format);
	if (lctx)
	{
		/* In case sbtinit returned some error, but now we're proceeding with
		* an sbt V2 session, we need to clear the static error code so that
		* sbterror retrieves further error information from the context. */
		sbtpvt_error_code = 0;
		lctx->sbtctx_error_code = error_code;
		dest_native = lctx->sbtctx_error_native;
		dest_utf8 = lctx->sbtctx_error_utf8;
	}
	else
	{
		sbtpvt_error_code = error_code;
		dest_native = sbtpvt_error_native;
		dest_utf8 = sbtpvt_error_utf8;
	}

	vsprintf(dest_native, format, v);
	sprintf(dest_utf8, "(UTF-8) %s", dest_native);

	switch (error_code)
	{
	case SBT_ERROR_EOF:
		break;
	default:
		sbtpvt_trace(lctx, SBT_ERROR, "error %lu: %s", error_code, dest_native);
	}
	::addPipeLog("[%s %s] sbtpvt_error end, %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
}

/*-------------------------------- sbtpvt_pcerror ---------------------------*/
/*
  NAME
    sbtpvt_pcerror - record error state in proxy copied file

  DESCRIPTION
    An error has previously been recorded by calling sbtpvt_error.  Take that
    information and store it in the proxy file descriptor.  Later, when the
    client calls sbtpcstatus to ask for the status of this file, we'll call
    sbtpvt_error again with the parameters we've stored in the sbptf, and
    the client will then call sbterror to retrieve the error.

  RETURNS
    nothing

  NOTES
*/
STATICF void sbtpvt_pcerror( sbtctx *lctx,
                             sbtpf  *pf )
{
	char *msg_native;
	char *msg_utf8;

	pf->sbtpf_state = SBTPCSTATUS_ERROR;
	sbterror((void*)lctx, (unsigned long) 0,\
			(unsigned long *)&pf->sbtpf_errcode, \
			&msg_native, &msg_utf8);
	strcpy(pf->sbtpf_errmsg, msg_native);
}

/*-------------------------------- sbtpvt_simerr ----------------------------*/
/*
  NAME
    sbtpvt_simerr - simulate an error from an SBT function

  DESCRIPTION

  RETURNS
    nothing

  NOTES
*/
STATICF int sbtpvt_simerr( sbtctx     *lctx,
                           CONST char *func )
{
	char *env = getenv("ORASBT_SIM_ERROR");

	if (!env || !strstr(env, func))
		return 0;

	sbtpvt_error(lctx, SBT_ERROR_MM,
				"sbtpvt_simerr: simulated error %d in function %s",
				lctx ? lctx->sbtctx_simerr_count++ : 0, func);
	return -1;
}

/* this function initializes the lock structure by setting the
 * type to ltype, and setting whence, start and len fields to
 * zero.
 * This is function helps portability as UNIX ports have
 * may not have identical flock_t structure
 */
STATICF void sbtpvt_lock_init( flock_t *lock,
                               short   ltype )
{
	lock->l_type   = ltype;
	lock->l_whence = 0;
	lock->l_start  = 0;
	lock->l_len    = 0;
}

STATICF sbtglobs* ssgetsbtglobs ()
{
	sbtglobs* globs = (sbtglobs*) pthread_getspecific (g_key);

	if (globs == 0) {
		globs = new sbtglobs;

		if (globs == 0) {
			::addPipeLog("[%s %s] ssgetsbtglobs - error !!! %s, %d",__DATE__,__TIME__, __FILE__, __LINE__);
		}

		pthread_setspecific (g_key, (void*) globs);
	}

	return globs;
}

/* end of file skgfqsbt.c */
