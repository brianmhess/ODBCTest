#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define SQL_QUERY "";

/*******************************************/
/* Macro to call ODBC functions and        */
/* report an error on failure.             */
/* Takes handle, handle type, and stmt     */
/*******************************************/

#define TRYODBC(h, ht, x)   {   RETCODE rc = x;		\
    if (rc != SQL_SUCCESS)				\
      {							\
	HandleDiagnosticRecord (h, ht, rc);		\
      }							\
    if (rc == SQL_ERROR)				\
      {							\
	fprintf(stderr, "Error in " #x "\n");	        \
	goto Exit;					\
      }							\
  }

/******************************************/
/* Forward references                     */
/******************************************/

void HandleDiagnosticRecord (SQLHANDLE      hHandle,    
			     SQLSMALLINT    hType,  
			     RETCODE        RetCode);

long long DisplayResults(HSTMT       hStmt,
			 SQLSMALLINT cCols,
			 bool        silent);

/*****************************************/
/* Some constants                        */
/*****************************************/
#define MAXCOLS (100)
#define BUFFERLEN (1024)


int main(int argc, char **argv)
{
  SQLHENV     hEnv = NULL;
  SQLHDBC     hDbc = NULL;
  SQLHSTMT    hStmt = NULL;
  char*       pConnStr;
  char        pQuery[1000];
  bool        silent = true;

  if (argc != 5) {
    fprintf(stderr, "Usage: %s <ConnString> <pkey range> <ccol range> <rand seed>\n", argv[0]);
    return 1;
  }
  pConnStr = argv[1];
  char *endptr;
  long long numkeys = strtoll(argv[2], &endptr, 10);
  long long rowsperkey = strtoll(argv[3], &endptr, 10);
  int seed = atoi(argv[4]);
  struct drand48_data lcg;
  srand48_r(seed, &lcg);

  // Allocate an environment
  if (!silent)
    fprintf(stderr, "Allocating Handle Enviroment\n");
  if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) == SQL_ERROR)
    {
      fprintf(stderr, "Unable to allocate an environment handle\n");
      exit(-1);
    }

  // Register this as an application that expects 3.x behavior,
  // you must register something if you use AllocHandle
  if (!silent)
    fprintf(stderr, "Setting to ODBC3\n");
  TRYODBC(hEnv,
	  SQL_HANDLE_ENV,
	  SQLSetEnvAttr(hEnv,
			SQL_ATTR_ODBC_VERSION,
			(SQLPOINTER)SQL_OV_ODBC3,
			0));

  // Allocate a connection
  if (!silent)
    fprintf(stderr, "Allocating Handle\n");
  TRYODBC(hEnv,
	  SQL_HANDLE_ENV,
	  SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc));

  // Connect to the driver.  Use the connection string if supplied
  // on the input, otherwise let the driver manager prompt for input.
  if (!silent)
    fprintf(stderr, "Connecting to driver\n");
  TRYODBC(hDbc,
	  SQL_HANDLE_DBC,
	  SQLDriverConnect(hDbc,
			   NULL,
			   pConnStr,
			   SQL_NTS,
			   NULL,
			   0,
			   NULL,
			   SQL_DRIVER_COMPLETE));

  fprintf(stderr, "Connected!\n");

  RETCODE     RetCode;
  SQLSMALLINT sNumResults;

  // Execute the query
  if (!silent)
    fprintf(stderr, "Executing query\n");
  long long i;
  double rval;
  long long numResults;
  for (i = 0; i < 100000; i++) {
    if (!silent)
      fprintf(stderr, "Allocating statement\n");
    TRYODBC(hDbc,
	    SQL_HANDLE_DBC,
	    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt));
    drand48_r(&lcg, &rval);
    sprintf(pQuery, "SELECT MAX(col1) FROM otest.test10 WHERE pkey = %lld", (long long)(rval * numkeys));
    RetCode = SQLExecDirect(hStmt, pQuery, SQL_NTS);

    switch(RetCode)
      {
      case SQL_SUCCESS_WITH_INFO:
	{
	  HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, RetCode);
	  // fall through
	}
      case SQL_SUCCESS:
	{
	  // If this is a row-returning query, display
	  // results
	  TRYODBC(hStmt,
		  SQL_HANDLE_STMT,
		  SQLNumResultCols(hStmt,&sNumResults));
	  
	  if (sNumResults > 0)
	    {
	      numResults = DisplayResults(hStmt,sNumResults, silent);
	      fprintf(stdout, "iteration %lld: numResults=%lld\n", i, numResults);
	    } 
	  else
	    {
	      SQLLEN cRowCount;
	      
	      TRYODBC(hStmt,
		      SQL_HANDLE_STMT,
		      SQLRowCount(hStmt,&cRowCount));
	      
	      if (cRowCount >= 0)
		{
		  printf("%d %s returned\n",
			 (int)cRowCount,
			 (cRowCount == 1) ? "row" : "rows");
		}
	    }
	  break;
	}
	
      case SQL_ERROR:
	{
	  HandleDiagnosticRecord(hStmt, SQL_HANDLE_STMT, RetCode);
	  break;
	}
	
      default:
	fprintf(stderr, "Unexpected return code %hd!\n", RetCode);
	
      }

    TRYODBC(hStmt,
	    SQL_HANDLE_STMT,
	    SQLFreeStmt(hStmt, SQL_CLOSE));
  }

 Exit:

  // Free ODBC handles and exit

  if (hStmt)
    {
      SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    }

  if (hDbc)
    {
      SQLDisconnect(hDbc);
      SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    }

  if (hEnv)
    {
      SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    }

  return 0;

}

