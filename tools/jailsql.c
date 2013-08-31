//Part of the CBSD Project
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sqlite3.h"

#include <getopt.h>

#include "gentools.h"
#include "sqlhelper.h"
#include "jailsql.h"

void update_inventory(char *column, char *value) {
char buf[SQLSTRLEN];
    sql_stmt("begin");
    bzero(buf,SQLSTRLEN);
    sprintf(buf,"update jails set %s = \"%s\" where jname=\"%s\"",column,value,jname);
    debugmsg(1,"SQL: %s\n",buf);
    sql_stmt(buf);
    sql_stmt("commit");
}

void delete_inventory(char *column) {
char buf[SQLSTRLEN];

    sql_stmt("begin");
    bzero(buf,SQLSTRLEN);
    sprintf(buf,"delete from jails where jname=\"%s\"",column);
    debugmsg(1,"SQL: %s\n",buf);
    sql_stmt(buf);
    sql_stmt("commit");
}

void usage() {
    printf("Tools for update or view cbsd inventory in sqlite database\n");
    printf("require: dbfile\n");
    printf("opt: action workdir param value sqlquery\n");
    printf("dbfile - sqlite database file\n");
    printf("sqlquery - execute direct sql query\n");
    printf("action can be:\n");
    printf("- init - for re-create database (drop and create empty table)\n");
    printf("- list - select all records from database\n");
    printf("- update ( param=XXXX value=XXXX ) for insert or update param\n");
    printf("- upgradedb - upgrade sql scheme if necessary\n");
    printf("- get - get value for --value=xxx\n");
    printf("- upgrade - upgrade scheme if nesessary\n");
//    printf("- delete ( param=XXXX ) for remove records from registry\n");
    printf("\n");
}


