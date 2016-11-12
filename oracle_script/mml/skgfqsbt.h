/*
   $Header: skgfqsbt.h 14-dec-2001.10:23:21 banand Exp $
   Copyright (c) 1995, 2001, Oracle Corporation.  All rights reserved.  
   All Rights Reserved.

  NAME
    SBT - API for Backup/Restore to/from sequential media
  DESCRIPTION
    This interface defines a set of logical operations to do backup/restore.
    The functions used and relevant defines and structs are defined here.

    For a detailed spec of the SBT API, please refer to the Media Management 
    API doc.
  MODIFIED    (MM/DD/YY)
    banand     12/14/01 - bug 2149318 - use ANSI function prototypes
    banand     04/10/01 - fix 1724690
    banand     02/05/01 - define oracle's disk sbt driver name
    swerthei   03/27/98 - more SBT 2.0 changes
    swerthei   03/24/98 - update for modified SBT spec
    swerthei   02/20/98 - continue SBT 2.0 reference implementation
    swerthei   01/14/98 - update for SBT version 2
    swerthei   08/07/97 - change error codes to conform to v1.1 spec
    tarora     05/23/96 - add a constant defn
    tarora     10/20/95 - added a note about SBTWTEET
    tarora     09/20/95 - cosmetic changes
    tarora     09/14/95 - added missing #include types.h
    tarora     01/20/94 - Modified to conform to new tape API (ver 1.1)
    ptsui      06/30/93 - Modified to conform to new tape API (ver 1.0)
    ptsui      07/17/92 - Creation
*/
#ifdef SBT_V1_FPTR

/* undefine macros if defined earlier by skgfqsbt.h */
#undef sbtinit
#undef sbtopen
#undef sbtread
#undef sbtwrite
#undef sbtclose
#undef sbtinfo
#undef sbtremove
/* define function points which will be resolved during link phase.
* duplicating the function prototype definitions to avoid lint
* warnings.
*/
int sbtinit( bserc *brse, sbtinit_input *initin,
          sbtinit_output **initout );
int sbtopen( bserc *brse, char *backup_file_name,
          unsigned long mode, size_t block_size,
           sbtobject *file_info );
int sbtread( bserc *brse, int th, char *buf );
int sbtwrite( bserc *brse, int th, char *buf );
int sbtclose( bserc *brse, int th, unsigned long flags );
char **sbtinfo( bserc *brse, char *backup_file_name,
              sbtobject *file_info );
int sbtremove( bserc *brse, char *backup_file_name,
            sbtobject *file_info );

#endif

#ifndef SKGFQSBT_ORACLE
#define SKGFQSBT_ORACLE

#include <stddef.h>

/***********************************************************************/
/* SBT size limits.  All are in units of bytes.  When the object is a  */
/* character string, the maximum length includes the terminating null. */
/***********************************************************************/

#define SBT_ERROR_MAX   1024      /* error string returned by sbterror */
#define SBT_FILE_MAX    512                        /* backup file name */
#define SBT_MMSDESC_MAX 64         /* SBT_MMS_DESC returned by sbtinit */
#define SBT_CONTEXT_MAX 1048576                         /* API context */
#define SBT_DBNAME_MAX  9                          /* SBTOBJECT_DBNAME */
#define SBT_COMMENT_MAX 80                        /* SBTBFINFO_COMMENT */
#define SBT_LABEL_MAX   64                          /* SBTBFINFO_LABEL */
#define SBT_COMMAND_MAX 512            /* command passed to sbtcommand */

/***********************************************************************/
/* Database Object being backed up or restored.                        */
/***********************************************************************/

struct sbtobject
{
	unsigned long o_flag;                         /* Type of the object */
#define SBTOBJECT_DBNAME       2                      /* Database Name */
#define SBTOBJECT_DBID         3                        /* Database ID */
#define SBTOBJECT_FILETYPE     5                          /* file type */
#define SBTOBJECT_END         32                        /* Null Object */
	void *o_thing;
/* values for SBTOBJECT_FILETYPE */
#define SBTOBJECT_DATAFILE      1                          /* datafile */
#define SBTOBJECT_ARCHLOG       2                 /* archived redo log */
#define SBTOBJECT_OTHER         3 /* neither datafile nor archived log */

};
typedef struct sbtobject sbtobject;

