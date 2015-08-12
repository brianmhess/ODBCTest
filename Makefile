compile: gen odbcsql cql otest1 otest2 otest3 otest4 ctest1

gen: gen.c
	gcc -o gen gen.c

odbcsql: odbcsql.c
	gcc -o odbcsql odbcsql.c -lodbc

cql: cql.c
	gcc -o cql cql.c -lcassandra

otest1: otest1.c
	gcc -o otest1 otest1.c -lodbc

otest2: otest2.c
	gcc -o otest2 otest2.c -lodbc

otest3: otest3.c
	gcc -o otest3 otest3.c -lodbc

otest4: otest4.c
	gcc -o otest4 otest4.c -lodbc

ctest1: ctest1.c
	gcc -o ctest1 ctest1.c -lcassandra
