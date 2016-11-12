/***************************************************************************************************
ncOracleBackupEngine.h:
	Copyright (c) Eisoo Software, Inc.(2004 - 2013), All rights reserved.

Purpose:
	 implementation file for Oracle ncOracleBackupEngine

Author:
	luo.qiang@eisoo.com

Creating Time:
	2013-7-18
***************************************************************************************************/
#include "ncOracleBackupEngine.h"
#include <ncIBackupScheduler.h>
#include <ncIBackupDataSource.h>
#include <ncOracleContractID.h>
#include <ncRACContractID.h>
#include <backupengine.h>
#include <ncOracleObject.h>

NS_IMPL_ISUPPORTS1(ncOracleBackupEngine, ncIBackupEngine)

ncOracleBackupEngine::ncOracleBackupEngine ()
											: _engineType (0)
											, _clientType (0)
{
	NC_ORACLE_BACKUP_ENGINE_TRACE(_T("ncOracleBackupEngine::ncOracleBackupEngine ()"));
}

ncOracleBackupEngine::~ncOracleBackupEngine ()
{
	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::~ncOracleBackupEngine ()"));
}

NS_IMETHODIMP_(ncIBackupDataSource *)
ncOracleBackupEngine::CreateDataSource (void)
{
	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::CreateDataSource () begin _clientType=%d, _engineType=%d"), _clientType, _engineType);
	nsresult result;
	nsCOMPtr <ncIBackupDataSource> oracleDataSource;
	if (_clientType == STAND_ALONE || _clientType == COUPLE_CLIENTS){
		oracleDataSource = do_CreateInstance (NC_ORACLE_DATA_SOURCE_CONTRACTID, &result);

		if (_clientType == COUPLE_CLIENTS) {
			vector<ncKeyValueParam>	valueVec;
			ncKeyValueParam  clientType;
			clientType.key = EEE_CLIENT_TYPE;
			clientType.value = ORACLE_COUPLE_CLIENTS;
			valueVec.push_back(clientType);
			oracleDataSource->SetParams (EEE_CLIENT_TYPE, valueVec);
		}
	}
	else if (_clientType == MULTI_CLIENTS3){
		oracleDataSource = do_CreateInstance (NC_RAC_DATA_SOURCE_CONTRACTID, &result);
	}
	else{
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_DATASOURCE_FAILED")), ERROR_ORACLE_DATA_SOURCE_CONTRACTID);
	}
	
	if(NS_FAILED (result)) {
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_DATASOURCE_FAILED")), ERROR_ORACLE_DATA_SOURCE_CONTRACTID);
	}

	NS_ADDREF (oracleDataSource.get ());
	
	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::CreateDataSource () end"));
	return oracleDataSource.get ();
}

NS_IMETHODIMP_(ncIBackupScheduler *)
ncOracleBackupEngine::CreateBackupScheduler ()
{
	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::CreateBackupScheduler () begin  _clientType=%d, _engineType=%d"), _clientType, _engineType);
	nsresult result;
	nsCOMPtr <ncIBackupScheduler> oracleBackupScheduler;
	if (_clientType == STAND_ALONE || _clientType == COUPLE_CLIENTS){
		if (_engineType == NORMAL_ENGINE) {
			oracleBackupScheduler = do_CreateInstance (NC_ORACLE_BACKUP_SCHEDULER_CONTRACTID, &result);
		}
		else if (_engineType == 13) {
			oracleBackupScheduler = do_CreateInstance (NC_ORACLE_EXECUTER_CONTRACTID, &result);
		}
	}
	else if (_clientType == MULTI_CLIENTS3){
		if (_engineType == 13) {
			oracleRestoreScheduler = do_CreateInstance(NC_ORACLE_EXECUTER_CONTRACTID, &result);
		}
		oracleBackupScheduler = do_CreateInstance (NC_RAC_BACKUP_SCHEDULER_CONTRACTID, &result);
	}
	else{
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_BACKUP_SCHEDULER_FAILED")), ERROR_CREATE_ORACLE_BACKUP_SCHEDULER_FAILED);
	}

	if(NS_FAILED (result))
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_BACKUP_SCHEDULER_FAILED")), ERROR_CREATE_ORACLE_BACKUP_SCHEDULER_FAILED);

	NS_ADDREF (oracleBackupScheduler.get ());

	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::CreateBackupScheduler () end"));
	return oracleBackupScheduler.get ();
}

NS_IMETHODIMP_(ncIBackupScheduler *)
ncOracleBackupEngine::CreateRestoreScheduler ()
{
	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::CreateRestoreScheduler () begin _clientType=%d, _engineType=%d"), _clientType, _engineType);
	nsresult result;
	nsCOMPtr <ncIBackupScheduler> oracleRestoreScheduler;

	if (_clientType == STAND_ALONE || _clientType == COUPLE_CLIENTS){
		if (_engineType == NORMAL_ENGINE || _engineType == FINE_GRAINED_ENGINE) {
			oracleRestoreScheduler = do_CreateInstance (NC_ORACLE_RESTORE_SCHEDULER_CONTRACTID, &result);
		}
		else if (_engineType == 13) {
			oracleRestoreScheduler = do_CreateInstance (NC_ORACLE_EXECUTER_CONTRACTID, &result);
		}
	}
	else if (_clientType == MULTI_CLIENTS3){
		if (_engineType = NORMAL_ENGINE){
			oracleRestoreScheduler = do_CreateInstance (NC_RAC_RESTORE_SCHEDULER_CONTRACTID, &result);
		}
		else if (_engineType == 13) {
			oracleRestoreScheduler = do_CreateInstance (NC_ORACLE_EXECUTER_CONTRACTID, &result);
		}
		else if (_engineType == FINE_GRAINED_ENGINE){
			//œ∏¡£∂»ª÷∏¥schedule£®Œ¥ µœ÷£©°£
		}
	}
	else 
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_RETORE_SCHEDULER_FAILED")), ERROR_CREATE_ORACLE_RETORE_SCHEDULER_FAILED);

	if(NS_FAILED (result)) {
		THROW_ORACLE_BACKUP_ERROR (LOAD_STRING (_T("IDS_CREATE_ORACLE_RETORE_SCHEDULER_FAILED")), ERROR_CREATE_ORACLE_RETORE_SCHEDULER_FAILED);
	}

	NS_ADDREF (oracleRestoreScheduler.get ());

	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::CreateRestoreScheduler () end"));
	return oracleRestoreScheduler.get ();
}

NS_IMETHODIMP_(void)
ncOracleBackupEngine::SetEngineType (int engineType)
{
	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::SetEngineType () engineType = %d"), engineType);
	_engineType = engineType;
}

NS_IMETHODIMP_(int)
ncOracleBackupEngine::GetEngineType ()
{
	return _engineType;
}

NS_IMETHODIMP_(void)
ncOracleBackupEngine::SetClientType (int clientType)
{
	NC_ORACLE_BACKUP_ENGINE_TRACE (_T("ncOracleBackupEngine::SetClientType () clientType = %d"), clientType);
	_clientType = clientType;
}

NS_IMETHODIMP_(int)
ncOracleBackupEngine::GetClientType ()
{
	return _clientType;
}