all: GradeServer GradeClient

GradeServer: grade_server.c
	gcc -pthread -o GradeServer grade_server.c

GradeClient: grade_client.c
	gcc -pthread -o GradeClient grade_client.c