#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

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
/* Structure to store information about   */
/* a column.                              */
/******************************************/

typedef struct STR_BINDING {
  SQLSMALLINT         cDisplaySize;           /* size to display  */
  char                *buffer;                /* display buffer   */
  SQLLEN              indPtr;                 /* size or null     */
  BOOL                fChar;                  /* character col?   */
  struct STR_BINDING  *sNext;                 /* linked list      */
} BINDING;



/******************************************/
/* Forward references                     */
/******************************************/

void HandleDiagnosticRecord (SQLHANDLE      hHandle,    
			     SQLSMALLINT    hType,  
			     RETCODE        RetCode);

void DisplayResults(HSTMT       hStmt,
                    SQLSMALLINT cCols);

void AllocateBindings(HSTMT         hStmt,
                      SQLSMALLINT   cCols,
                      BINDING**     ppBinding,
                      SQLSMALLINT*  pDisplay);

void SetConsole(DWORD   cDisplaySize,
                BOOL    fInvert);

/*****************************************/
/* Some constants                        */
/*****************************************/


#define DISPLAY_MAX 50          // Arbitrary limit on column width to display
#define DISPLAY_FORMAT_EXTRA 3  // Per column extra display bytes (| <data> )
#define DISPLAY_FORMAT      "%c %*.*s "
#define DISPLAY_FORMAT_C    "%c %-*.*s "
#define NULL_SIZE           6   // <NULL>
#define SQL_QUERY_SIZE      1000 // Max. Num characters for SQL Query passed in.

#define PIPE                '|'

int   gHeight = 80;       // Users screen height

int main(int argc, char **argv)
{
  SQLHENV     hEnv = NULL;
  SQLHDBC     hDbc = NULL;
  SQLHSTMT    hStmt = NULL;
  char*       pConnStr;
  char*       pQuery;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <ConnString> <Query>\n", argv[0]);
    return 1;
  }
  pConnStr = argv[1];
  pQuery = argv[2];

  // Allocate an environment

  if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) == SQL_ERROR)
    {
      fprintf(stderr, "Unable to allocate an environment handle\n");
      exit(-1);
    }

  // Register this as an application that expects 3.x behavior,
  // you must register something if you use AllocHandle

  TRYODBC(hEnv,
	  SQL_HANDLE_ENV,
	  SQLSetEnvAttr(hEnv,
			SQL_ATTR_ODBC_VERSION,
			(SQLPOINTER)SQL_OV_ODBC3,
			0));

  // Allocate a connection
  TRYODBC(hEnv,
	  SQL_HANDLE_ENV,
	  SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc));

  // Connect to the driver.  Use the connection string if supplied
  // on the input, otherwise let the driver manager prompt for input.

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

  TRYODBC(hDbc,
	  SQL_HANDLE_DBC,
	  SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt));

  RETCODE     RetCode;
  SQLSMALLINT sNumResults;

  // Execute the query
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
	    DisplayResults(hStmt,sNumResults);
	  } 
	else
	  {
	    SQLLEN cRowCount;

	    TRYODBC(hStmt,
		    SQL_HANDLE_STMT,
		    SQLRowCount(hStmt,&cRowCount));

	    if (cRowCount >= 0)
	      {
		printf("%d %s affected\n",
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

  printf("\nDisconnected.\n");

  return 0;

}

/************************************************************************
/* DisplayResults: display results of a select query
/*
/* Parameters:
/*      hStmt      ODBC statement handle
/*      cCols      Count of columns
/************************************************************************/

void DisplayResults(HSTMT       hStmt,
                    SQLSMALLINT cCols)
{
  BINDING         *pFirstBinding, *pThisBinding;          
  SQLSMALLINT     cDisplaySize;
  RETCODE         RetCode = SQL_SUCCESS;
  int             iCount = 0;
  long long numReceived = 0;

  // Allocate memory for each column 

  //AllocateBindings(hStmt, cCols, &pFirstBinding, &cDisplaySize);

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
#if 0

	// Display the data.   Ignore truncations

	for (pThisBinding = pFirstBinding;
	     pThisBinding;
	     pThisBinding = pThisBinding->sNext)
	  {
	    if (pThisBinding->indPtr != SQL_NULL_DATA)
	      {
		printf(pThisBinding->fChar ? DISPLAY_FORMAT_C:DISPLAY_FORMAT,
		       PIPE,
		       pThisBinding->cDisplaySize,
		       pThisBinding->cDisplaySize,
		       pThisBinding->buffer);
	      } 
	    else
	      {
		printf(DISPLAY_FORMAT_C,
		       PIPE,
		       pThisBinding->cDisplaySize,
		       pThisBinding->cDisplaySize,
		       "<NULL>");
	      }
	  }
	printf(" %c\n",PIPE);
#endif
	numReceived++;
      }
  } while (!fNoData);

 Exit:
  printf("numRecieved = %lld\n", numReceived);

  // Clean up the allocated buffers
  /*
  while (pFirstBinding)
    {
      pThisBinding = pFirstBinding->sNext;
      free(pFirstBinding->buffer);
      free(pFirstBinding);
      pFirstBinding = pThisBinding;
    }
  */
}