/***********************************************************************/
/* These #defines are obsolete - do not assign the same values to      */
/* new constants.                                                      */
/* Discontinued with API Spec Version 1.1:                             */
/*   #define SBTOBJECT_HOST 1 Host Name                                */
/*   #define SBTOBJECT_SID  4 Oracle SID of Database                   */
/*                                                                     */
/* Discontinued with API Spec Version 2.0                              */
#define SBTOBJECT_TSNAME       8   /* Tablespace Name                  */
#define SBTOBJECT_FILENAME    16   /* File Name                        */
#define SBTOBJECT_DOMAIN      64   /* Domain Name                      */
/***********************************************************************/

/***********************************************************************/
/* Initialization information passed to sbtinit by the API client.     */
/***********************************************************************/

struct sbtinit_input
{
	unsigned long i_flag;                      /* Type of the object */
#define SBTINIT_INEND       1                           /* Null Object */
#define SBTINIT_TRACE_NAME  2                       /* Trace File Name */
	void *i_thing;                          /* Initialization Object */
};
typedef struct sbtinit_input sbtinit_input;

/***********************************************************************/
/* Initialization information returned to the API client from sbtinit  */
/***********************************************************************/

struct sbtinit_output
{
	unsigned long o_flag;                      /* Type of the object */
#define SBTINIT_OUTEND          1                       /* Null Object */
#define SBTINIT_SIGNAL          2             /* Signal Initialization */
#define SBTINIT_MAXSIZE         3          /* maximum backup file size */
#define SBTINIT_MMS_APIVSN      4        /* Version Number of Tape API */
#define SBTINIT_MMS_DESC        5                /* vendor description */
#define SBTINIT_MMS_FPTR        6                 /* function pointers */
#define SBTINIT_CTXSIZE         7                 /* context area size */
#define SBTINIT_MMSVSN          8     /* Version Number of MM Software */
#define SBTINIT_PROXY           9           /* API supports proxy copy */
#define SBTINIT_PROXY_MAXFILES  10  /* Maximum concurrent proxy copies */
	void *o_thing;                          /* Initialization Object */
};
typedef struct sbtinit_output sbtinit_output;

/***********************************************************************/
/* Initialization information passed to sbtinit2 by the API client     */
/***********************************************************************/
struct sbtinit2_input
{
	unsigned long sbtinit2_input_type;    /* Type of the object */
#define SBTINIT2_TRACE_LEVEL       1  /* Trace level */
#define SBTINIT2_CLIENT_APIVSN     2  /* API version supported by client */
#define SBTINIT2_OPERATOR_NOTIFY   3  /* API client can notify operator */
#define SBTINIT2_END            9999  /* null entry - end of list */
	void *sbtinit2_input_value;           /* Initialization Object */
};
typedef struct sbtinit2_input sbtinit2_input;

/***********************************************************************/
/* Initialization information returned by sbtinit2                     */
/***********************************************************************/

struct sbtinit2_output
{
	unsigned long sbtinit2_output_type;  /* Type of the object */
	void *sbtinit2_output_value;         /* Initialization Object */
};
typedef struct sbtinit2_output sbtinit2_output;

/***********************************************************************/
/* Backup file information                                             */
/***********************************************************************/

