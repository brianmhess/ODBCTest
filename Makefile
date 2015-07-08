gen: gen.c
	gcc -o gen gen.c

odbcsql: odbcsql.c
	gcc -o odbcsql odbcsql.c -lodbc

cql: cql.c
	gcc -o cql cql.c -lcassandra
