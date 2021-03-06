#ifdef DATAINTERFACE
#include <sqlite3.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdint.h>
int ch;
void conclude_stat(sqlite3 *db,int target);
void create_log(sqlite3 *db,unsigned long np,uint32_t tot_s);
void update_data(sqlite3 *db,char *data,int target,uint32_t pkt_size,uint16_t port);
int data_choice(sqlite3 *db,char *ip,int target,uint16_t port);
void insert_data(sqlite3 *db,char *ip,int target,uint16_t port,uint32_t pkt_size,int type);
static int callback_printdata(void *data,int argc,char **argv,char **azColName);
void create_tbl(sqlite3 *db);
#endif
