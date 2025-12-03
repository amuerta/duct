build: 
	clang -o main ./src/main.c -Wall -Wno-c11-extensions -Wextra -ggdb -std=c99 -pedantic #\
		#-fsanitize=address 
	# last cheked Sun Aug 13 17:23

run: build
	./main
