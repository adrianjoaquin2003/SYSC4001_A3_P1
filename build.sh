if [ ! -d "bin" ]; then
    mkdir bin
else
    rm bin/*
fi

g++ -std=c++17 -g -O0 -I . -o bin/interrupts_EP interrupts_student1_student2_EP.cpp
g++ -std=c++17 -g -O0 -I . -o bin/interrupts_RR interrupts_student1_student2_RR.cpp
g++ -std=c++17 -g -O0 -I . -o bin/interrupts_EP_RR interrupts_student1_student2_EP_RR.cpp