struct sbtbfinfo
{
	unsigned long sbtbfinfo_type;        /* Type of the object */
#define SBTBFINFO_NAME     1            /* name of backup file */
#define SBTBFINFO_METHOD   2            /* creation method */
#define SBTBFINFO_CRETIME  3            /* file creation date/time */
#define SBTBFINFO_EXPTIME  4            /* file expiration date/time */
#define SBTBFINFO_LABEL    5            /* Volume label */
#define SBTBFINFO_SHARE    6            /* media sharing mode */
#define SBTBFINFO_ORDER    7            /* file ordering mode */
#define SBTBFINFO_NOTFOUND 8            /* file not found */
#define SBTBFINFO_COMMENT  9            /* vendor-defined comment string */
#define SBTBFINFO_END      9999         /* end of array */
	void *sbtbfinfo_value;               /* value of the object */

/* values for sharing mode */
#define SBTBFINFO_SHARE_SINGLE   1      /* one user at a time */
#define SBTBFINFO_SHARE_MULTIPLE 2      /* multiple concurrent users */

/* values for access mode */
#define SBTBFINFO_ORDER_SEQ    1        /* should be accessed sequentially */
#define SBTBFINFO_ORDER_RANDOM 2        /* can be accessed randomly */

/* values for creation method */
#define SBTBFINFO_METHOD_STREAM 1       /* regular backup file */
#define SBTBFINFO_METHOD_PROXY  2       /* proxy copied file */

};
typedef struct sbtbfinfo sbtbfinfo;

/***********************************************************************/
/* Pointers to functions provided by the Media Management software     */
/***********************************************************************/

struct sbt_mms_fptr
{
	int (*sbt_mms_fptr_sbtbackup)( void *ctx, unsigned long flags,
								  char *backup_file_name,
								  sbtobject *file_info,
								  size_t block_size,
								  size_t max_size,
								  unsigned int copy_number,
								  unsigned int media_pool );
	int (*sbt_mms_fptr_sbtclose2)( void *ctx, unsigned long flags );
	int (*sbt_mms_fptr_sbtcommand)( void *ctx, unsigned long flags,
                                     char *command );
	int (*sbt_mms_fptr_sbtend)( void *ctx, unsigned long flags );
	int (*sbt_mms_fptr_sbterror)( void *ctx, unsigned long flags,
								  unsigned long *error_code,
								  char **error_text_native,
								  char **error_text_utf8 );
	int (*sbt_mms_fptr_sbtinfo2)( void *ctx, unsigned long flags,
								  char **backup_file_name_list,
								  sbtbfinfo **backup_file_info );
	int (*sbt_mms_fptr_sbtinit2)( void *ctx, unsigned long flags,
								  sbtinit2_input *initin,
								  sbtinit2_output **initout );
	int (*sbt_mms_fptr_sbtread2)( void *ctx, unsigned long flags,
								  void *buf );
	int (*sbt_mms_fptr_sbtremove2)( void *ctx, unsigned long flags,
								  char **backup_file_name_list );
	int (*sbt_mms_fptr_sbtrestore)( void *ctx, unsigned long flags,
								  char *backup_file_name,
								  size_t block_size );
	int (*sbt_mms_fptr_sbtwrite2)( void *ctx, unsigned long flags,
								   void *buf );
	int (*sbt_mms_fptr_sbtpcbackup)( void *ctx, unsigned long flags,
								   unsigned long *handle_pointer,
								   char *backup_file_name,
								   char *os_file_name,
								   sbtobject *file_info,
								   unsigned long os_reserved_size,
								   unsigned long platform_reserved_size,
								   unsigned long block_size,
								   unsigned int media_pool );
	int (*sbt_mms_fptr_sbtpccancel)( void *ctx,
								   unsigned long flags,
								   unsigned long handle );
	int (*sbt_mms_fptr_sbtpccommit)( void *ctx,
								   unsigned long flags,
								   unsigned long handle );
	int (*sbt_mms_fptr_sbtpcend)( void *ctx, unsigned long flags );
	int (*sbt_mms_fptr_sbtpcquerybackup)( void *ctx,
								   unsigned long flags,
								   char *os_file_name );
	int (*sbt_mms_fptr_sbtpcqueryrestore)( void *ctx,
								   unsigned long flags,
								   char *backup_file_name,
								   char *os_file_name );
	int (*sbt_mms_fptr_sbtpcrestore)( void *ctx, unsigned long flags,
								   unsigned long *handle_pointer,
								   char *backup_file_name,
								   char *os_file_name,
								   unsigned long os_reserved_size );
	int (*sbt_mms_fptr_sbtpcstart)( void *ctx,
								   unsigned long flags,
								   unsigned long handle,
								   unsigned long file_size );
	int (*sbt_mms_fptr_sbtpcstatus)( void *ctx,
								   unsigned long flags,
								   unsigned long handle,
								   unsigned long *handle_pointer,
								   unsigned int *file_status );
	int (*sbt_mms_fptr_sbtpcvalidate)( void *ctx,
								   unsigned long flags );
};

