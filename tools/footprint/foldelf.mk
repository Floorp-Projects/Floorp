foldelf: foldelf.cpp
	$(CXX) -O -o foldelf foldelf.cpp

clean:
	rm -f *.o test *.test foldelf *~

