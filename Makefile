Converter: clean File.o Proof.o Solver.o SBST_converter.o
	g++ -o $@ -std=c++11 -g File.o Proof.o Solver.o SBST_converter.o

File.o: File.cpp
	g++ -c -std=c++11 -g File.cpp

Proof.o: Proof.cpp
	g++ -c -std=c++11 -g Proof.cpp

Solve.o: Solver.cpp
	g++ -c -std=c++11 -g Solver.cpp

SBST_converter.o: SBST_converter.cpp
	g++ -c -std=c++11 -g SBST_converter.cpp

clean:
	rm -f *.o satTest tags
