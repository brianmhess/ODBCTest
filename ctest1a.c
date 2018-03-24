#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

#include "cassandra.h"

#define TRYCASS(x)   {   CassError rc = x;			\
  if (rc != CASS_OK)						\
    {								\
      fprintf(stderr, "ERROR: %s\n", cass_error_desc(rc));	\
      exit(-1);							\
    }								\
  }

void print_error(CassFuture* future) {
  const char* message;
  size_t message_length;
  cass_future_error_message(future, &message, &message_length);
  fprintf(stderr, "Error: %.*s\n", (int)message_length, message);
}

CassCluster* create_cluster(const char* contact_points) {
  CassCluster* cluster = cass_cluster_new();
  cass_cluster_set_contact_points(cluster, contact_points);
  return cluster;
}

CassError connect_session(CassSession* session, const CassCluster* cluster) {
  CassError rc = CASS_OK;
  CassFuture* future = cass_session_connect(session, cluster);

  cass_future_wait(future);
  rc = cass_future_error_code(future);
  if (rc != CASS_OK) {
    print_error(future);
  }
  cass_future_free(future);

  return rc;
}

CassError execute_query(CassSession* session, const char* query) {
  CassError rc = CASS_OK;
  CassFuture* future = NULL;
  CassStatement* statement = cass_statement_new(query, 0);

  future = cass_session_execute(session, statement);
  cass_future_wait(future);

  rc = cass_future_error_code(future);
  if (rc != CASS_OK) {
    print_error(future);
  }  

  cass_future_free(future);
  cass_statement_free(statement);
  
  return rc;
}

char* get_column_as_string(const CassValue *value, char* buf, int bufsize) {
  buf[0] = '\0';
  const char *tbuf;
  char *tbuf2;
  size_t tbufsize;
  CassValueType vtype = cass_value_type(value);
  cass_int64_t val_int64;
  cass_int32_t val_int32;
  cass_float_t val_float;
  cass_double_t val_double;
  cass_bool_t val_bool;
  CassUuid val_uuid;
  CassInet val_inet;
  const cass_byte_t *val_byte;
  
  switch (vtype) {
  case CASS_VALUE_TYPE_ASCII:
  case CASS_VALUE_TYPE_TEXT:
  case CASS_VALUE_TYPE_VARCHAR:
    TRYCASS(cass_value_get_string(value, &tbuf, &tbufsize));
    if (tbufsize < bufsize - 1) {
      memcpy(buf, tbuf, tbufsize);
      buf[tbufsize] = '\0';
    }
    break;
  case CASS_VALUE_TYPE_BIGINT:
  case CASS_VALUE_TYPE_TIMESTAMP:
    TRYCASS(cass_value_get_int64(value, &val_int64));
    sprintf(buf, "%lld", (long long)val_int64);
    break;
  case CASS_VALUE_TYPE_INT:
    TRYCASS(cass_value_get_int32(value, &val_int32));
    sprintf(buf, "%d", (int)val_int32);
    break;
  case CASS_VALUE_TYPE_FLOAT:
    TRYCASS(cass_value_get_float(value, &val_float));
    sprintf(buf, "%f", (float)val_float);
    break;
  case CASS_VALUE_TYPE_DOUBLE:
    TRYCASS(cass_value_get_double(value, &val_double));
    sprintf(buf, "%f", (double)val_double);
    break;
  case CASS_VALUE_TYPE_BOOLEAN:
    TRYCASS(cass_value_get_bool(value, &val_bool));
    sprintf(buf, "%s", (val_bool == cass_true) ? "1" : "0");
    break;
  case CASS_VALUE_TYPE_UUID:
  case CASS_VALUE_TYPE_TIMEUUID:
    TRYCASS(cass_value_get_uuid(value, &val_uuid));
    cass_uuid_string(val_uuid, tbuf2);
    sprintf(buf, "%s", tbuf2);
    break;
  case CASS_VALUE_TYPE_INET:
    TRYCASS(cass_value_get_inet(value, &val_inet));
    cass_inet_string(val_inet, tbuf2);
    sprintf(buf, "%s", tbuf2);
    break;
  case CASS_VALUE_TYPE_BLOB:
  case CASS_VALUE_TYPE_VARINT:
  case CASS_VALUE_TYPE_LIST:
  case CASS_VALUE_TYPE_MAP:
  case CASS_VALUE_TYPE_SET:
    TRYCASS(cass_value_get_bytes(value, &val_byte, &tbufsize));
    if (tbufsize < bufsize - 1) {
      memcpy(buf, val_byte, tbufsize);
      buf[tbufsize] = '\0';
    }
    break;
  default:
    break;
  }
  return buf;
}

