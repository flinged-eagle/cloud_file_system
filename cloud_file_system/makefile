all:.PHONY
.PHONY: ./root/upload main
./root/upload:upload.cpp
	g++ -std=c++11 $^ -o $@ -lboost_filesystem -lboost_system
main:main.cpp 
	g++ -std=c++11 $^ -o $@ -lpthread -lboost_filesystem -lboost_system 

clean:
	rm main ./root/upload