typedef struct sbt_mms_fptr sbt_mms_fptr;
/* shorthand function invocation macros */
#define sbtbackup_p(s)         (*(s).sbt_mms_fptr_sbtbackup)
#define sbtclose2_p(s)         (*(s).sbt_mms_fptr_sbtclose2)
#define sbtcommand_p(s)        (*(s).sbt_mms_fptr_sbtcommand)
#define sbtend_p(s)            (*(s).sbt_mms_fptr_sbtend)
#define sbterror_p(s)          (*(s).sbt_mms_fptr_sbterror)
#define sbtinfo2_p(s)          (*(s).sbt_mms_fptr_sbtinfo2)
#define sbtinit2_p(s)          (*(s).sbt_mms_fptr_sbtinit2)
#define sbtread2_p(s)          (*(s).sbt_mms_fptr_sbtread2)
#define sbtremove2_p(s)        (*(s).sbt_mms_fptr_sbtremove2)
#define sbtrestore_p(s)        (*(s).sbt_mms_fptr_sbtrestore)
#define sbtwrite2_p(s)         (*(s).sbt_mms_fptr_sbtwrite2)
#define sbtpcbackup_p(s)       (*(s).sbt_mms_fptr_sbtpcbackup)
#define sbtpccancel_p(s)       (*(s).sbt_mms_fptr_sbtpccancel)
#define sbtpccommit_p(s)       (*(s).sbt_mms_fptr_sbtpccommit)
#define sbtpcend_p(s)          (*(s).sbt_mms_fptr_sbtpcend)
#define sbtpcquerybackup_p(s)  (*(s).sbt_mms_fptr_sbtpcquerybackup)
#define sbtpcqueryrestore_p(s) (*(s).sbt_mms_fptr_sbtpcqueryrestore)
#define sbtpcrestore_p(s)      (*(s).sbt_mms_fptr_sbtpcrestore)
#define sbtpcstart_p(s)        (*(s).sbt_mms_fptr_sbtpcstart)
#define sbtpcstatus_p(s)       (*(s).sbt_mms_fptr_sbtpcstatus)
#define sbtpcvalidate_p(s)     (*(s).sbt_mms_fptr_sbtpcvalidate)

/***********************************************************************/
/* Definitions used by the API function parameters.                    */
/***********************************************************************/

/* values for flags parameter to sbtclose */
#define SBTCLOSE2_ABORT 0x1       /* file creation is being aborted */

/* values for flags parameter to sbtend */
#define SBTEND_ABEND 0x1         /* Abnormal termination of API session */

/* values for media_pool parameter to sbtbackup and sbtpcbackup */
#define SBTMEDIA_NORMAL   0
#define SBTMEDIA_ARCHIVAL 1
#define SBTMEDIA_SALTMINE 2
/* Media Pool values 0-31 are reserved for Oracle. The remaining
 * Values may be used by the media manager. */


/* values for flags parameter to sbtpcstatus */
#define SBTPCSTATUS_WAIT 0x00000001

/* values returned from sbtpcstatus in file_status parameter */
#define SBTPCSTATUS_NOTREADY   1
#define SBTPCSTATUS_READY      2
#define SBTPCSTATUS_INPROGRESS 3
#define SBTPCSTATUS_END        4
#define SBTPCSTATUS_DONE       5
#define SBTPCSTATUS_ERROR      6