int main(int argc, char **argv)
{
int win = FALSE;
int optcode = 0;
int option_index = 0, ret = 0;
int action=0;
int i=0, firstrow=0;
char buf[SQLSTRLEN];

static struct option long_options[] = {
    { "dbfile", required_argument, 0 , C_DBFILE },
    { "action", required_argument, 0 , C_ACTION },
    { "help", no_argument, 0, C_HELP },
    { "debug", required_argument, 0, C_DEBUG },
    { "workdir", required_argument, 0, C_WORKDIR },
    { "param", required_argument, 0, C_PARAM },
    { "value", required_argument, 0, C_VALUE },
    { "sqlquery", required_argument, 0, C_SQLQUERY },
    { "jname", required_argument, 0, C_JNAME },
    /* End of options marker */
    { 0, 0, 0, 0 }
    };

    while (TRUE) {
	optcode = getopt_long(argc, argv, "h", long_options, &option_index);
	if (optcode == -1) break;
	int this_option_optind = optind ? optind : 1;
	switch (optcode) {
	    case C_DBFILE:
		dbfile=optarg;
		break;
	    case C_ACTION:
		i=0;
		while (actionlist[++i]!=NULL)
		    if (!strcmp(optarg,actionlist[i])) {
			action=i;
			break;
		    }
		    if (action==0) {
			errmsg("Bad action\r\n");
			exit(1);
		    }
		break;
	    case C_HELP:      /* usage() */
		usage();
		exit(0);
		break;
	    case C_DEBUG:      /* debuglevel 0-2 */
		debug=atoi(optarg);
		break;
	    case C_PARAM:
		param=optarg;
		break;
	    case C_VALUE:
		memset(buf,0,sizeof(buf));
		strcpy(buf,optarg);
		if (optind < argc)
		    while (optind < argc) {
			    strcat(buf," ");
			    strcat(buf,argv[optind++]);
		    }
		value=malloc(strlen(buf)+1);
		memcpy(value,buf,strlen(buf));
		value[strlen(buf)]='\0';
		break;
	    case C_SQLQUERY:
		memset(buf,0,sizeof(buf));
		strcpy(buf,optarg);
		if (optind < argc)
		    while (optind < argc) {
			    strcat(buf," ");
			    strcat(buf,argv[optind++]);
		    }
		sqlquery=malloc(strlen(buf)+1);
		memcpy(sqlquery,buf,strlen(buf));
		sqlquery[strlen(buf)]='\0';
		debugmsg(1,"Execute direct SQL: [%s]\n",sqlquery);
		sqlite3_open(dbfile, &db);
		if (db == 0 ) {
			errmsg("Could not open database %s\n",dbfile);
			exit(1);
		}
		ret=select_valstmt(sqlquery);
		goto closeexit;
		break;
	    case C_JNAME:
		jname=optarg;
		break;
	} //case
    } //while

    if (argc<2) exit(0);

    if (dbfile==NULL) {
	errmsg("Please set --dbfile=\n");
	exit(1);
    }

    if (action==0) {
	errmsg("Please set correct --action\n");
	exit(1);
    }

    debugmsg(2,"Action choosed: %d\n",action);
    sqlite3_open(dbfile, &db);

    if(db == 0) {
	errmsg("Could not open database %s\n",dbfile);
	return 1;
    }

    switch (action) {
	case (INIT):
	    ret=sql_stmt("drop table if exists jails");
	    memset(buf,0,sizeof(buf));
	    strcpy(buf,"create table jails ( ");

	    firstrow=0;
	    //aggregate all actual data for SQL tables
	    for (i=0 ; *sqldb_info[i].rowname != '\n'; i++) {
		    if (firstrow) strcat(buf,", ");
		    strcat(buf,sqldb_info[i].rowname);
		    strcat(buf," ");
		    strcat(buf,sqldb_info[i].rowtype);
		    firstrow++;
	    }

	    debugmsg(2,"DB have %d items\n",firstrow);
	    strcat(buf,")");
	    debugmsg(1,"SQL Exec: %s\n",buf);
	    ret=sql_stmt(buf);

	    if (ret==0) debugmsg(1,"table 'jails' init successfull\n");
		else errmsg("table 'jails' init failed\n");

            goto closeexit;
        break;
	case (UPGRADE):
            //aggregate all actual data for SQL tables
    	    for (i=0 ; *sqldb_info[i].rowname != '\n'; i++) {
                memset(buf,0,sizeof(buf));
                    sprintf(buf,"SELECT %s FROM jails LIMIT 1",sqldb_info[i].rowname);
                    ret=sql_stmt(buf);
                    if (ret!=0) {
                        //probably not exist, try to alter
                        memset(buf,0,sizeof(buf));
                        sprintf(buf,"ALTER TABLE jails ADD COLUMN %s %s",sqldb_info[i].rowname,sqldb_info[i].rowtype);
                        ret=sql_stmt(buf);
                        if (ret==0)
                            debugmsg(0,"jailssql table updated: %s column\n",sqldb_info[i].rowname);
                        else
                            errmsg("update jailsql table error for %s column\n",sqldb_info[i].rowname);
                    }
            }
            goto closeexit;
        break;

	case (LIST):
            ret=select_stmt("select * from jails");
            goto closeexit;
	    break;
	case (GET):
	    if (!value) {
		errmsg("required arguments: --value\n");
		ret=1;
		goto closeexit;
	    }
	    memset(buf,0,sizeof(buf));
	    sprintf(buf,"select %s from jails",value);
            ret=select_valstmt(buf);
            goto closeexit;
        break;
	case (UPDATE):
            if (!param||!value||!jname) {
                errmsg("required arguments: --param, --value, --jname\n");
                ret=1;
                goto closeexit;
            }
            update_inventory(param,value);
            goto closeexit;
        break;
	case (DELETE):
            if (!param) {
                errmsg("required arguments: --param\n");
                ret=1;
                goto closeexit;
            }
            delete_inventory(param);
            goto closeexit;
        break;
    } //switch

closeexit:
    sqlite3_close(db);
    return ret;
}