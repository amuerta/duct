EXEC_NAME="ducti"

build: 
	clang -o $(EXEC_NAME) ./src/dt.c\
		-ggdb\
		-std=c99\
		# -fsanitize=address 
		# -pedantic
	# -Wall -Wextra 
	# last cheked this cmd: 2025 Wed Dec 13 22:22

loc:
	wc -lc ./src/{compiler/*.{h,c},vm/*.{h,c}}

run: build
	./$(EXEC_NAME)