/* Maximum length of API error text, including terminating null */
#define SBT_ERROR_TEXT_MAX 1024

/***********************************************************************/
/* Data types used to indicate which signals the media management      */
/* software needs to handle.                                           */
/***********************************************************************/
typedef void Sighandler( int sig );

struct sbtsiginit
{
	int sig;                                            /* Signal Number */
	Sighandler *sigfunc;                     /* Signal handling function */
};

typedef struct sbtsiginit sbtsiginit;

/***********************************************************************/
/* Version number of this API.  Both the API client and the            */
/* media management software support a distinct version of this API,   */
/* and those versions may be different.                                */
/*                                                                     */
/* This version number must correspond to an existing version of this  */
/* API, such as 1.1 or 2.0.                                            */
/*                                                                     */
/* The API Version Number is stored in an unsigned long, and must only */
/* be tested and set with the following macros.                        */
/***********************************************************************/

#define SBT_APIVSN_GET_MAJOR(ver) ((ver) >> 8 & 0xff)
#define SBT_APIVSN_GET_MINOR(ver) ((ver)      & 0xff)
#define SBT_APIVSN_SET(major, minor) \
   (((major & 0xff) << 8) | (minor & 0xff))

/***********************************************************************/
/* Version number of the media management software.  This version      */
/* number is chosen by the media management software vendor.           */
/*                                                                     */
/* This version number is selected by the vendor.                      */
/*                                                                     */
/* The MMS version number is stored in an unsigned long, and consists  */
/* of four parts.  It must only be tested and set with the following   */
/* macros.                                                             */
/***********************************************************************/

#define SBT_MMSVSN_GET_PART1(ver) ((ver) >> 24 & 0xff)
#define SBT_MMSVSN_GET_PART2(ver) ((ver) >> 16 & 0xff)
#define SBT_MMSVSN_GET_PART3(ver) ((ver) >>  8 & 0xff)
#define SBT_MMSVSN_GET_PART4(ver) ((ver)       & 0xff)
#define SBT_MMSVSN_SET(v1, v2, v3, v4) \
   (((v1 & 0xff) << 24) | \
    ((v2 & 0xff) << 16) | \
    ((v3 & 0xff) << 8)  | \
    (v4 & 0xff))

/***********************************************************************/
/* ERROR MESSAGES.  An error code from this list must be placed        */
/* in the bsercerrno field of the bserc structure, whenever an API     */
/* function returns due to error.                                      */
/***********************************************************************/
/* Common error codes.  Can be issued by any SBT V2 function. */
#define SBT_ERROR_OPERATOR  7500 /* Operator intervention requested */
#define SBT_ERROR_MM        7501 /* Catch-all media manager error */
#define SBT_ERROR_NOTFOUND  7502 /* File Not Found */
#define SBT_ERROR_EXISTS    7503 /* File already exists */
#define SBT_ERROR_EOF       7504 /* End of File */
#define SBT_ERROR_NOPROXY   7505 /* cant proxy copy the specified file */
#define SBT_ERROR_NOWORK    7506 /* no proxy work in progress */


/* error codes for sbtinit */
#define SBTINIT_ERROR_ARG      7110 /* invalid argument(s) */
#define SBTINIT_ERROR_SYS      7111 /* OS error */

/************************************************************/
/* Error Code Structure. Not used by version 2.0 functions. */
/************************************************************/

struct bserc
{
	int   bsercoer;    /* Oracle-specified error code */
	int   bsercerrno;  /* Vendor-specified error code */
};

typedef struct bserc bserc;

#define BSLERC_ERROR(brse) ((brse).bsercoer != 0)
#define BSLERC_OERC(brse) ((brse).bsercoer)

