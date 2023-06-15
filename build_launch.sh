make clean
source ./activate
cd userprog
make clean
make
cd build
sleep 2
make check