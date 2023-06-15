cd ..
source ./activate
cd userprog
make clean
make
cd build
pintos -v -k -T 600 -m 20 -m 20   --fs-disk=10 -p tests/userprog/no-vm/multi-oom:multi-oom -- -q   -f run multi-oom < /dev/null 2> tests/userprog/no-vm/multi-oom.errors |tee tests/userprog/no-vm/multi-oom.output
Kernel command line: -q -f put multi-oom run multi-oom
perl -I../.. ../../tests/userprog/no-vm/multi-oom.ck tests/userprog/no-vm/multi-oom tests/userprog/no-vm/multi-oom.result