/***********************************************************************/
/*       Functions used by 1.1 and earlier API clients                 */
/***********************************************************************/
extern "C"
{
int sbtclose( bserc *brse, int th, unsigned long flags );
/* values for flags parameter to sbtclose */
#define SBTCLOSE_REWIND 1                               /* Rewind tape */
#define SBTCLOSE_NOREWIND 2                      /* Do not rewind tape */

char **sbtinfo( bserc *brse,
                char *backup_file_name,
                sbtobject *file_info );
int sbtopen( bserc *brse, char *backup_file_name, unsigned long mode,
             size_t block_size, sbtobject *file_info );
/* values for mode parameter to sbtopen */
#define SBTOPEN_READ  1        /* Open backup file for reading */
#define SBTOPEN_WRITE 2        /* Open backup file for writing */

int sbtread( bserc *brse, int th, char *buf );
int sbtremove( bserc *brse,
               char *backup_file_name,
               sbtobject *file_info );
int sbtwrite( bserc *brse, int th, char *buf );

/***********************************************************************/
/*       Functions used by all API clients                             */
/***********************************************************************/

int sbtinit( bserc *brse,
             sbtinit_input *initin,
             sbtinit_output **initout );
}

/**********************************************************************/
/* Functions provided by the API client for use by the media          */
/* management software.                                               */
/**********************************************************************/

Sighandler *sbrtsigset( int sig, Sighandler *func );
unsigned int sbrtalarm( unsigned int sec );

/***********************************************************************/
/* Macros used to call version 1 functions by pointer.  These are      */
/* reserved for use by the API client.                                 */
/***********************************************************************/

struct sbt_v1_fptr
{
	int    (*sbt_v1_fptr_sbtinit)( bserc *brse,
                                  sbtinit_input *initin,
                                  sbtinit_output **initout );
	int    (*sbt_v1_fptr_sbtopen)( bserc *brse,
                                  char *backup_file_name,
                                  unsigned long mode,
                                  size_t block_size,
                                  sbtobject *file_info );
	int    (*sbt_v1_fptr_sbtread)( bserc *brse,
                                  int th,
                                  char *buf );
	int    (*sbt_v1_fptr_sbtwrite)(bserc *brse,
                                  int th,
                                  char *buf );
	int    (*sbt_v1_fptr_sbtclose)(bserc *brse,
                                  int th,
                                  unsigned long flags );
	char** (*sbt_v1_fptr_sbtinfo)( bserc *brse,
                                  char *backup_file_name,
                                  sbtobject *file_info );
	int    (*sbt_v1_fptr_sbtremove)( bserc *brse,
                                    char *backup_file_name,
                                    sbtobject *file_info );
};

typedef struct sbt_v1_fptr sbt_v1_fptr;

#define sbtinit_p(s)   (*(s).sbt_v1_fptr_sbtinit)
#define sbtopen_p(s)   (*(s).sbt_v1_fptr_sbtopen)
#define sbtread_p(s)   (*(s).sbt_v1_fptr_sbtread)
#define sbtwrite_p(s)  (*(s).sbt_v1_fptr_sbtwrite)
#define sbtclose_p(s)  (*(s).sbt_v1_fptr_sbtclose)
#define sbtinfo_p(s)   (*(s).sbt_v1_fptr_sbtinfo)
#define sbtremove_p(s) (*(s).sbt_v1_fptr_sbtremove)

/***********************************************************************/
/* Version 1.1 error codes.                                            */
/***********************************************************************/

/* error codes for sbtopen */
#define SBTOPEN_ERROR_NOTFOUND 7000 /* file not found (only for read) */
#define SBTOPEN_ERROR_EXISTS   7001 /* file exists (only for write) */
#define SBTOPEN_ERROR_MODE     7002 /* bad mode specified */
#define SBTOPEN_ERROR_BLKSIZE  7003 /* bad block size specified */
#define SBTOPEN_ERROR_NODEV    7004 /* no tape device found */
#define SBTOPEN_ERROR_BUSY     7005 /* device is busy ; try again later */
#define SBTOPEN_ERROR_NOVOL    7006 /* tape volume not found */
#define SBTOPEN_ERROR_INUSE    7007 /* tape volume is in-use */
#define SBTOPEN_ERROR_IO       7008 /* I/O Error */
#define SBTOPEN_ERROR_CONNECT  7009 /* cant connect with Media Manager */
#define SBTOPEN_ERROR_AUTH     7010 /* permission denied */
#define SBTOPEN_ERROR_SYS      7011 /* OS error */
#define SBTOPEN_ERROR_ARG      7012 /* invalid argument(s) */

