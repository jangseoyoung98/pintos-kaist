cd ..
source ./activate
cd vm
make clean
make 
cd build

pintos -v -k -T 180 -m 8   --fs-disk=10 -p tests/vm/swap-file:swap-file -p ../../tests/vm/large.txt:large.txt --swap-disk=10 -- -q   -f run swap-file < /dev/null 2> tests/vm/swap-file.errors |tee tests/vm/swap-file.output
perl -I../.. ../../tests/vm/swap-file.ck tests/vm/swap-file tests/vm/swap-file.result

pintos -v -k -T 180 -m 10   --fs-disk=10 -p tests/vm/swap-anon:swap-anon --swap-disk=30 -- -q   -f run swap-anon < /dev/null 2> tests/vm/swap-anon.errors |tee tests/vm/swap-anon.output
perl -I../.. ../../tests/vm/swap-anon.ck tests/vm/swap-anon tests/vm/swap-anon.result

pintos -v -k -T 180 -m 10   --fs-disk=10 -p tests/vm/swap-iter:swap-iter -p ../../tests/vm/large.txt:large.txt --swap-disk=50 -- -q   -f run swap-iter < /dev/null 2> tests/vm/swap-iter.errors |tee tests/vm/swap-iter.output
perl -I../.. ../../tests/vm/swap-iter.ck tests/vm/swap-iter tests/vm/swap-iter.result

