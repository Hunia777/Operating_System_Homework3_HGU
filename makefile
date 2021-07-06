all : pfind worker

pfind : manager.c
	gcc manager.c -o pfind
worker : worker.c
	gcc worker.c -o worker
clean :
	rm pfind worker RESULTS.txt worker_to_manager manager_to_worker
