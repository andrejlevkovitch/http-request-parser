exe:
	g++ test.cpp http_request_parser.cpp -Wall -Wextra -g -o tests

test: exe
	./tests