int main(int argc, char **argv) {
  char *contact_points;
  //char query[] = "SELECT xml_doc_id_nbr, structure_id_nbr, create_mint_cd, last_update_system_nm, last_update_tmstp, msg_major_version_nbr, msg_minor_version_nbr, msg_payload_img, opt_lock_token_nbr FROM ks.tbl WHERE xml_doc_id_nbr = ?";
  char query[] = "SELECT xml_doc_id_nbr, COUNT(*) FROM ks.tbl WHERE xml_doc_id_nbr = ? GROUP BY xml_doc_id_nbr";
  bool silent = true;

  if (argc != 5) {
    fprintf(stderr, "Usage: %s <contact_points> <pkey range> <ccol range> <rand seed>\n", argv[0]);
    return 1;
  }
  contact_points = argv[1];
  char *endptr;
  long long numkeys = strtoll(argv[2], &endptr, 10);
  long long rowsperkey = strtoll(argv[3], &endptr, 10);
  int seed = atoi(argv[4]);
  struct drand48_data lcg;
  srand48_r(seed, &lcg);


  CassCluster* cluster = create_cluster(contact_points);
  CassSession* session = cass_session_new();
  CassFuture* close_future = NULL;

  if (connect_session(session, cluster) != CASS_OK) {
    cass_cluster_free(cluster);
    cass_session_free(session);
    return -1;
  }

  CassError rc = CASS_OK;
  CassStatement* statement = NULL;
  CassFuture* future = NULL;
  long numResults = 0;
  const char *tbuf;
  size_t blen;
  char buf[1025];

  long long i;
  double rval, rval2;
  long long tval, tval2;
  cass_int64_t val;
  cass_int32_t val2;
  clock_t start = clock();
  
  for (i = 0; i < 100000; i++) {
    statement = cass_statement_new(query, 1);
    numResults = 0;
    drand48_r(&lcg, &rval);
    //drand48_r(&lcg, &rval2);
    tval = (long long)(rval * numkeys);
    //tval2 = (tval * 100) + (long long)(rval2 * rowsperkey);
    val = (cass_int64_t)tval;
    val2 = (cass_int32_t)tval2;
    cass_statement_bind_int64(statement, 0, val);
    //cass_statement_bind_int32(statement, 1, val2);
    future = cass_session_execute(session, statement);
    cass_future_wait(future);

    rc = cass_future_error_code(future);
    if (rc != CASS_OK) {
      print_error(future);
    } 
    else {
      const CassResult* result = cass_future_get_result(future);
      size_t nCols = cass_result_column_count(result);
      size_t i;
      CassIterator* iterator = cass_iterator_from_result(result);
      
      while (cass_true == cass_iterator_next(iterator)) {
	if (!silent) {
	  const CassRow* row = cass_iterator_get_row(iterator);
	  const CassValue* val = cass_row_get_column(row, 0);
	  printf("%s", get_column_as_string(val, buf, sizeof(buf)));
	  for (i = 1; i < nCols; i++) {
	    val = cass_row_get_column(row, i);
	    printf(",%s", get_column_as_string(val, buf, sizeof(buf)));
	  }
	  printf("\n");
	}
	numResults++;
      }

      cass_result_free(result);
      cass_iterator_free(iterator);
    }

    if (!silent)
      fprintf(stdout, "iteration %lld: numResults = %ld\n", i, numResults);

    cass_future_free(future);
    cass_statement_free(statement);
  }
  clock_t end = clock();
    fprintf(stderr, "Elapsed time = %f seconds\n", ((double) (end - start)) / CLOCKS_PER_SEC);


  close_future = cass_session_close(session);
  cass_future_wait(close_future);
  cass_future_free(close_future);

  cass_cluster_free(cluster);
  cass_session_free(session);

  return 0;
}
