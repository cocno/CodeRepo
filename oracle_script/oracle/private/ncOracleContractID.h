/****************************************************************************************
ncOracleContractID.h
	Copyright (c)  Eisoo Software Inc. (2004-2013), All right reserved

PursePose:
	header file for Oracle ContractID

Author: 
	huang.kaide@eisoo.com

Created Time: 
	2013-7-15
***************************************************************************************/
#ifndef _NC_ORACLE_CONTRACT_ID_H_
#define _NC_ORACLE_CONTRACT_ID_H_

//NC_ORACLE_BACKUP_SCHEDULER_CID   {693B6030-AE37-4697-B9D4-C61524D04AD7}
#define NC_ORACLE_BACKUP_SCHEDULER_CID                 \
{                                                      \
	0x693b6030,                                        \
	0xae37,                                            \
	0x4697,                                            \
	{ 0xb9, 0xd4, 0xc6, 0x15, 0x24, 0xd0, 0x4a, 0xd7 } \
}

//oracle backup schedule contract  id
#define NC_ORACLE_BACKUP_SCHEDULER_CONTRACTID          \
	"eisoo.com/oraclebackupschedule;1"

//NC_ORACLE_RESTORE_SCHEDULER_CID   {AE2B5D77-4AED-4d3c-98F1-0206A5BEF2F0}
#define NC_ORACLE_RESTORE_SCHEDULER_CID                \
{                                                      \
	0xae2b5d77,                                        \
	0x4aed,                                            \
	0x4d3c,                                            \
	{ 0x98, 0xf1, 0x2, 0x6, 0xa5, 0xbe, 0xf2, 0xf0 }   \
}

//oracle restore schedule contract  id
#define NC_ORACLE_RESTORE_SCHEDULER_CONTRACTID         \
	"eisoo.com/oraclerestoreschedule;1"

//NC_ORACLE_DATA_SOURCE_CID        {18BFFD31-1499-47f7-A518-B2FC607CABA7}
#define  NC_ORACLE_DATA_SOURCE_CID                      \
{                                                       \
	0x18bffd31,                                         \
	0x1499,                                             \
	0x47f7,                                             \
	{ 0xa5, 0x18, 0xb2, 0xfc, 0x60, 0x7c, 0xab, 0xa7 }  \
}

//oracle data  source contract  id
#define NC_ORACLE_DATA_SOURCE_CONTRACTID                \
	"eisoo.com/oracledatasource;1"

//NC_ORACLE_FG_BACKUP_SCHEDULER_CID {72A03F90-7487-4A07-945E-4DFB26284CDD}
#define NC_ORACLE_FG_BACKUP_SCHEDULER_CID				\
{														\
	0x72a03f90,											\
	0x7487,												\
	0x4a07,												\
	{ 0x94, 0x5e, 0x4d, 0xfb, 0x26, 0x28, 0x4c, 0xdd }	\
}

//oracle fine-grained backup schedule contract id
#define NC_ORACLE_FG_BACKUP_SCHEDULER_CONTRACTID          \
	"eisoo.com/oraclefgbackupschedule;1"

//NC_ORACLE_FG_RESTORE_SCHEDULER_CID {567AD415-3644-4E1A-9101-E32475AB473A}
#define NC_ORACLE_FG_RESTORE_SCHEDULER_CID				\
{														\
	0x567ad415,											\
	0x3644,												\
	0x4e1a,												\
	{ 0x91, 0x01, 0xe3, 0x24, 0x75, 0xab, 0x47, 0x3a }	\
}

//oracle fine-grained restore schedule contract id
#define NC_ORACLE_FG_RESTORE_SCHEDULER_CONTRACTID          \
	"eisoo.com/oraclefgrestoreschedule;1"


//NC_ORACLE__EXECUTER_CID {AB7C184E-549C-405D-8A98-500B5B010055}
#define NC_ORACLE__EXECUTER_CID							\
{														\
	0xab7c184e,											\
	0x549c,												\
	0x405d,												\
	{ 0x8a, 0x98, 0x50, 0xb, 0x5b, 0x1, 0x0, 0x55 }		\
}

//oracle custom script backup excuter contract id
#define NC_ORACLE_EXECUTER_CONTRACTID          \
	"eisoo.com/oraclefgrestoreschedule;1"

#endif /* _NC_ORACLE_CONTRACT_ID_H_ */