/************************************************************************
/* DisplayResults: display results of a select query
/*
/* Parameters:
/*      hStmt      ODBC statement handle
/*      cCols      Count of columns
/************************************************************************/

long long DisplayResults(HSTMT       hStmt,
                    SQLSMALLINT cCols,
		    bool        silent)
{
  SQLSMALLINT     cDisplaySize;
  RETCODE         RetCode = SQL_SUCCESS;
  int             iCount = 0;
  long long numReceived = 0;

  // Allocate memory for each column 
  SQLCHAR buffer[MAXCOLS][BUFFERLEN];
  SQLLEN indPtr[MAXCOLS];
  int iCol;

  for (iCol = 0; iCol < cCols; iCol++) {
    TRYODBC(hStmt,
	    SQL_HANDLE_STMT,
	    SQLBindCol(hStmt,
		       iCol+1,
		       SQL_C_CHAR,
		       (SQLPOINTER) buffer[iCol],
		       (BUFFERLEN) * sizeof(char),
		       &indPtr[iCol]));    
  }

  // Fetch and display the data

  bool fNoData = false;

  do {
    // Fetch a row

    TRYODBC(hStmt, SQL_HANDLE_STMT, RetCode = SQLFetch(hStmt));

    if (RetCode == SQL_NO_DATA_FOUND)
      {
	fNoData = true;
      }
    else
      {
	if (!silent) {
	  // Display the data.   Ignore truncations	
	  printf("%s", buffer[0]);
	  for (iCol = 1; iCol < cCols; iCol++) {
	    printf(",%s", buffer[iCol]);
	  }
	  printf("\n");
	}

	numReceived++;
      }
  } while (!fNoData);

 Exit:
  //printf("numRecieved = %lld\n", numReceived);
  return numReceived;
}

/************************************************************************
/* HandleDiagnosticRecord : display error/warning information
/*
/* Parameters:
/*      hHandle     ODBC handle
/*      hType       Type of handle (HANDLE_STMT, HANDLE_ENV, HANDLE_DBC)
/*      RetCode     Return code of failing command
/************************************************************************/

void HandleDiagnosticRecord (SQLHANDLE      hHandle,    
                             SQLSMALLINT    hType,  
                             RETCODE        RetCode)
{
  SQLSMALLINT iRec = 0;
  SQLINTEGER  iError;
  char        message[1000];
  char        state[SQL_SQLSTATE_SIZE+1];


  if (RetCode == SQL_INVALID_HANDLE)
    {
      fprintf(stderr, "Invalid handle!\n");
      return;
    }

  while (SQLGetDiagRec(hType,
		       hHandle,
		       ++iRec,
		       state,
		       &iError,
		       message,
		       (SQLSMALLINT)(sizeof(message) / sizeof(WCHAR)),
		       (SQLSMALLINT *)NULL) == SQL_SUCCESS)
    {
      // Hide data truncated..
      if (strncmp(state, "01004", 5))
        {
	  fprintf(stderr, "[%5.5s] %s (%d)\n", state, message, iError);
        }
    }

}