/* error codes for sbtclose */
/* SBTCLOSE_ERROR_CONNECT has been added in API version 1.1 */
#define SBTCLOSE_ERROR_HANDLE  7020 /* bad th (no sbtopen done) */
#define SBTCLOSE_ERROR_FLAGS   7021 /* bad flags */
#define SBTCLOSE_ERROR_IO      7022 /* I/O error */
#define SBTCLOSE_ERROR_SYS     7023 /* OS error */
#define SBTCLOSE_ERROR_ARG     7024 /* invalid argument(s) */
#define SBTCLOSE_ERROR_CONNECT 7025 /* cant connect with Media Manager */

/* error codes for sbtwrite */
#define SBTWRITE_ERROR_HANDLE 7040 /* bad th */
#define SBTWRITE_ERROR_IO     7042 /* I/O error */
#define SBTWRITE_ERROR_SYS    7043 /* OS error */
#define SBTWRITE_ERROR_ARG    7044 /* invalid argument(s) */

/* error codes for sbtread */
#define SBTREAD_ERROR_HANDLE 7060 /* bad th */
#define SBTREAD_ERROR_EOF    7061 /* EOF encountered */
#define SBTREAD_ERROR_IO     7063 /* I/O error */
#define SBTREAD_ERROR_SYS    7064 /* OS error */
#define SBTREAD_ERROR_ARG    7065 /* invalid argument(s) */

/* error codes for sbtremove */
#define SBTREMOVE_ERROR_NOTFOUND 7080 /* backup file not found */
#define SBTREMOVE_ERROR_INUSE    7081 /* backup file being used */
#define SBTREMOVE_ERROR_IO       7082 /* I/O Error */
#define SBTREMOVE_ERROR_CONNECT  7083 /* cant connect with Media Manager */
#define SBTREMOVE_ERROR_AUTH     7084 /* permission denied */
#define SBTREMOVE_ERROR_SYS      7085 /* OS error */
#define SBTREMOVE_ERROR_ARG      7086 /* invalid argument(s) */

/* error codes for sbtinfo */
#define SBTINFO_ERROR_NOTFOUND 7090 /* backup file not found */
#define SBTINFO_ERROR_IO       7091 /* I/O Error */
#define SBTINFO_ERROR_CONNECT  7092 /* cant connect with Media Manager */
#define SBTINFO_ERROR_AUTH     7093 /* permission denied */
#define SBTINFO_ERROR_SYS      7094 /* OS error */
#define SBTINFO_ERROR_ARG      7095 /* invalid argument(s) */

/***********************************************************************/
/* Version 1.1 definitions. These old names are retained for           */
/* backwards compatibility, but their use is discouraged.              */
/***********************************************************************/

