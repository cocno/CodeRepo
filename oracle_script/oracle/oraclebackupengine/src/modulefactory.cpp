/***************************************************************************************************
modulefactory.cpp:
	Copyright (c) Eisoo Software, Inc.(2004 - 2013), All rights reserved.

Purpose:
	implementation file for Oracle backupEngine modulefactory

Author:
	luo.qiang@eisoo.com

Creating Time:
	2013-7-18
***************************************************************************************************/
#include <abprec.h>
#include <ncutil.h>
#include <backupengine.h>
#include <ncOracleContractID.h>
#include "ncOracleBackupEngineCommon.h"
#include "ncOracleBackupEngine.h"

#define LIB_NAME _T("oraclebackupengine")

IResourceLoader* oracleBackupEngineLoader = 0;

/**
* DEFINE TRACE
*/
NC_DEFINE_TRACE_MODULE (ncOracleBackupEngineTrace);

NS_GENERIC_FACTORY_CONSTRUCTOR (ncOracleBackupEngine)

//
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static const nsModuleComponentInfo components[] = 
{
	{
		"ncOracleBackupEngine",
		NC_ORACLE_BACKUP_ENGINE_CID,
		NC_ORACLE_BACKUP_ENGINE_CONTRACTID,
		ncOracleBackupEngineConstructor
	},
};


/**
* 初始化和关闭库。
*/
class ncOracleBackupEngineLib: public ISharedLibrary
{
public:	
	virtual void onInitLibrary (const AppSettings *appSettings, 
							  	const class AppContext *appCtx)
	{	
		if (oracleBackupEngineLoader != 0)
			return;

		oracleBackupEngineLoader = new MoResourceLoader (::getResourceFileName (LIB_NAME,
												   					   appSettings,
													   				   appCtx,
																	   AB_RESOURCE_MO_EXT_NAME));

		//
		// 添加 trace
		//
		NC_CREATE_TRACE_MODULE (ncOracleBackupEngineTrace, LIB_NAME);
	}	

	virtual void onCloseLibrary (void) NO_THROW
	{
		if (oracleBackupEngineLoader != 0) {
			delete oracleBackupEngineLoader;
			oracleBackupEngineLoader = 0;
		}
	}	
	
	virtual void onInstall (const AppSettings* appSettings, 
							 const AppContext* appCtx)
	{
	}	
	
	virtual void onUninstall (void) NO_THROW
	{
	}
		
	virtual const tchar_t* getLibName (void) const
	{
		return LIB_NAME;
	}
}; // class ncOracleBackupEngineLib

//
// 定义共享库
//

AB_IMPL_NSGETMODULE_WITH_LIB (LIB_NAME, components, ncOracleBackupEngineLib)