/************************************************************************
/* AllocateBindings:  Get column information and allocate bindings
/* for each column.  
/*
/* Parameters:
/*      hStmt      Statement handle
/*      cCols       Number of columns in the result set
/*      *lppBinding Binding pointer (returned)
/*      lpDisplay   Display size of one line
/************************************************************************/

void AllocateBindings(HSTMT         hStmt,
                      SQLSMALLINT   cCols,
                      BINDING       **ppBinding,
                      SQLSMALLINT   *pDisplay)
{
  SQLSMALLINT     iCol;
  BINDING         *pThisBinding, *pLastBinding = NULL;
  SQLLEN          cchDisplay, ssType;
  SQLSMALLINT     cchColumnNameLength;

  *pDisplay = 0;

  for (iCol = 1; iCol <= cCols; iCol++)
    {
      pThisBinding = (BINDING *)(malloc(sizeof(BINDING)));
      if (!(pThisBinding))
        {
	  fprintf(stderr, "Out of memory!\n");
	  exit(-100);
        }

      if (iCol == 1)
        {
	  *ppBinding = pThisBinding;
        }
      else
        {
	  pLastBinding->sNext = pThisBinding;
        }
      pLastBinding = pThisBinding;


      // Figure out the display length of the column (we will
      // bind to char since we are only displaying data, in general
      // you should bind to the appropriate C type if you are going
      // to manipulate data since it is much faster...)

      TRYODBC(hStmt,
	      SQL_HANDLE_STMT,
	      SQLColAttribute(hStmt,
			      iCol,
			      SQL_DESC_DISPLAY_SIZE,
			      NULL,
			      0,
			      NULL,
			      &cchDisplay));


      // Figure out if this is a character or numeric column; this is
      // used to determine if we want to display the data left- or right-
      // aligned.

      // SQL_DESC_CONCISE_TYPE maps to the 1.x SQL_COLUMN_TYPE. 
      // This is what you must use if you want to work
      // against a 2.x driver.

      TRYODBC(hStmt,
	      SQL_HANDLE_STMT,
	      SQLColAttribute(hStmt,
			      iCol,
			      SQL_DESC_CONCISE_TYPE,
			      NULL,
			      0,
			      NULL,
			      &ssType));


      pThisBinding->fChar = (ssType == SQL_CHAR ||
			     ssType == SQL_VARCHAR ||
			     ssType == SQL_LONGVARCHAR);

      pThisBinding->sNext = NULL;

      // Arbitrary limit on display size
      if (cchDisplay > DISPLAY_MAX)
	cchDisplay = DISPLAY_MAX;

      // Allocate a buffer big enough to hold the text representation
      // of the data.  Add one character for the null terminator

      pThisBinding->buffer = (char *)malloc((cchDisplay+1) * sizeof(char));

      if (!(pThisBinding->buffer))
        {
	  fprintf(stderr, "Out of memory!\n");
	  exit(-100);
        }

      // Map this buffer to the driver's buffer.   At Fetch time,
      // the driver will fill in this data.  Note that the size is 
      // count of bytes (for Unicode).  All ODBC functions that take
      // SQLPOINTER use count of bytes; all functions that take only
      // strings use count of characters.

      TRYODBC(hStmt,
	      SQL_HANDLE_STMT,
	      SQLBindCol(hStmt,
			 iCol,
			 SQL_C_TCHAR,
			 (SQLPOINTER) pThisBinding->buffer,
			 (cchDisplay + 1) * sizeof(char),
			 &pThisBinding->indPtr));


      // Now set the display size that we will use to display
      // the data.   Figure out the length of the column name

      TRYODBC(hStmt,
	      SQL_HANDLE_STMT,
	      SQLColAttribute(hStmt,
			      iCol,
			      SQL_DESC_NAME,
			      NULL,
			      0,
			      &cchColumnNameLength,
			      NULL));

      pThisBinding->cDisplaySize = (SQLSMALLINT)cchDisplay;
      if (pThisBinding->cDisplaySize < NULL_SIZE)
	pThisBinding->cDisplaySize = NULL_SIZE;

      *pDisplay += pThisBinding->cDisplaySize + DISPLAY_FORMAT_EXTRA;

    }

  return;

 Exit:

  exit(-1);

  return;
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
  char        wszMessage[1000];
  char        wszState[SQL_SQLSTATE_SIZE+1];


  if (RetCode == SQL_INVALID_HANDLE)
    {
      fprintf(stderr, "Invalid handle!\n");
      return;
    }

  while (SQLGetDiagRec(hType,
		       hHandle,
		       ++iRec,
		       wszState,
		       &iError,
		       wszMessage,
		       (SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)),
		       (SQLSMALLINT *)NULL) == SQL_SUCCESS)
    {
      // Hide data truncated..
      if (strncmp(wszState, "01004", 5))
        {
	  fprintf(stderr, "[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
        }
    }

}