#define SBTOPFHS SBTOBJECT_HOST
#define SBTOPFSI SBTOBJECT_SID
#define SBTOPFDB SBTOBJECT_DBNAME
#define SBTOPFTS SBTOBJECT_TSNAME
#define SBTOPFFL SBTOBJECT_FILENAME
#define SBTOPFNL SBTOBJECT_END
#define SBTOPFDN SBTOBJECT_DOMAIN
#define sbtiniobj sbtinit_input
#define SBTIIFNL SBTINIT_INEND
#define SBTIIFTF SBTINIT_TRACE_NAME
#define sbtinoobj sbtinit_output
#define SBTIOFNL SBTINIT_OUTEND
#define SBTIOFSG SBTINIT_SIGNAL
#define SBTIOFVI SBTINIT_MMS_APIVSN
#define SBTIOFVT SBTINIT_MMSVSN
#define SBTIOVIMAJ SBT_APIVSN_GET_MAJOR
#define SBTIOVIMIN SBT_APIVSN_GET_MINOR
#define SBTIOVTV1 SBT_MMSVSN_GET_PART1
#define SBTIOVTV2 SBT_MMSVSN_GET_PART2
#define SBTIOVTV3 SBT_MMSVSN_GET_PART3
#define SBTIOVTV4 SBT_MMSVSN_GET_PART4
#define SBTCLFRW SBTCLOSE_REWIND
#define SBTCLFNR SBTCLOSE_NOREWIND
#define SBTOPMRD SBTOPEN_READ
#define SBTOPMWT SBTOPEN_WRITE
#define SBTOPENF SBTOPEN_ERROR_NOTFOUND
#define SBTOPEEX SBTOPEN_ERROR_EXISTS
#define SBTOPEBM SBTOPEN_ERROR_MODE
#define SBTOPEBS SBTOPEN_ERROR_BLKSIZE
#define SBTOPEDV SBTOPEN_ERROR_NODEV
#define SBTOPEDB SBTOPEN_ERROR_BUSY
#define SBTOPEVL SBTOPEN_ERROR_NOVOL
#define SBTOPEVB SBTOPEN_ERROR_INUSE
#define SBTOPEIO SBTOPEN_ERROR_IO
#define SBTOPECN SBTOPEN_ERROR_CONNECT
#define SBTOPEAC SBTOPEN_ERROR_AUTH
#define SBTOPEOS SBTOPEN_ERROR_SYS
#define SBTOPEIA SBTOPEN_ERROR_ARG
#define SBTCLENF SBTCLOSE_ERROR_HANDLE
#define SBTCLEBF SBTCLOSE_ERROR_FLAGS
#define SBTCLEIO SBTCLOSE_ERROR_IO
#define SBTCLEOS SBTCLOSE_ERROR_SYS
#define SBTCLEIA SBTCLOSE_ERROR_ARG
#define SBTCLECN SBTCLOSE_ERROR_CONNECT
#define SBTWTENF SBTWRITE_ERROR_HANDLE
#define SBTWTEET 7041 /* this is an old error code - do not use in 2.0 */
#define SBTWTEER SBTWRITE_ERROR_IO
#define SBTWTEOS SBTWRITE_ERROR_SYS
#define SBTWTEIA SBTWRITE_ERROR_ARG
#define SBTRDENF SBTREAD_ERROR_HANDLE
#define SBTRDEEF SBTREAD_ERROR_EOF
#define SBTRDEET 7062 /* this is an old error code - do not use in 2.0 */
#define SBTRDEER SBTREAD_ERROR_IO
#define SBTRDEOS SBTREAD_ERROR_SYS
#define SBTRDEIA SBTREAD_ERROR_ARG
#define SBTRMENF SBTREMOVE_ERROR_NOTFOUND
#define SBTRMEUS SBTREMOVE_ERROR_INUSE
#define SBTRMEIO SBTREMOVE_ERROR_IO
#define SBTRMECN SBTREMOVE_ERROR_CONNECT
#define SBTRMEAC SBTREMOVE_ERROR_AUTH
#define SBTRMEOS SBTREMOVE_ERROR_SYS
#define SBTRMEIA SBTREMOVE_ERROR_ARG
#define SBTIFENF SBTINFO_ERROR_NOTFOUND
#define SBTIFEIO SBTINFO_ERROR_IO
#define SBTIFECN SBTINFO_ERROR_CONNECT
#define SBTIFEAC SBTINFO_ERROR_AUTH
#define SBTIFEOS SBTINFO_ERROR_SYS
#define SBTIFEIA SBTINFO_ERROR_ARG
#define SBTINEIA SBTINIT_ERROR_ARG
#define SBTINEOS SBTINIT_ERROR_SYS

#endif /* SKGFQSBT_ORACLE */
