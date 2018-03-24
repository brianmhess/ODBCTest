compile: gen odbcsql cql otest1 otest2 otest3 otest4 ctest1

gen: gen.c
	gcc -o gen gen.c

odbcsql: odbcsql.c
	gcc -Wl,-rpath=/usr/local/lib -o odbcsql odbcsql.c -lodbc

cql: cql.c
	gcc -I/usr/local/include/dse -L/usr/local/lib/x86_64-linux-gnu -Wl,-rpath=/usr/local/lib/x86_64-linux-gnu -o cql cql.c -ldse

otest1: otest1.c
	gcc -Wl,-rpath=/usr/local/lib -o otest1 otest1.c -lodbc

otest1a: otest1a.c
	gcc -Wl,-rpath=/usr/local/lib -o otest1a otest1a.c -lodbc

otest2: otest2.c
	gcc -o otest2 otest2.c -lodbc

otest3: otest3.c
	gcc -o otest3 otest3.c -lodbc

otest4: otest4.c
	gcc -o otest4 otest4.c -lodbc

ctest1: ctest1.c
	gcc -I/usr/local/include/dse -L/usr/local/lib/x86_64-linux-gnu -Wl,-rpath=/usr/local/lib/x86_64-linux-gnu -o ctest1 ctest1.c -ldse

ctest1a: ctest1a.c
	gcc -I/usr/local/include/dse -L/usr/local/lib/x86_64-linux-gnu -Wl,-rpath=/usr/local/lib/x86_64-linux-gnu -o ctest1a ctest1a.c -ldse
