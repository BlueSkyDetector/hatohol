/*
 * Copyright (C) 2014 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DBTablesHost.h"
using namespace std;
using namespace mlpl;

static const char *TABLE_NAME_HOST_LIST       = "host_list";
static const char *TABLE_NAME_SERVER_HOST_DEF = "server_host_def";
static const char *TABLE_NAME_HOST_LOCATION   = "host_location";
static const char *TABLE_NAME_VM_LIST         = "vm_list";

const int DBTablesHost::TABLES_VERSION = 1;

const uint64_t NO_HYPERVISOR = -1;
const size_t MAX_HOST_NAME_LENGTH =  255;

static const ColumnDef COLUMN_DEF_HOST_LIST[] = {
{
	"id",                              // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_PRI,                       // keyType
	SQL_COLUMN_FLAG_AUTO_INC,          // flags
	NULL,                              // defaultValue
}, {
	"name",                            // columnName
	SQL_COLUMN_TYPE_VARCHAR,           // type
	255,                               // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
},
};

enum {
	IDX_HOST_LIST_ID,
	IDX_HOST_LIST_NAME,
	NUM_IDX_HOST_LIST
};

static const DBAgent::TableProfile tableProfileHostList =
  DBAGENT_TABLEPROFILE_INIT(TABLE_NAME_HOST_LIST,
			    COLUMN_DEF_HOST_LIST,
			    NUM_IDX_HOST_LIST);

static const ColumnDef COLUMN_DEF_SERVER_HOST_DEF[] = {
{
	"id",                              // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_PRI,                       // keyType
	SQL_COLUMN_FLAG_AUTO_INC,          // flags
	NULL,                              // defaultValue
}, {
	// Host ID in host_list table.
	"host_id",                         // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	true,                              // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
}, {
	"server_id",                       // columnName
	SQL_COLUMN_TYPE_INT,               // type
	11,                                // columnLength
	0,                                 // decFracLength
	true,                              // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
}, {
	// The type of this column is a string so that a UUID string
	// can also be stored.
	"host_id_in_server",               // columnName
	SQL_COLUMN_TYPE_VARCHAR,           // type
	255,                               // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
}, {
	// The name the monitoring server uses
	"name",                            // columnName
	SQL_COLUMN_TYPE_VARCHAR,           // type
	255,                               // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
},
};

enum {
	IDX_HOST_SERVER_HOST_DEF_ID,
	IDX_HOST_SERVER_HOST_DEF_HOST_ID,
	IDX_HOST_SERVER_HOST_DEF_SERVER_ID,
	IDX_HOST_SERVER_HOST_DEF_HOST_ID_IN_SERVER,
	IDX_HOST_SERVER_HOST_DEF_HOST_NAME,
	NUM_IDX_SERVER_HOST_DEF
};

static const int columnIndexesHostServerDefIdx[] = {
  IDX_HOST_SERVER_HOST_DEF_SERVER_ID,
  IDX_HOST_SERVER_HOST_DEF_HOST_ID_IN_SERVER, DBAgent::IndexDef::END,
};

static const DBAgent::IndexDef indexDefsHostServerDef[] = {
  {"HostServerDefIdx", (const int *)columnIndexesHostServerDefIdx, false},
  {NULL}
};

static const DBAgent::TableProfile tableProfileServerHostDef =
  DBAGENT_TABLEPROFILE_INIT(TABLE_NAME_SERVER_HOST_DEF,
			    COLUMN_DEF_SERVER_HOST_DEF,
			    NUM_IDX_SERVER_HOST_DEF,
			    indexDefsHostServerDef);

// We manage multiple IP adresses and host naems for one host.
// So the following are defined in the independent table.
static const ColumnDef COLUMN_DEF_HOST_LOCATION[] = {
{
	"id",                              // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_PRI,                       // keyType
	SQL_COLUMN_FLAG_AUTO_INC,          // flags
	NULL,                              // defaultValue
}, {
	"host_id",                         // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
}, {
	// Any of IPv4, IPv6 and FQDN
	"ip_addr_or_fqdn",                 // columnName
	SQL_COLUMN_TYPE_VARCHAR,           // type
	255,                               // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
}, {
	// The value in this column offers a hint to decide which IP/FQDN
	// should be used. The greater number has higher priority.
	"priority",                        // columnName
	SQL_COLUMN_TYPE_INT,               // type
	11,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_NONE,                      // keyType
	0,                                 // flags
	NULL,                              // defaultValue
},
};

enum {
	IDX_HOST_LOCATION_ID,
	IDX_HOST_LOCATION_HOST_ID,
	IDX_HOST_LOCATION_IP_ADDR_OR_FQDN,
	IDX_HOST_LOCATION_PRIORITY,
	NUM_IDX_HOST_LOCATION
};

static const DBAgent::TableProfile tableProfileHostInfo =
  DBAGENT_TABLEPROFILE_INIT(TABLE_NAME_HOST_LOCATION,
			    COLUMN_DEF_HOST_LOCATION,
			    NUM_IDX_HOST_LOCATION);

static const ColumnDef COLUMN_DEF_VM_LIST[] = {
{
	"id",                              // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_PRI,                       // keyType
	SQL_COLUMN_FLAG_AUTO_INC,          // flags
	NULL,                              // defaultValue
}, {
	"host_id",                         // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
}, {
	"hypervisor_host_id",              // columnName
	SQL_COLUMN_TYPE_BIGUINT,           // type
	20,                                // columnLength
	0,                                 // decFracLength
	false,                             // canBeNull
	SQL_KEY_IDX,                       // keyType
	0,                                 // flags
	NULL,                              // defaultValue
},
};

enum {
	IDX_HOST_VM_LIST_ID,
	IDX_HOST_VM_LIST_HOST_ID,
	IDX_HOST_VM_LIST_HYPERVISOR_HOST_ID,
	NUM_IDX_VM_LIST
};

static const DBAgent::TableProfile tableProfileVMList =
  DBAGENT_TABLEPROFILE_INIT(TABLE_NAME_VM_LIST,
			    COLUMN_DEF_VM_LIST,
			    NUM_IDX_VM_LIST);

struct DBTablesHost::Impl
{
	Impl(void)
	{
	}

	virtual ~Impl()
	{
	}
};

static bool updateDB(DBAgent &dbAgent, const int &oldVer, void *data)
{
	return true;
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------
void DBTablesHost::reset(void)
{
	getSetupInfo().initialized = false;
}

DBTablesHost::DBTablesHost(DBAgent &dbAgent)
: DBTables(dbAgent, getSetupInfo()),
  m_impl(new Impl())
{
}

DBTablesHost::~DBTablesHost()
{
}

HostIdType DBTablesHost::addHost(const string &name)
{
	HostIdType hostId;
	DBAgent::InsertArg arg(tableProfileHostList);
	arg.add(AUTO_INCREMENT_VALUE);
	arg.add(name);
	getDBAgent().runTransaction(arg, &hostId);
	return hostId;
}

GenericIdType DBTablesHost::upsertServerHostDef(
  const ServerHostDef &serverHostDef)
{
	GenericIdType id;
	DBAgent::InsertArg arg(tableProfileServerHostDef);
	arg.add(serverHostDef.id);
	arg.add(serverHostDef.hostId);
	arg.add(serverHostDef.serverId);
	arg.add(serverHostDef.hostIdInServer);
	arg.add(serverHostDef.name);
	arg.upsertOnDuplicate = true;
	getDBAgent().runTransaction(arg, &id);
	return id;
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------
DBTables::SetupInfo &DBTablesHost::getSetupInfo(void)
{
	//
	// set database info
	//
	static const TableSetupInfo TABLE_INFO[] = {
	{
		&tableProfileHostList,
	}, {
		&tableProfileServerHostDef,
	}, {
		&tableProfileHostInfo,
	}
	};
	static const size_t NUM_TABLE_INFO =
	  ARRAY_SIZE(TABLE_INFO);

	static SetupInfo SETUP_INFO = {
		DB_TABLES_ID_HOST,
		TABLES_VERSION,
		NUM_TABLE_INFO,
		TABLE_INFO,
		&updateDB,
	};
	return SETUP_INFO;
}

