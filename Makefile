build: 
	clang -o main ./src/dt.c\
		-ggdb\
		-std=c99\
		# -fsanitize=address 
		# -pedantic
	# -Wall -Wextra 
	# last cheked Wed 13 22:22

loc:
	wc -lc ./src/{compiler/*.{h,c},vm/*.{h,c}}

run: build
	./main
