##ODBC Test
Simple program to issue an ODBC query and return the results in CSV
format, or silently count them.

For comparison of various ODBC drivers.

Also includes a Cassandra C driver application for comparison.

## Data
The data is 1B rows of schema:
```CREATE TABLE test.test10(pkey BIGINT, ccol BIGINT, col1 BIGINT, col2 BIGINT, col3 BIGINT, col4 BIGINT, col5 BIGINT, col6 BIGINT, col7 BIGINT, col8 BIGINT, PRIMARY KEY ((pkey), ccol))```

For each PKEY, we generate 20 rows.  CCOL then goes from 0..19 and the rest of the columns are randomly chosen BIGINT values [0,1M).  The data is in 100 files of 10M rows each.

## Queries
### Case 1: Select all data for a pkey
Do this 100000 times and see the time.
```SELECT col1 FROM odbctest.test10 WHERE pkey = {something}```

### Case 2: Select MAX of all data for a pkey
Do this 100000 times and see the time.
```SELECT MAX(col1) FROM odbctest.test10 WHERE pkey = {something}```

### Case 3: Select all data for a ccol
Do this 100000 times and see the time
```SELECT col1 FROM odbctest.test10 WHERE ccol = {something}```

### Case 4: Select MAX of all data for a ccol
Do this 100000 times and see the time
```SELECT MAX(col1) FROM odbctest.test10 WHERE ccol = {something}```

### Case 5: Select MAX and group by pkey
Do this a few times and see the average
```SELECT pkey, MAX(col1) FROM mytable GROUP BY pkey```

### Case 6: Select MAX and group by ccol
Do this a few times and see the aberage
```SELECT ccol, MAX(col1) FROM mytable GROUP BY ccol```

## Additional queries
### A: Global Max
```SELECT MAX(col1) FROM mytable```

### B: Max of column with restriction on pkey (pkey > X)
```SELECT MAX(col1) FROM mytable WHERE pkey > {something}```

### C: Max of column with restriction on ccol (ccol > X)
```SELECT MAX(col1) FROM mytable WHERE ccol > {something}```

### D: Max of column with restriction on regular column (col2 > X)
```SELECT MAX(col1) FROM mytable WHERE col2 > {something}```

### E: Max of joined tables, joined on pkey and ccol
```SELECT MAX(a.col1 + b.col1) FROM mytable AS a JOIN mytable AS b ON (a.pkey = b.pkey AND a.ccol = b.ccol)```

### F: Max of joined tables, joined just on pkey
```SELECT MAX(a.col1 + b.col1) FROM mytable AS a JOIN mytable AS b ON (a.pkey = b.pkey)```

### G: Max of joined tables, joined just on ccol
```SELECT MAX(a.col1 + b.col1) FROM mytable AS a JOIN mytable AS b ON (a.ccol = b.ccol)```



